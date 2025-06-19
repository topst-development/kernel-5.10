// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT

/*
 * (C) COPYRIGHT 2023 Telechips Inc
 * (C) COPYRIGHT 2020-2023 ARM Limited or its affiliates. All rights reserved.
 */

/*
 * This is the Resource Group module which is responsible for communicating
 * with the Access Windows (or more precisely the kbase driver associated with
 * those AWs). The communication channel is still enabled when an AW is not
 * active on a partition.
 *
 * It is also responsible for controlling the Power, Reset and Clock state of
 * slices.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/workqueue.h>
#include <linux/of_platform.h>
#include <linux/iopoll.h>
#include <linux/atomic.h>

#include <arb_vm_protocol/mali_arb_vm_protocol.h>
#include <linux/mali_gpu_power.h>
#include "mali_arbiter.h"
#include "mali_gpu_resource_group.h"
#include "mali_gpu_assign.h"
#include "mali_gpu_ptm_msg.h"


/* Resource group version against which was implemented this module */
#define MALI_RESOURCE_GROUP_IMPLEMENTATION_VERSION 1
#if MALI_RESOURCE_GROUP_IMPLEMENTATION_VERSION != MALI_RESOURCE_GROUP_VERSION
#error "Unsupported resource group API version."
#endif

/* Arbiter version against which was implemented this module */
#define MALI_REQUIRED_ARBITER_VERSION 2
#if MALI_REQUIRED_ARBITER_VERSION != MALI_ARBITER_VERSION
#error "Unsupported Mali Arbiter API version."
#endif

/* GPU Power version against which was implemented this module */
#define MALI_REQUIRED_GPU_POWER_VERSION 1
#if MALI_REQUIRED_GPU_POWER_VERSION != MALI_GPU_POWER_VERSION
#error "Unsupported gpu power version."
#endif

#define EDGE_IRQ(x) (((x) & (IRQF_TRIGGER_RISING | IRQF_TRIGGER_FALLING)) != 0)

/* Device tree compatible ID of the Resource Group module */
#define RES_GROUP_DT_NAME "arm,mali-gpu-resource-group"
/* Device tree compatible ID of the parent node of Resource Group */
#define PWR_CTRL_DT_NAME "arm,mali-ptm"
/* Device tree compatible ID of the Partition Config module */
#define PTM_CONFIG_DT_NAME "arm,mali-gpu-partition-config"
/* Device tree compatible ID of the Partition Control module */
#define PTM_CONTROL_DT_NAME "arm,mali-gpu-partition-control"
/* Device tree compatible ID of the GPU interface */
#define PTM_INTERFACE_DT_NAME "arm,mali-ptm-interface"
/* Device tree attribute for partition config phandles */
#define PTM_CONFIG_DT_PHANDLES_NAME "partition-config"
/* Device tree attribute for fallback partition config phandles */
#define PTM_CONFIG_DT_FALLBACK_PHANDLES_NAME "partition_config"
/* Device tree attribute for partition control phandles */
#define PTM_CONTROL_DT_PHANDLES_NAME "partition-control"
/* Device tree attribute for fallback partition control phandles */
#define PTM_CONTROL_DT_FALLBACK_PHANDLES_NAME "partition_control"

/*
 * The maximum number of nodes to traverse looking for the power device
 */
#define MAX_PARENTS (3)

/* General defs of the Partition Manager HW */
#define PTM_DEVICE_ID 0x00
/* We check PTM_DEVICE_ID against this value to determine compatibility */
#define PTM_SUPPORTED_VER 0x9e550000
/* Required and masked fields: */
/* Bits [3:0] VERSION_STATUS - masked */
/* Bits [11:4] VERSION_MINOR - masked */
/* Bits [15:12] VERSION_MAJOR - masked */
/* Bits [19:16] PRODUCT_MAJOR - required */
/* Bits [23:20] ARCH_REV - masked */
/* Bits [27:24] ARCH_MINOR - required */
/* Bits [31:28] ARCH_MAJOR - required */
#define PTM_VER_MASK 0xFF0F0000
#define PTM_NUM_OF_SLICES 0x8
#define PTM_NUM_OF_WINDOWS 0x10

/* Slice specific defs of the Partition Manager HW  */
#define PTM_SLICE_FEATURES 0x0008
#define PTM_SLICE_CORES0 0x000C
#define PTM_SLICE_CORES1 0x0010
#define PTM_SLICE_FEATURES_TILER_MASK 0x1
#define PTM_SLICE_FEATURES_STRIDE_MASK 0xFF
#define PTM_SLICE_CORE_COUNT_MASK 0xFF
#define PTM_SLICE_CORE_BITS_PER_SLICE 0x8

/* Resource Group specific defs of the Partition Manager HW */
#define PTM_RESOURCE_SLICE_MASK 0x0014
#define PTM_RESOURCE_PARTITION_MASK 0x0018
#define PTM_RESOURCE_AW_MASK 0x001C

#define PTM_RESOURCE_IRQ_RAWSTAT 0x0080
#define PTM_RESOURCE_IRQ_CLEAR 0x0084
#define PTM_RESOURCE_IRQ_MASK 0x0088
#define PTM_RESOURCE_IRQ_STATUS 0x008C
#define PTM_RESOURCE_SLICE_POWER_STATE 0x0098
#define PTM_RESOURCE_SLICE_POWER_SET 0x009C
#define PTM_RESOURCE_SLICE_CLOCK_SET 0x00A4
#define PTM_RESOURCE_SLICE_RESET_STATE 0x00A8
#define PTM_RESOURCE_SLICE_RESET_SET 0x00AC
#define PTM_RESOURCE_AW0_MESSAGE 0x0100

#define MAX_SLICE_MASK 0xFF
#define MAX_PARTITION_MASK 0xF
#define MAX_AW_MASK 0xFFFF

#define SLICE_CLOCK_MASK_BIT 0x03
#define CLOCK_REG_BITS_PER_SLICE 0x2
#define CLOCK_SET_AUTOMATIC 0x2
#define CLOCK_SET_DISABLE 0x0

#define SLICE_RESET_MASK_BIT 0x01
#define RESET_REG_BITS_PER_SLICE 0x1
#define RESET_SET_RESET 0x1

#define MAX_AW_NUM 16

/* Register map layout values, used for determining hardware indices */
#define PTM_PT_OFFSET 0x20000
#define PTM_PT_STRIDE 0x20000

/* Timeouts used to poll registers */
#define REG_POLL_SLEEP_US 1
#define REG_POLL_TIMEOUT_US 40
#define REG_POLL_RESET_TIMEOUT_US 20000


/**
 * enum ptm_rg_state - Resource Group states
 * @HANDSHAKE_INIT:        The protocol handshake is being initialized.
 * @HANDSHAKE_IN_PROGRESS: The protocol handshake is in progress.
 * @HANDSHAKE_DONE:        The protocol handshake was done successfully.
 * @HANDSHAKE_FAILED:      The protocol handshake failed.
 *
 * Definition of the possible Resource Group's states
 */
enum ptm_rg_state {
	HANDSHAKE_INIT,
	HANDSHAKE_IN_PROGRESS,
	HANDSHAKE_DONE,
	HANDSHAKE_FAILED,
};

/**
 * struct ptm_rg_protocol - Resource Group protocol data
 * @state:              Current Resource Group state.
 * @version_in_use:     Agreed protocol versions used for the communication with
 *                      the Access Windows. Defined during protocol handshake.
 * @posthandshake_stop: keeps track if the VMs needs to receive a gpu_stop
 *                      message when handshaking is complete.
 * @mutex:              Mutex to protect against concurrent API calls accessing
 *                      the protocol data.
 */
struct ptm_rg_protocol {
	enum ptm_rg_state state;
	uint8_t version_in_use;
	atomic_t posthandshake_stop;
	struct mutex mutex;
};

/**
 * struct mali_vm_priv_data - VM private data for the Resource Group
 * @dev:    Pointer to the resource group device instance managing this VM.
 * @id:     VM index used by the resource group to identify each VM.
 * @arb_vm: Private Arbiter VM data used by the Arbiter to identify and work
 *          with each VM. This is populated by the Arbiter during the
 *          registration. See register_vm callback in the public interface.
 *
 * Stores the relevant data of each VM managed by this Resource Group.
 */
struct mali_vm_priv_data {
	struct device *dev;
	uint32_t id;
	struct mali_arb_priv_data *arb_vm;
};

/**
 * struct mali_gpu_ptm_rg - Resource Group module data.
 * @dev:             Pointer to the resource group device instance.
 * @mem:             Virtual memory address to the Resource Group device.
 * @res:             Resource data from the Resource Group device.
 * @mutex:           Mutex to protect against concurrent API calls accessing the
 *                   RG hardware registers.
 * @edge_irq:        If Resource Group uses edge based IRQ.
 * @rg_irq:          Resource Group module IRQ data.
 * @rg_irq.irq:      Resource Group IRQ ID.
 * @rg_irq.flags:    Resource Group IRQ flags.
 * @ptm_rg_ops:      Specific callback functions of the Resource Group.
 * @res_if:          Carries the resources interfaces of the group.
 * @ptm_rg_wq:       Pointer to the work queue used to queue the receive
 *                   message processing works.
 * @ptm_rg_req_work: The work element used to process a message received.
 * @msg_handler:     A ptm_msg_handler structure for managing messages coming
 *                   from any Access Window via the ISR.
 * @arb_data:        Public data of the Arbiter that interfaces with the
 *                   Resource Group.
 * @vm_ops:          Public VM calbacks implemented in the Resource Group
 * @ptm_rg_vm:       Resource Group VM private data used to identify each VM.
 * @prot_data:       Internal protocol data for correct communication with the
 *                   AWs.
 * @pwr_if:          Carries the power interface data.
 * @ptm_rg_irq_mask: PTM_RESOURCE_IRQ_MASK register value to store/restore in suspend/resume
 * @slices:          Derrived number of slices
 *
 * Stores the relevant data for the Resource Group driver.
 */

struct mali_gpu_ptm_rg {
	struct device *dev;
	void __iomem *mem;
	struct resource *res;
	struct mutex mutex;
	bool edge_irq;

	struct {
		int irq;
		int flags;
	} rg_irq;

	struct mali_ptm_rg_ops ptm_rg_ops;
	struct resource_interfaces res_if;
	struct workqueue_struct *ptm_rg_wq;
	struct work_struct ptm_rg_req_work;
	struct ptm_msg_handler msg_handler;

	struct mali_arb_data *arb_data;
	struct mali_vm_data vm_ops;
	struct mali_vm_priv_data *ptm_rg_vm[MAX_AW_NUM];
	struct ptm_rg_protocol prot_data[MAX_AW_NUM];

	struct power_interface pwr_if;
	u32 ptm_rg_irq_mask;
	uint32_t slices;

};

/**
 * check_ptm_version() - checks that this driver supports the hardware PTM
 * version.
 * @dev:		pointer to the device, for logging.
 * @ptm_device_id:	32bit integer read from PTM_DEVICE_ID register.
 *
 * Return:
 * * 0		- Hardware version supported.
 * * -ENODEV	- Hardware version not supported.
 * * -EIO	- Hardware version reported as zero.
 * * -EINVAL	- an argument was invalid.
 */
static int check_ptm_version(struct device *dev, u32 ptm_device_id)
{
	if (!dev) {
		pr_err("%s invalid argument.\n", __func__);
		return -EINVAL;
	}

	if (ptm_device_id == 0) {
		dev_err(dev, "Read zero from PTM ID register.\n");
		return -EIO;
	}

	/* Mask the status, minor, major versions and arch_rev. */
	ptm_device_id &= PTM_VER_MASK;

	if (ptm_device_id != (PTM_SUPPORTED_VER & PTM_VER_MASK)) {
		dev_err(dev, "Unsupported PTM version (DRV: 0x%X HW: 0x%X)\n",
			PTM_SUPPORTED_VER & PTM_VER_MASK, ptm_device_id);
		return -ENODEV;
	} else {
		return 0;
	}
}

/**
 * to_mali_gpu_ptm_rg() - gets pointer to mali_gpu_ptm_rg
 * @dev: Pointer to the resource group device.
 *
 * Gets pointer to mali_gpu_ptm_rg structure from device data
 *
 * Return: mali_gpu_ptm_rg* if successful, otherwise NULL
 */
static struct mali_gpu_ptm_rg *to_mali_gpu_ptm_rg(struct device *dev)
{
	struct mali_ptm_rg_ops *data;

	data = dev_get_drvdata(dev);
	if (likely(data))
		return container_of(data, struct mali_gpu_ptm_rg, ptm_rg_ops);

	dev_err(dev, "Resource Group interface missing.\n");

	return NULL;
}

/**
 * rg_get_slice_mask() - Get the slice mask.
 * @dev:  Pointer to the resource group module device.
 * @slice_mask: Pointer to variable to receive the mask.
 *
 * Gets a bitmap, of 1 bit per slice, of the slices assigned to this Resource
 * Group with non zero number of cores.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 * * -EIO	- failed with hardware fault.
 */
static int rg_get_slice_mask(struct device *dev, uint32_t *slice_mask)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	uint32_t slice_mask_reg;
	uint32_t slice_mask_val = 0;
	uint64_t slice_cores;
	int i = 0;

	if (!dev) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	slice_mask_reg = ioread32(ptm_rg->mem + PTM_RESOURCE_SLICE_MASK);
	if (slice_mask_reg > MAX_SLICE_MASK) {
		dev_err(dev, "Hardware reports out of range slice mask.\n");
		return -EIO;
	}

	slice_cores = ioread32(ptm_rg->mem + PTM_SLICE_CORES1);
	slice_cores <<= 32;
	slice_cores |= ioread32(ptm_rg->mem + PTM_SLICE_CORES0);

	while ((slice_mask_reg != 0) && (slice_cores != 0) && (i < PTM_NUM_OF_SLICES)) {
		if ((slice_mask_reg & 0x1) && (slice_cores & PTM_SLICE_CORE_COUNT_MASK)) {
			slice_mask_val |= (1 << i);
		}
		slice_cores >>= PTM_SLICE_CORE_BITS_PER_SLICE;
		slice_mask_reg >>= 1;
		i++;
	}

	*slice_mask = slice_mask_val;

	return 0;
}

/**
 * rg_get_partition_mask() - Get the partition mask.
 * @dev:  Pointer to the resource group module device.
 * @partition_mask: Pointer to variable to receive the mask.
 *
 * Gets a bitmap, of 1 bit per partition, of the partitions assigned to this
 * Resource Group.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 * * -EIO	- failed with hardware fault.
 */
static int rg_get_partition_mask(struct device *dev, uint32_t *partition_mask)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	uint32_t partition_mask_val;

	if (!dev) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	partition_mask_val = ioread32(ptm_rg->mem + PTM_RESOURCE_PARTITION_MASK);
	if (partition_mask_val > MAX_PARTITION_MASK) {
		dev_err(dev, "Hardware reports out of range partition mask.\n");
		return -EIO;
	}

	*partition_mask = partition_mask_val;

	return 0;
}

/**
 * rg_get_aw_mask() - Get the Access Window mask.
 * @dev:  Pointer to the resource group module device.
 * @aw_mask: Pointer to variable to receive the mask.
 *
 * Gets a bitmap, of 1 bit per Access Window, of the Access Windows assigned to
 * this Resource Group.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 * * -EIO	- failed with hardware fault.
 */
static int rg_get_aw_mask(struct device *dev, uint32_t *aw_mask)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	uint32_t aw_mask_val;

	if (!dev) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	/* No need for locking as these are read-only */
	aw_mask_val = ioread32(ptm_rg->mem + PTM_RESOURCE_AW_MASK);

	/* The only possible check in this case is to the range expected */
	if (aw_mask_val > MAX_AW_MASK) {
		dev_err(dev, "Hardware reports out of range aw mask.\n");
		return -EIO;
	}

	*aw_mask = aw_mask_val;

	return 0;
}

/**
 * rg_get_slices_core_mask() - Get a mask of the cores in all slices
 * @dev:              Pointer to the resource group device.
 * @core_mask:        Pointer to variable to receive a mask of the cores for all
 *                    slices in the PTM.
 * @core_mask_stride: Pointer to variable to receive the number of bits
 *                    allocated to each slice in the mask.
 *
 * Reads PTM_SLICE_CORES register which identifies the number of cores in
 * each slice and returns a mask with the cores allocated along with the number
 * of bits allocated to each slice in this mask.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 */
static int rg_get_slices_core_mask(struct device *dev, uint64_t *core_mask,
				   uint8_t *core_mask_stride)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	uint64_t slice_cores;
	uint32_t slice_features;
	uint32_t slice_count = 0;
	uint64_t slice_core_mask = 0;

	if (!dev || !core_mask || !core_mask_stride) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	/* Read the number of cores per slice */
	slice_cores = ioread32(ptm_rg->mem + PTM_SLICE_CORES1);
	slice_cores <<= 32;
	slice_cores |= ioread32(ptm_rg->mem + PTM_SLICE_CORES0);

	/* Read the core mask stride of the cores in the shader_present. */
	slice_features = ioread32(ptm_rg->mem + PTM_SLICE_FEATURES);
	*core_mask_stride = slice_features & PTM_SLICE_FEATURES_STRIDE_MASK;

	*core_mask = 0;
	/* Build the correct bitmap with the overall mask. */
	for (slice_count = 0; slice_count < PTM_NUM_OF_SLICES; slice_count++) {
		slice_core_mask = (1ULL << (slice_cores & PTM_SLICE_CORE_COUNT_MASK)) - 1;
		*core_mask |= slice_core_mask << (*core_mask_stride * slice_count);
		slice_cores >>= hweight32(PTM_SLICE_CORE_COUNT_MASK);
	}

	return 0;
}

/**
 * rg_poweron_slices() - Power on slices.
 * @dev:	Pointer to the resource group module device.
 * @slice_mask:	Mask of slices to be powered on.
 *
 * Power on the slices indicated by slice_mask parameter.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 * * -EIO	- failed with hardware fault.
 */
static int rg_poweron_slices(struct device *dev, uint32_t slice_mask)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	uint32_t slice_power_set_val;
	uint32_t slice_power_status_read;
	int err = 0;

	if (!dev) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	/* If the slice mask is zero, there is no work to do */
	if (slice_mask == 0)
		return 0;

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	/* Read both power and clock set registers */
	mutex_lock(&ptm_rg->mutex);
	slice_power_set_val = ioread32(ptm_rg->mem + PTM_RESOURCE_SLICE_POWER_SET);

	/*
	 * The mask represents just the slices to power on so we can't power off
	 * other slices that might be already on but not in the mask
	 */
	slice_power_set_val |= slice_mask;

	/* Write the result to both power and clock set registers */
	iowrite32(slice_power_set_val, ptm_rg->mem + PTM_RESOURCE_SLICE_POWER_SET);

	/*
	 * Read back the power status register to check that the new setting
	 * was applied
	 */
	err = readx_poll_timeout(ioread32, ptm_rg->mem + PTM_RESOURCE_SLICE_POWER_STATE,
				 slice_power_status_read,
				 ((slice_power_status_read & slice_mask) == slice_mask),
				 REG_POLL_SLEEP_US, REG_POLL_TIMEOUT_US);
	if (err) {
		dev_err(ptm_rg->dev, "Power state of slices not expected %08X != %08X\n",
			slice_power_status_read, slice_power_set_val);
		err = -EIO;
	}

	mutex_unlock(&ptm_rg->mutex);

	return err;
}

/**
 * rg_poweroff_slices() - Power off slices.
 * @dev:	Pointer to the resource group module device.
 * @slice_mask:	Mask of slices to be powered off.
 *
 * Power off the slices indicated by slice_mask parameter.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 * * -EIO	- failed with hardware fault.
 */
static int rg_poweroff_slices(struct device *dev, uint32_t slice_mask)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	uint32_t slice_power_set_val;
	uint32_t slice_power_status_read;
	int err = 0;

	if (!dev) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	/* If the slice mask is zero, there is no work to do */
	if (slice_mask == 0)
		return 0;

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	/* Read both power and clock set registers */
	mutex_lock(&ptm_rg->mutex);
	slice_power_set_val = ioread32(ptm_rg->mem + PTM_RESOURCE_SLICE_POWER_SET);

	/*
	 * The mask represents just the slices to power off so we can't power
	 * off other slices that might be on but not in the mask
	 */
	slice_power_set_val &= ~slice_mask;

	/* Write the result to power set register */
	iowrite32(slice_power_set_val, ptm_rg->mem + PTM_RESOURCE_SLICE_POWER_SET);

	/*
	 * Read back the power status register to check that the new setting
	 * was applied
	 */
	err = readx_poll_timeout(ioread32, ptm_rg->mem + PTM_RESOURCE_SLICE_POWER_STATE,
				 slice_power_status_read,
				 ((slice_power_status_read & slice_mask) == 0), REG_POLL_SLEEP_US,
				 REG_POLL_TIMEOUT_US);
	if (err) {
		dev_err(ptm_rg->dev, "Power state of slices not expected %08X != %08X\n",
			slice_power_status_read, slice_power_set_val);
		err = -EIO;
	}

	mutex_unlock(&ptm_rg->mutex);

	return err;
}

/**
 * rg_get_powered_slices_mask() - Get a mask of the powered on slices.
 * @dev:  Pointer to the resource group device.
 * @mask: Pointer to variable to receive the mask.
 *
 * Reads PTM_SLICE_POWER_STATE register and assigns to mask.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 */
static int rg_get_powered_slices_mask(struct device *dev, uint32_t *mask)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	int err = 0;

	if (!dev || !mask) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	mutex_lock(&ptm_rg->mutex);
	*mask = ioread32(ptm_rg->mem + PTM_RESOURCE_SLICE_POWER_STATE) & MAX_SLICE_MASK;
	mutex_unlock(&ptm_rg->mutex);

	return err;
}


/**
 * rg_enable_slices() - Release slices from reset.
 * @dev:	Pointer to the resource group module device.
 * @slice_mask:	Mask of slices to be enabled.
 *
 * Enable the clock and take out of reset the slices indicated by slice_mask
 * parameter.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 * * -EIO	- failed with hardware fault.
 */
static int rg_enable_slices(struct device *dev, uint32_t slice_mask)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	uint32_t slice_reset_set_val;
	uint32_t slice_clock_set_val;
	uint32_t slice_reset_status_read;
	uint32_t reset_bit_val;
	uint32_t clock_bit_val;
	int i, err = 0;

	if (!dev) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	/* If the slice mask is zero, there is no work to do */
	if (slice_mask == 0)
		return 0;

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	/* Read the current reset and clock set registers */
	mutex_lock(&ptm_rg->mutex);
	slice_reset_set_val = ioread32(ptm_rg->mem + PTM_RESOURCE_SLICE_RESET_STATE);
	slice_clock_set_val = ioread32(ptm_rg->mem + PTM_RESOURCE_SLICE_CLOCK_SET);

	/*
	 * The mask represents just the slices to release so we can't reset
	 * other slices that might be on released already but not in the mask
	 */
	for (i = 0; (i < PTM_NUM_OF_SLICES) && (slice_mask != 0); ++i) {
		if (slice_mask & 0x1) {
			reset_bit_val = SLICE_RESET_MASK_BIT;
			slice_reset_set_val &= ~(reset_bit_val << (i * RESET_REG_BITS_PER_SLICE));

			/* Just turn ON slices currently disabled
			 * (slice clock = 0).
			 */
			clock_bit_val = (slice_clock_set_val >> (i * CLOCK_REG_BITS_PER_SLICE)) &
					SLICE_CLOCK_MASK_BIT;

			if (clock_bit_val == CLOCK_SET_DISABLE) {
				clock_bit_val = CLOCK_SET_AUTOMATIC;
				slice_clock_set_val |= clock_bit_val
						       << (i * CLOCK_REG_BITS_PER_SLICE);
			}
		}
		slice_mask >>= 1;
	}

	/* Write the result to the reset and clock set registers */
	iowrite32(slice_reset_set_val, ptm_rg->mem + PTM_RESOURCE_SLICE_RESET_SET);
	err = readx_poll_timeout(ioread32, ptm_rg->mem + PTM_RESOURCE_SLICE_RESET_STATE,
				 slice_reset_status_read,
				 (slice_reset_status_read == slice_reset_set_val),
				 REG_POLL_SLEEP_US, REG_POLL_RESET_TIMEOUT_US);
	if (err) {
		dev_err(ptm_rg->dev, "Reset state of slices is in a wrong state %08X != %08X\n",
			slice_reset_status_read, slice_reset_set_val);
		err = -EIO;
		goto fail;
	}

	iowrite32(slice_clock_set_val, ptm_rg->mem + PTM_RESOURCE_SLICE_CLOCK_SET);

	err = 0;
fail:
	mutex_unlock(&ptm_rg->mutex);
	return err;
}

/**
 * rg_disable_slices() - Reset slices.
 * @dev:	Pointer to the resource group module device.
 * @slice_mask:	Mask of slices to be reset.
 *
 * Disable the clock and put into reset the slices indicated by slice_mask
 * parameter.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 * * -EIO	- failed with hardware fault.
 */
static int rg_disable_slices(struct device *dev, uint32_t slice_mask)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	uint32_t slice_reset_set_val;
	uint32_t slice_reset_status_read;
	uint32_t slice_clock_set_val;
	uint32_t reset_bit_val;
	uint32_t clock_bit_val;
	int i, err = 0;

	if (!dev) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	/* If the slice mask is zero, there is no work to do */
	if (slice_mask == 0)
		return 0;

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	/* Read the current clock and reset set registers. */
	mutex_lock(&ptm_rg->mutex);
	slice_clock_set_val = ioread32(ptm_rg->mem + PTM_RESOURCE_SLICE_CLOCK_SET);
	slice_reset_set_val = ioread32(ptm_rg->mem + PTM_RESOURCE_SLICE_RESET_STATE);

	/*
	 * The mask represents just the slices to reset so we can't release
	 * other slices that might be on reset already but not in the mask.
	 */
	for (i = 0; (i < PTM_NUM_OF_SLICES) && (slice_mask != 0); ++i) {
		/* Turn OFF slices with clock enabled (1) or automatic (2). */
		if (slice_mask & 0x1) {
			clock_bit_val = SLICE_CLOCK_MASK_BIT;
			slice_clock_set_val &= ~(clock_bit_val << (i * CLOCK_REG_BITS_PER_SLICE));
			reset_bit_val = RESET_SET_RESET;
			slice_reset_set_val |= reset_bit_val << (i * RESET_REG_BITS_PER_SLICE);
		}
		slice_mask >>= 1;
	}

	/* Write the result to the clock and reset set registers. */
	iowrite32(slice_reset_set_val, ptm_rg->mem + PTM_RESOURCE_SLICE_RESET_SET);
	err = readx_poll_timeout(ioread32, ptm_rg->mem + PTM_RESOURCE_SLICE_RESET_STATE,
				 slice_reset_status_read,
				 (slice_reset_status_read == slice_reset_set_val),
				 REG_POLL_SLEEP_US, REG_POLL_RESET_TIMEOUT_US);
	if (err) {
		dev_err(ptm_rg->dev, "Reset state of slices is in a wrong state %08X != %08X\n",
			slice_reset_status_read, slice_reset_set_val);
		err = -EIO;
		goto fail;
	}

	/* clock needs to be deassert after reset to allow a correct reset
	 * state transition
	 */
	iowrite32(slice_clock_set_val, ptm_rg->mem + PTM_RESOURCE_SLICE_CLOCK_SET);

	err = 0;
fail:
	mutex_unlock(&ptm_rg->mutex);
	return err;
}

/**
 * rg_get_enabled_slices_mask() - Get a mask of the enabled slices.
 * @dev:  Pointer to the resource group device.
 * @mask: Pointer to variable to receive the mask.
 *
 * Reads PTM_SLICE_RESET_STATE register and assigns to mask.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 */
static int rg_get_enabled_slices_mask(struct device *dev, uint32_t *mask)
{
	struct mali_gpu_ptm_rg *ptm_rg;

	if (!dev || !mask) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	mutex_lock(&ptm_rg->mutex);
	*mask = ~ioread32(ptm_rg->mem + PTM_RESOURCE_SLICE_RESET_STATE) & MAX_SLICE_MASK;
	mutex_unlock(&ptm_rg->mutex);

	return 0;
}

/**
 * rg_get_slice_tiler_type - Get the slice tiler type.
 *
 * @dev:      Pointer to the partition config module device.
 * @slice:    Slice index
 * @type:     Pointer to the result
 *
 * Gets the tiler type of the given slice.
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 */
static int rg_get_slice_tiler_type(struct device *dev, uint8_t slice,
				   enum mali_gpu_slice_tiler_type *type)
{
	uint32_t all_slices;
	struct mali_gpu_ptm_rg *ptm_rg;

	if (!dev || !type) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	if (slice >= PTM_NUM_OF_SLICES) {
		dev_err(dev, "Slice out of range.\n");
		return -EINVAL;
	}

	/* No need for locking as this is read-only */
	all_slices = ioread32(ptm_rg->mem + PTM_SLICE_FEATURES);
	*type = (all_slices >> (8 + slice)) & PTM_SLICE_FEATURES_TILER_MASK;

	return 0;
}

/**
 * res_group_set_state() - Sets the resource group protocol state.
 * @ptm_rg:    Resource group module internal data.
 * @new_state: What the new state should be.
 * @index:     Index of the state to change. Related the access window ID.
 *
 * Sets the resource group protocol state for a given Access Window.
 */
static void res_group_set_state(struct mali_gpu_ptm_rg *ptm_rg, enum ptm_rg_state new_state,
				uint32_t index)
{
	if (!ptm_rg || index > MAX_AW_NUM - 1)
		return;

	lockdep_assert_held(&ptm_rg->prot_data[index].mutex);
	ptm_rg->prot_data[index].state = new_state;
}

/**
 * res_group_get_state() - Gets the resource group protocol states.
 * @ptm_rg:     Resource group module internal data.
 * @index:      Index of the state to change. Related the access window ID.
 * @curr_state: Returns the current state for that AW ID.
 *
 * Gets the resource group protocol state for a given Access Window.
 *
 * Return: 0 if success, or a Linux error code
 */
static int res_group_get_state(struct mali_gpu_ptm_rg *ptm_rg, uint32_t index,
			       enum ptm_rg_state *curr_state)
{
	if (!ptm_rg || index > MAX_AW_NUM - 1)
		return -EINVAL;

	lockdep_assert_held(&ptm_rg->prot_data[index].mutex);
	*curr_state = ptm_rg->prot_data[index].state;

	return 0;
}

/**
 * send_message() - Write message to the sending buffer and try a sychronous
 *                  send if the handshake is done.
 * @ptm_rg:   Resource group module internal data.
 * @vm_id:    VM ID used by the resource group to know which AW the
 *            message is aimed to.
 * @message:  Pointer to 64-bit message to send.
 *
 * Writes the message to the sending buffer and try a synchronous register write
 * to send the message if the handshake is done.
 */
static void send_message(struct mali_gpu_ptm_rg *ptm_rg, uint32_t vm_id, uint64_t *message)
{
	struct ptm_msg_handler *msg_handler;
	enum ptm_rg_state rg_state = HANDSHAKE_INIT;
	int error;

	if (!ptm_rg || !message || vm_id > MAX_AW_NUM - 1)
		return;

	msg_handler = &ptm_rg->msg_handler;

	mutex_lock(&ptm_rg->prot_data[vm_id].mutex);
	/* Retrieve RG State. */
	res_group_get_state(ptm_rg, vm_id, &rg_state);
	switch (rg_state) {
	case HANDSHAKE_DONE:
		/* If Handshake is done, try a synchronous message send */
		error = ptm_msg_buff_write(&msg_handler->send_msgs, vm_id, *message);
		if (error > 0)
			dev_dbg(msg_handler->dev, "Overriding outgoing PTM message for AW%d\n",
				vm_id);
		else if (error < 0) {
			dev_err(msg_handler->dev, "RG message buffer write failed\n");
			goto cleanup_mutex;
		}
		ptm_msg_send(msg_handler, vm_id);
		break;
	case HANDSHAKE_IN_PROGRESS:
		dev_info(ptm_rg->dev, "HANDSHAKE_IN_PROGRESS, MSG to AW %d can't be sent now\n",
			 vm_id);
		break;
	case HANDSHAKE_FAILED:
		dev_info(ptm_rg->dev, "HANDSHAKE_FAILED, MSG to AW %d can't be sent\n", vm_id);
		break;
	default:
		dev_info(ptm_rg->dev, "HANDSHAKE_STATE with AW %d not recognised.\n", vm_id);
		break;
	}
cleanup_mutex:
	mutex_unlock(&ptm_rg->prot_data[vm_id].mutex);
}

/**
 * on_gpu_granted() - GPU has been granted by the arbiter
 * @vm:   Opaque pointer to the private VM data used by the Resource Group to
 *        identify each VM.
 * @freq: current frequency of the GPU to be passed to the VM,
 *        NO_FREQ value to be used in case of no support.
 *
 * Sends the ARB_VM_GPU_GRANTED to the AW.
 */
static void on_gpu_granted(struct mali_vm_priv_data *vm, uint32_t freq)
{
	uint64_t msg_payload = 0;
	struct mali_gpu_ptm_rg *ptm_rg;

	if (!vm || vm->id >= PTM_NUM_OF_WINDOWS || !vm->dev)
		return;

	ptm_rg = to_mali_gpu_ptm_rg(vm->dev);
	if (!ptm_rg)
		return;

	if (arb_vm_gpu_granted_build_msg(freq, &msg_payload))
		return;

	send_message(ptm_rg, vm->id, &msg_payload);
}

/**
 * on_gpu_stop() - Arbiter requires AW to stop using GPU
 * @vm: Opaque pointer to the private VM data used by the Resource Group to
 *      identify each VM.
 *
 * Sends the ARB_VM_GPU_STOP to the VM.
 */
static void on_gpu_stop(struct mali_vm_priv_data *vm)
{
	uint64_t msg_payload = 0;
	struct mali_gpu_ptm_rg *ptm_rg;
	enum ptm_rg_state rg_state = HANDSHAKE_INIT;

	if (!vm || vm->id >= PTM_NUM_OF_WINDOWS || !vm->dev)
		return;

	ptm_rg = to_mali_gpu_ptm_rg(vm->dev);
	if (!ptm_rg)
		return;

	/* Record any STOP being sent to active AW on restart,
	 * and send when handshake done, so message isn't
	 * overwritten by INIT.
	 */
	res_group_get_state(ptm_rg, vm->id, &rg_state);
	if (rg_state == HANDSHAKE_INIT) {
		/* Set stop request after handshake */
		atomic_set(&ptm_rg->prot_data[vm->id].posthandshake_stop, 1);
		return;
	}

	if (arb_vm_gpu_stop_build_msg(&msg_payload))
		return;

	send_message(ptm_rg, vm->id, &msg_payload);
}

/**
 * on_gpu_lost() - Sends message to AW that GPU has been removed
 * @vm: Opaque pointer to the private VM data used by the Resource Group to
 *      identify each VM.
 *
 * Sends the ARB_VM_GPU_LOST to the AW.
 */
static void on_gpu_lost(struct mali_vm_priv_data *vm)
{
	uint64_t msg_payload = 0;
	struct mali_gpu_ptm_rg *ptm_rg;

	if (!vm || vm->id >= PTM_NUM_OF_WINDOWS || !vm->dev)
		return;

	ptm_rg = to_mali_gpu_ptm_rg(vm->dev);
	if (!ptm_rg)
		return;

	if (arb_vm_gpu_lost_build_msg(&msg_payload))
		return;

	send_message(ptm_rg, vm->id, &msg_payload);
}

/**
 * send_arb_vm_init() - Sends arb_vm_init data to a given AW.
 * @msg_handler:   Message handler
 * @vm_id:         VM ID used by the resource group to know which AW the
 *                 message is aimed to.
 * @ack:           Acknoledge of the protocol handshake.
 * @version:       Version of the protocol.
 * @max_l2_slices: contains max l2 slices data calculated by arbiter to
 *                 send to AW.
 * @max_core_mask: contains max core mask data calculated by arbiter
 *                 to send to AW.
 *
 * Sends the ARB_VM_INIT to the AW.
 */
static void send_arb_vm_init(struct ptm_msg_handler *msg_handler, uint32_t vm_id, uint8_t ack,
			     uint8_t version, uint32_t max_l2_slices, uint32_t max_core_mask)
{
	uint64_t msg_payload = 0;
	int res = 0;

	if (vm_id >= PTM_NUM_OF_WINDOWS || !msg_handler)
		return;

	res = arb_vm_init_build_msg(max_l2_slices, max_core_mask, ack, version, &msg_payload);
	if (res)
		return;

	/* Write to the sending buffer. */
	res = ptm_msg_buff_write(&msg_handler->send_msgs, vm_id, msg_payload);
	if (res > 0)
		dev_dbg(msg_handler->dev, "Overriding outgoing PTM message for AW%d\n", vm_id);
	else if (res < 0) {
		dev_err(msg_handler->dev, "RG message buffer write failed\n");
		return;
	}
	/*
	 * Try a synchronous msg send directly without calling the method
	 * send_message otherwise it would prevent this to be sent when the
	 * protocol handshake is not done.
	 */
	ptm_msg_send(msg_handler, vm_id);
}

/**
 * res_group_respond_init() - Respond INIT message to conclude the protocol
 *                            handshake.
 * @ptm_rg:       Resource group module internal data.
 * @aw_id:        AW ID which the response is aimed at.
 * @recv_version: Protocol version received from the VM_ARB_INIT.
 *
 * Responds the INIT message received from the AW to conclude the protocol
 * handshake. This function should only be called when a VM_ARB_INIT is received
 * with ack = 0 and the handshake is not in progress.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static int res_group_respond_init(struct mali_gpu_ptm_rg *ptm_rg, uint32_t aw_id,
				  uint8_t recv_version)
{
	uint8_t sending_ack;
	uint8_t sending_version;
	uint32_t max_l2_slices = 0;
	uint32_t max_core_mask = 0;
	struct mali_arb_ops arb_ops;
	struct mali_arb_priv_data *arb_vm;
	int ret = 0;

	if (!ptm_rg || !ptm_rg->arb_data || aw_id > MAX_AW_NUM - 1)
		return -EINVAL;

	if (!ptm_rg->ptm_rg_vm[aw_id])
		return -EINVAL;

	arb_vm = ptm_rg->ptm_rg_vm[aw_id]->arb_vm;
	arb_ops = ptm_rg->arb_data->ops;

	/* Get maximum configuration from the Arbiter */
	if (arb_ops.get_max)
		arb_ops.get_max(arb_vm, &max_l2_slices, &max_core_mask);

	/* Init response should always be with ack = 1 */
	sending_ack = 1;

	/* If the received version is not supported the handshake fails */
	if (recv_version < MIN_SUPPORTED_VERSION) {
		/* Send higher version to make sure the other side also fails */
		sending_version = MIN_SUPPORTED_VERSION;
		res_group_set_state(ptm_rg, HANDSHAKE_FAILED, aw_id);
		dev_err(ptm_rg->dev, "Protocol handshake failed with AW %d\n", aw_id);
		ret = -EPERM;
	} else {
		/* Send minimum of the maximum versions */
		sending_version = (recv_version < CURRENT_VERSION) ? recv_version : CURRENT_VERSION;
		res_group_set_state(ptm_rg, HANDSHAKE_DONE, aw_id);
		/* Set protocol in use */
		ptm_rg->prot_data[aw_id].version_in_use = sending_version;
	}

	send_arb_vm_init(&ptm_rg->msg_handler, aw_id, sending_ack, sending_version, max_l2_slices,
			 max_core_mask);

	return ret;
}

/**
 * ptm_send_posthandshake_stop() - Sends STOP message if gpu_stop was received
 *                                 before the handshake was initialized
 * @ptm_rg: Resource group module internal data.
 * @aw_id:  AW ID which the stop message should be sent to.
 *
 * This function should only be called after the handshake is done.
 */
static void ptm_send_posthandshake_stop(struct mali_gpu_ptm_rg *ptm_rg, uint32_t aw_id)
{
	uint64_t msg_payload = 0;
	enum ptm_rg_state rg_state = HANDSHAKE_INIT;

	if (!ptm_rg || aw_id >= PTM_NUM_OF_WINDOWS)
		return;

	res_group_get_state(ptm_rg, aw_id, &rg_state);
	if (rg_state != HANDSHAKE_DONE)
		return;

	if (arb_vm_gpu_stop_build_msg(&msg_payload))
		return;

	mutex_unlock(&ptm_rg->prot_data[aw_id].mutex);
	send_message(ptm_rg, aw_id, &msg_payload);
	mutex_lock(&ptm_rg->prot_data[aw_id].mutex);
}

/**
 * res_group_validate_init() - Validate INIT message response to conclude the
 *                             protocol handshake.
 * @ptm_rg:       Resource group module internal data.
 * @aw_id:        AW ID which the response is coming from.
 * @recv_version: Protocol version received from the VM_ARB_INIT.
 *
 * Validates the INIT message received from the AW to conclude the protocol
 * handshake. This function should only be called when a VM_ARB_INIT is received
 * with ack = 1 and the handshake is in progress.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static int res_group_validate_init(struct mali_gpu_ptm_rg *ptm_rg, uint32_t aw_id,
				   uint8_t recv_version)
{
	if (!ptm_rg || aw_id > MAX_AW_NUM - 1)
		return -EINVAL;

	/* If the received version is not supported the handshake fails */
	if (recv_version > CURRENT_VERSION || recv_version < MIN_SUPPORTED_VERSION) {
		res_group_set_state(ptm_rg, HANDSHAKE_FAILED, aw_id);
		dev_err(ptm_rg->dev, "Protocol handshake failed with AW %d.\n", aw_id);
		return -EPERM;
	}

	/* If supported the received version should be used */
	res_group_set_state(ptm_rg, HANDSHAKE_DONE, aw_id);
	ptm_rg->prot_data[aw_id].version_in_use = recv_version;

	/* Send STOP to the AW if a gpu_stop was called before the handshake.
	 * Any messages flushed above should have made it to the HW registers
	 * so there is no problem in trying to send the STOP now.
	 */
	if (atomic_read(&ptm_rg->prot_data[aw_id].posthandshake_stop)) {
		ptm_send_posthandshake_stop(ptm_rg, aw_id);
		dev_dbg(ptm_rg->dev, "Sending STOP to VM active during restart\n");
		atomic_set(&ptm_rg->prot_data[aw_id].posthandshake_stop, 0);
	}
	return 0;
}

/**
 * res_group_initialize_init() - Initialize INIT process to start the
 *                               protocol handshake.
 * @ptm_rg: Resource group module internal data.
 * @aw_id:  AW ID which the response is coming from.
 *
 * Initializes the INIT message to start the protocol handshake. This function
 * should only be called when RG is probing and the given AW has not initialized
 * the handshake yet.
 */
static void res_group_initialize_init(struct mali_gpu_ptm_rg *ptm_rg, uint32_t aw_id)
{
	uint8_t sending_ack;
	uint8_t sending_version;
	uint32_t max_l2_slices = 0;
	uint32_t max_core_mask = 0;
	struct mali_arb_ops arb_ops;
	struct mali_arb_priv_data *arb_vm;

	if (!ptm_rg || !ptm_rg->arb_data || aw_id > MAX_AW_NUM - 1)
		return;

	if (!ptm_rg->ptm_rg_vm[aw_id])
		return;

	arb_vm = ptm_rg->ptm_rg_vm[aw_id]->arb_vm;
	arb_ops = ptm_rg->arb_data->ops;

	/* Get maximum configuration from the Arbiter */
	if (arb_ops.get_max)
		arb_ops.get_max(arb_vm, &max_l2_slices, &max_core_mask);

	/* Initial Init should always be with ack = 0 */
	sending_ack = 0;
	/* and version should be the current */
	sending_version = CURRENT_VERSION;

	/* Send message and change the RG status. */
	send_arb_vm_init(&ptm_rg->msg_handler, aw_id, sending_ack, sending_version, max_l2_slices,
			 max_core_mask);
	res_group_set_state(ptm_rg, HANDSHAKE_IN_PROGRESS, aw_id);
}

/**
 * process_protocol_handshake() - Process the protocol handshake
 * @ptm_rg:   Resource group module internal data.
 * @rg_state: Resource group state related to the given AW ID.
 * @aw_id:    AW ID which the INIT message was received from.
 * @message:  Received INIT message.
 *
 * Process an INIT message received and respond to it according to the given
 * current state for the given AW ID.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static int process_protocol_handshake(struct mali_gpu_ptm_rg *ptm_rg, enum ptm_rg_state rg_state,
				      uint32_t aw_id, uint64_t *message)
{
	uint8_t recv_ack;
	uint8_t recv_version;
	uint8_t message_id;
	struct device *rg_dev;
	int ret = 0;

	if (!ptm_rg || !message)
		return -EINVAL;
	rg_dev = ptm_rg->dev;

	/* Decode the rest of the message. */
	if (get_msg_id(message, &message_id) || get_msg_protocol_version(message, &recv_version) ||
	    get_msg_init_ack(message, &recv_ack))
		return -EPERM;

	if (message_id != VM_ARB_INIT)
		return -EPERM;

	switch (rg_state) {
	case HANDSHAKE_INIT:
	case HANDSHAKE_DONE:
		if (recv_ack == 0) {
			/* If the other side initialized the handshake */
			ret = res_group_respond_init(ptm_rg, aw_id, recv_version);
		} else {
			dev_dbg(rg_dev, "Unexpected message from AW %d\n", aw_id);
			ret = -EPERM;
		}
		break;
	case HANDSHAKE_IN_PROGRESS:
		if (recv_ack == 1) {
			/* If the other side has responded the init. */
			ret = res_group_validate_init(ptm_rg, aw_id, recv_version);
		} else {
			dev_dbg(rg_dev, "Protocol handshake with AW %d started by RG.\n", aw_id);
			ret = -EPERM;
		}
		break;
	case HANDSHAKE_FAILED:
		dev_dbg(rg_dev, "%s: Unexpected message from AW %d. Handshake failed.\n", __func__,
			aw_id);
		ret = -EPERM;
		break;
	default:
		break;
	}

	return ret;
}

/**
 * process_msg_from_vm() - Processes a message received from the VM.
 * @ptm_rg:  Resource group module internal data.
 * @payload: The message payload received from the VM.
 * @aw_id:   Access window ID
 *
 * Processes a message received from the VM. according to the definitions
 * of the VM-Arbiter protocol.
 *
 * Return: 0 if successful, otherwise a negative error code.
 */
static int process_msg_from_vm(struct mali_gpu_ptm_rg *ptm_rg, uint64_t *payload, uint32_t aw_id)
{
	struct mali_arb_ops arb_ops;
	struct device *rg_dev;
	uint8_t message_id;
	int res;
	enum ptm_rg_state rg_state = HANDSHAKE_INIT;
	struct mali_arb_priv_data *arb_vm;
	uint8_t gpu_request = 0;
	bool stop_restart = false;

	if (!ptm_rg || !ptm_rg->arb_data || aw_id > MAX_AW_NUM - 1)
		return -EINVAL;

	rg_dev = ptm_rg->dev;
	/* Get the arbiter callbacks. */
	arb_ops = ptm_rg->arb_data->ops;
	/* Get information about the VM which the message was received from. */
	arb_vm = ptm_rg->ptm_rg_vm[aw_id]->arb_vm;

	if (!rg_dev || !arb_vm)
		return -EINVAL;

	res = get_msg_id(payload, &message_id);
	if (res)
		return res;

	mutex_lock(&ptm_rg->prot_data[aw_id].mutex);
	/*retrieve RG State for the given AW */
	res_group_get_state(ptm_rg, aw_id, &rg_state);

	/* Deal with the protocol handshake if VM_ARB_INIT */
	if (message_id == VM_ARB_INIT) {
		if (get_msg_init_request(payload, &gpu_request)) {
			res = -EPERM;
			goto cleanup_mutex;
		}
		/* If gpu_request = 1, there is no reason to send stop */
		if (gpu_request && atomic_read(&ptm_rg->prot_data[aw_id].posthandshake_stop)) {
			stop_restart = true;
			atomic_set(&ptm_rg->prot_data[aw_id].posthandshake_stop, 0);
		}

		/* First process the handshake on INIT message */
		res = process_protocol_handshake(ptm_rg, rg_state, aw_id, payload);
		if (res)
			goto cleanup_mutex;
	} else if (rg_state != HANDSHAKE_DONE) {
		dev_dbg(rg_dev, "Handshake not completed, MSG from AW %d ignored\n", aw_id);
		res = -EPERM;
		goto cleanup_mutex;
	}

	/* After dealing with RG state, mutex can be unlocked */
	mutex_unlock(&ptm_rg->prot_data[aw_id].mutex);

	/* Interface with the Arbiter according to the message received */
	switch (message_id) {
	case VM_ARB_INIT: {
		/* If gpu_request = 1, gpu_request must be called for that VM.*/
		if (gpu_request && stop_restart) {
			if (!arb_ops.gpu_stopped)
				return -EPERM;
			arb_ops.gpu_stopped(arb_vm, true);
		} else if (gpu_request) {
			if (!arb_ops.gpu_request)
				return -EPERM;
			arb_ops.gpu_request(arb_vm);
		}
		break;
	}
	case VM_ARB_GPU_IDLE:
		if (!arb_ops.gpu_idle)
			return -EPERM;
		arb_ops.gpu_idle(arb_vm);
		break;
	case VM_ARB_GPU_ACTIVE:
		/*
		 * TO-DO: Priority is not implemented yet. It will be
		 * implemented according to AUTOSW-358 tickets. Thus, the
		 * priority will need to be passed in the callback at this
		 * point. The same applies to GPU_REQUEST and GPU_STOPPED.
		 */
		if (!arb_ops.gpu_active)
			return -EPERM;
		arb_ops.gpu_active(arb_vm);
		break;
	case VM_ARB_GPU_REQUEST:
		/*
		 * TO-DO: Priority passing to the arbiter (see comment above).
		 */
		if (!arb_ops.gpu_request)
			return -EPERM;
		arb_ops.gpu_request(arb_vm);
		break;
	case VM_ARB_GPU_STOPPED: {
		bool req_again;
		uint8_t priority;

		res = get_msg_priority(payload, &priority);
		if (res)
			return res;
		/*
		 * TO-DO: Replacing req_again for priority passing to the
		 * arbiter (see comment above).
		 */
		req_again = (priority != 0) ? true : false;
		/*
		 * While priority mechanism is not implemented we will use the
		 * priority field to pass the req_again param for the
		 * GPU_STOPPED message.
		 */
		if (!arb_ops.gpu_stopped)
			return -EPERM;
		arb_ops.gpu_stopped(arb_vm, req_again);
		break;
	}
	default:
		/* Return error if the message ID is not known. */
		return -EINVAL;
	}

	return 0;

cleanup_mutex:
	mutex_unlock(&ptm_rg->prot_data[aw_id].mutex);
	return res;
}

/**
 * recv_msg_worker() - Receive worker function for use with
 *                     ptm_msg_process_msgs()
 * @params: Parameters required to process the message
 *
 * Reads the appropriate message from the receive messages and passes it
 * to process_msg_from_vm() for processing
 */
static void recv_msg_worker(struct msg_worker_params *params)
{
	uint64_t message;
	struct mali_gpu_ptm_rg *ptm_rg;
	int error;

	if (WARN_ON(!params) || WARN_ON(!params->data))
		return;

	ptm_rg = (struct mali_gpu_ptm_rg *)params->data;

	/* Read the incoming message, overwriting if necessary */
	error = ptm_msg_buff_read(&ptm_rg->msg_handler.recv_msgs, params->aw_id, &message);
	if (error) {
		dev_info(ptm_rg->dev, "Failed to get message (%d)\n", error);
		return;
	}

	/*
	 * The protocol processes the received message
	 * and call the correct arbiter callback
	 */
	process_msg_from_vm(ptm_rg, &message, params->aw_id);
}

/**
 * res_group_process_message() - Worker thread for processing received messages
 * @data: Work contained within the device data.
 *
 * Process the receiving commands and schedule the required work to handle them
 */
static void res_group_process_message(struct work_struct *data)
{
	struct mali_gpu_ptm_rg *ptm_rg;

	if (WARN_ON(!data))
		return;

	ptm_rg = container_of(data, struct mali_gpu_ptm_rg, ptm_rg_req_work);

	/* Check if the Arbiter public data is available */
	if (!ptm_rg->arb_data) {
		dev_err(ptm_rg->dev, "Arbiter callback handlers not found.\n");
		return;
	}

	/* Process pending incoming messages */
	ptm_msg_process_msgs(&ptm_rg->msg_handler.recv_msgs, ptm_rg, recv_msg_worker);

	if (ptm_rg->msg_handler.recv_msgs.mask)
		queue_work(ptm_rg->ptm_rg_wq, &ptm_rg->ptm_rg_req_work);
}

/**
 * res_group_read_incoming_msg() - Reads the last message received and buffers
 *                                 it.
 * @ptm_rg:  Resource Group device structure.
 * @aw_id:   Access Window ID from which the message should be read.
 * @payload: returns the message payload from the message registers.
 *
 * This function reads incoming message registers for a given VM ID and buffers
 * the message so that it can be processed later. Note that this function does
 * not check if the message is valid or not, it just buffers what is in the
 * registers. It is the caller responsability to make sure that a new message
 * has arrived.
 */
static void res_group_read_incoming_msg(struct mali_gpu_ptm_rg *ptm_rg, uint32_t aw_id,
					uint64_t *payload)
{
	int error;

	if (!payload || !ptm_rg || aw_id > MAX_AW_NUM)
		return;

	/* Read the incoming messages */
	ptm_msg_read(&ptm_rg->msg_handler, aw_id, payload);

	error = ptm_msg_buff_write(&ptm_rg->msg_handler.recv_msgs, aw_id, *payload);
	if (error > 0)
		dev_dbg(ptm_rg->dev, "Overriding incoming PTM message\n");
	else if (error < 0) {
		dev_err(ptm_rg->dev, "RG message buffer write failed\n");
		return;
	}

	iowrite32(0x1 << aw_id, ptm_rg->mem + PTM_RESOURCE_IRQ_CLEAR);
}

/**
 * res_group_irq_handler() - Handles the IRQ raised for the resource group.
 * @irq:	The IRQ ID according to the DT.
 * @data:	The data of the device where the IRQ was raised.
 *
 * Return: 0 if success, or an error code.
 */
static irqreturn_t res_group_irq_handler(int irq, void *data)
{
	struct mali_gpu_ptm_rg *ptm_rg = data;
	uint32_t irq_status;
	uint64_t payload = 0;
	uint32_t aw_id = 0;
	irqreturn_t ret = IRQ_HANDLED;

	/* Check if the IRQ in known. */
	if (irq == ptm_rg->rg_irq.irq)
		irq_status = ioread32(ptm_rg->mem + PTM_RESOURCE_IRQ_STATUS) & MAX_AW_MASK;
	else {
		dev_err(ptm_rg->dev, "Unknown irq %d\n", irq);
		ret = IRQ_NONE;
		goto cleanup;
	}

	if (irq_status == 0) {
		ret = IRQ_NONE;
		goto cleanup;
	}

	aw_id = 0;
	do {
		if (irq_status & 0x1) {
			/* Read the incoming messages. */
			res_group_read_incoming_msg(ptm_rg, aw_id, &payload);
		}
		/* Check next AW. */
		++aw_id;
		irq_status >>= 1;
	} while (irq_status);

	/* Schedule the work to process the message received. */
	queue_work(ptm_rg->ptm_rg_wq, &ptm_rg->ptm_rg_req_work);

cleanup:
	/* If this is an edge-based interrupt, clear and restore the mask
	 * to create an edge in case there's another interrupt pending
	 */
	if (ptm_rg->edge_irq) {
		iowrite32(0, ptm_rg->mem + PTM_RESOURCE_IRQ_MASK);
		iowrite32(MAX_AW_MASK, ptm_rg->mem + PTM_RESOURCE_IRQ_MASK);
	}

	return ptm_rg->edge_irq ? IRQ_HANDLED : ret;
}

/**
 * initialize_communication() - Initialize communication with the assigned AWs.
 * @ptm_rg: pointer to resource group data
 *
 * Return: 0 on success, or error code
 */
static int initialize_communication(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct device *rg_dev;
	uint32_t aw_mask;
	uint32_t irq_status;
	uint8_t aw_id = 0;
	uint64_t payload = 0;
	int err = 0;
	int i;

	if (!ptm_rg)
		return -EINVAL;

	rg_dev = ptm_rg->dev;

	/* Initialize all the protocol data mutexes */
	for (i = 0; i < MAX_AW_NUM; ++i)
		mutex_init(&ptm_rg->prot_data[i].mutex);
	/* Initialize the msg_handler */
	ptm_msg_handler_init(&ptm_rg->msg_handler, ptm_rg->dev,
			     ptm_rg->mem + PTM_RESOURCE_AW0_MESSAGE, MAX_AW_NUM,
			     ARB_TO_VM_BUFF_SIZE, VM_TO_ARB_BUFF_SIZE);

	/* Get the assigned AWs to this RG */
	err = rg_get_aw_mask(rg_dev, &aw_mask);
	if (err) {
		dev_err(rg_dev, "Cannot get AW mask.\n");
		return -EPERM;
	}

	/* Read the IRQ raw status to verify any message received before */
	irq_status = ioread32(ptm_rg->mem + PTM_RESOURCE_IRQ_RAWSTAT) & MAX_AW_MASK;

	while (aw_mask && irq_status) {
		if ((aw_mask & 0x1) && (irq_status & 0x1))
			res_group_read_incoming_msg(ptm_rg, aw_id, &payload);
		aw_mask >>= 1;
		irq_status >>= 1;
		aw_id++;
	}

	/* Synchronously process any new incoming messages */
	res_group_process_message(&ptm_rg->ptm_rg_req_work);

	return 0;
}

/**
 * initialize_handshake() - Initialize protocol handshake with the assigned AWs.
 * @ptm_rg: pointer to resource group data
 *
 * Return: 0 on success, or error code
 */
static int initialize_handshake(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct device *rg_dev;
	uint32_t aw_mask;
	enum ptm_rg_state rg_state = HANDSHAKE_INIT;
	uint8_t aw_id = 0;
	int err = 0;

	if (!ptm_rg)
		return -EINVAL;

	rg_dev = ptm_rg->dev;

	/* Get the assigned AWs to this RG */
	err = rg_get_aw_mask(rg_dev, &aw_mask);
	if (err) {
		dev_err(rg_dev, "Cannot get AW mask.\n");
		return -EPERM;
	}

	/*
	 * Initialize Protocol handshake with the assigned AWs that have not
	 * initialized it yet.
	 */
	while (aw_mask) {
		mutex_lock(&ptm_rg->prot_data[aw_id].mutex);
		/*retrieve RG State for the given AW */
		res_group_get_state(ptm_rg, aw_id, &rg_state);
		if (aw_mask & 0x1 && rg_state == HANDSHAKE_INIT)
			res_group_initialize_init(ptm_rg, aw_id);
		mutex_unlock(&ptm_rg->prot_data[aw_id].mutex);
		aw_mask >>= 1;
		aw_id++;
	}

	return 0;
}

/**
 * check_ptm_configuration() - Check the Partition Manager Configuration
 * @ptm_rg: pointer to resource group data
 *
 * Check that the Partition Manager is supported and if the Resource group
 * has any partition assigned.
 *
 * Return: 0 if success, or a Linux error code.
 */
static int check_ptm_configuration(struct mali_gpu_ptm_rg *ptm_rg)
{
	int err;
	uint32_t partition_mask = 0;
	int retry = 0;

	if (!ptm_rg)
		return -ENODEV;

	/* Check the partition manager ID. */
	err = check_ptm_version(ptm_rg->dev, ioread32(ptm_rg->mem + PTM_DEVICE_ID));
	if (err)
		return err;

	/* Check if the Resource Group has any partitions assigned to it. */
	/* In Telechips GPU virtualization environment, guest OS waits for host OS to be initialized. */
	while(partition_mask == 0 && retry < 1000){
		err = rg_get_partition_mask(ptm_rg->dev, &partition_mask);
		retry++;
		mdelay(1);
	}

	if (err) {
		dev_err(ptm_rg->dev, "Failed to get the partition mask\n");
		return err;
	} else if (partition_mask == 0) {
		dev_err(ptm_rg->dev, "No partitions are assigned\n");
		return -ENODEV;
	}

	return 0;
}

/**
 * rg_release_resources() - Release resources allocated to this Resource Group.
 * @ptm_rg: pointer to the resource group internal data
 */
static void rg_release_resources(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct repartition_interface *repart_if;
	struct vm_assign_interface *vm_assign_if;
	struct device *dev;
	uint32_t i;

	if (!ptm_rg)
		return;

	dev = ptm_rg->dev;

	/* Release the module drivers, and partition data */
	for (i = 0; i < ptm_rg->res_if.num_if; i++) {
		repart_if = ptm_rg->res_if.repartition[i];
		if (repart_if) {
			if (repart_if->dev) {
				module_put(repart_if->dev->driver->owner);
				put_device(repart_if->dev);
			}
			devm_kfree(dev, repart_if);
		}
		vm_assign_if = ptm_rg->res_if.vm_assign[i];
		if (vm_assign_if) {
			if (vm_assign_if->dev) {
				module_put(vm_assign_if->dev->driver->owner);
				put_device(vm_assign_if->dev);
			}
			devm_kfree(dev, vm_assign_if);
		}
	}
	ptm_rg->res_if.num_if = 0;
	if (ptm_rg->res_if.repartition)
		devm_kfree(dev, ptm_rg->res_if.repartition);
	if (ptm_rg->res_if.vm_assign)
		devm_kfree(dev, ptm_rg->res_if.vm_assign);
}

/**
 * alloc_next_if() - Returns index for an interface.
 * @ptm_rg: pointer to the resource group internal data
 * @pt_index: Partition index
 *
 * Returns the index of an existing interface if one has already exists,
 * otherwise allocate a new one.
 *
 * Return: interface index if successful, or -ENOMEM insufficient free memory.
 */
static int alloc_next_if(struct mali_gpu_ptm_rg *ptm_rg, u32 pt_index)
{
	struct resource_interfaces *res_if;
	uint32_t i;
	struct vm_assign_interface *vm_assign_if = NULL;
	struct repartition_interface *repart_if = NULL;

	res_if = &ptm_rg->res_if;
	for (i = 0; i < res_if->num_if; ++i) {
		vm_assign_if = res_if->vm_assign[i];
		if (!vm_assign_if)
			break;

		if (pt_index == res_if->vm_assign[i]->if_id) {
			repart_if = res_if->repartition[i];
			break;
		}
	}
	if (!vm_assign_if) {
		res_if->vm_assign[i] =
			devm_kzalloc(ptm_rg->dev, sizeof(struct vm_assign_interface), GFP_KERNEL);
		if (!res_if->vm_assign[i])
			return -ENOMEM;

		res_if->vm_assign[i]->if_id = pt_index;
	}
	if (!repart_if) {
		res_if->repartition[i] =
			devm_kzalloc(ptm_rg->dev, sizeof(struct repartition_interface), GFP_KERNEL);
		if (!res_if->repartition[i])
			return -ENOMEM;

		res_if->repartition[i]->if_id = pt_index;
	}
	return i;
}

/**
 * set_partition_control() - Update partition control/assign interface.
 * @ptm_rg: pointer to the resource group internal data
 * @pt_index: Partition index
 * @node: Device tree node for partition control
 *
 * Allocates a new assign interface for a particular index and
 * initializes it based on the given DT node.
 *
 * Return: 0 if successful,
 *         -EINVAL arguments are invalid,
 *         -ENODEV partition control node not found in DT,
 *         -EPROBE_DEFER partition control driver not loaded.
 */
static int set_partition_control(struct mali_gpu_ptm_rg *ptm_rg, u32 pt_index,
				 struct device_node *node)
{
	struct vm_assign_interface *vm_assign_if;
	struct resource_interfaces *res_if;
	struct platform_device *pt_pdev;
	struct device *dev;
	int idx;

	if (!ptm_rg || !node)
		return -EINVAL;

	dev = ptm_rg->dev;

	/* Get the platform device of the partition config/control */
	pt_pdev = of_find_device_by_node(node);
	if (!pt_pdev) {
		dev_err(dev, "Platform device for partition control %s not found.\n", node->name);
		return -ENODEV;
	}
	/* Get the module driver */
	if (!pt_pdev->dev.driver || !try_module_get(pt_pdev->dev.driver->owner)) {
		dev_err(dev, "Partition control driver not available.\n");
		put_device(&pt_pdev->dev);
		return -EPROBE_DEFER;
	}
	idx = alloc_next_if(ptm_rg, pt_index);
	if (idx < 0) {
		put_device(&pt_pdev->dev);
		return idx;
	}

	res_if = &ptm_rg->res_if;
	vm_assign_if = res_if->vm_assign[idx];
	vm_assign_if->dev = &pt_pdev->dev;
	vm_assign_if->ops = dev_get_drvdata(&pt_pdev->dev);
	return 0;
}

/**
 * set_partition_config() - Update partition config/repartition interface.
 * @ptm_rg: pointer to the resource group internal data
 * @pt_index: Partition index
 * @node: Device tree node for partition config
 *
 * Allocates a new repartition interface for a particular index and
 * initializes it based on the given DT node.
 *
 * Return: 0 if successful,
 *         -EINVAL arguments are invalid,
 *         -ENODEV partition config node not found in DT,
 *         -EPROBE_DEFER partition config driver not loaded.
 */
static int set_partition_config(struct mali_gpu_ptm_rg *ptm_rg, u32 pt_index,
				struct device_node *node)
{
	struct repartition_interface *repart_if;
	struct resource_interfaces *res_if;
	struct platform_device *pt_pdev;
	struct device *dev;
	int idx;

	if (!ptm_rg || !node)
		return -EINVAL;

	dev = ptm_rg->dev;

	pt_pdev = of_find_device_by_node(node);
	if (!pt_pdev) {
		dev_err(dev, "Platform device for partition config %s not found.\n", node->name);
		return -ENODEV;
	}
	/* Get the module driver */
	if (!pt_pdev->dev.driver || !try_module_get(pt_pdev->dev.driver->owner)) {
		dev_err(dev, "Partition config driver not available.\n");
		put_device(&pt_pdev->dev);
		return -EPROBE_DEFER;
	}
	idx = alloc_next_if(ptm_rg, pt_index);
	if (idx < 0) {
		put_device(&pt_pdev->dev);
		return idx;
	}

	res_if = &ptm_rg->res_if;
	repart_if = res_if->repartition[idx];
	repart_if->dev = &pt_pdev->dev;
	repart_if->ops = dev_get_drvdata(&pt_pdev->dev);
	return 0;
}

/**
 * allocate_interfaces() - Allocates interface structures.
 * @ptm_rg: pointer to the resource group internal data
 *
 * Return: 0 if successful,
 *         -ENOMEM Not enough memory for allocation.
 */
static int allocate_interfaces(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct resource_interfaces *res_if;
	struct device *dev;

	dev = ptm_rg->dev;
	res_if = &ptm_rg->res_if;

	/* Allocate the interfaces arrays */
	res_if->repartition = devm_kzalloc(
		dev, sizeof(struct repartition_interface *) * res_if->num_if, GFP_KERNEL);
	if (!res_if->repartition)
		return -ENOMEM;

	res_if->vm_assign = devm_kzalloc(dev, sizeof(struct vm_assign_interface *) * res_if->num_if,
					 GFP_KERNEL);
	if (!res_if->vm_assign) {
		devm_kfree(dev, res_if->repartition);
		return -ENOMEM;
	}

	return 0;
}

/**
 * get_resources_from_phandles() - Extract partition config from DT phandles.
 * @ptm_rg: pointer to the resource group internal data
 *
 * Uses phandles in the DT to retrieve the partition configuration and
 * control resources.
 *
 * Return: 0 if successful,
 *         -EINVAL arguments are invalid or not enough phandles in DT,
 *         -ENODEV partition control/config node not found in DT,
 *         -ENOMEM Not enough memory for allocation.
 *         -EPROBE_DEFER partition control/config driver not loaded.
 */
static int get_resources_from_phandles(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct resource_interfaces *res_if;
	struct device *dev;
	int i;
	struct device_node *node;
	int err;

	if (!ptm_rg)
		return -EINVAL;

	dev = ptm_rg->dev;

	res_if = &ptm_rg->res_if;

	/* Added by Telechips for default settings */
	of_property_read_u32(dev->of_node, "res_grps_index", &res_if->rg_index);

	i = of_count_phandle_with_args(dev->of_node, PTM_CONTROL_DT_PHANDLES_NAME, NULL);
	if (i < 0)
		i = of_count_phandle_with_args(dev->of_node, PTM_CONTROL_DT_FALLBACK_PHANDLES_NAME,
					       NULL);

	if (i <= 0) {
		res_if->num_if = 0;
		return 0;
	}
	res_if->num_if = i;

	i = of_count_phandle_with_args(dev->of_node, PTM_CONFIG_DT_PHANDLES_NAME, NULL);
	if (i < 0)
		i = of_count_phandle_with_args(dev->of_node, PTM_CONFIG_DT_FALLBACK_PHANDLES_NAME,
					       NULL);

	if (i != (int)res_if->num_if) {
		dev_err(dev, "Partition config/control DT phandles do not match\n");
		return -EINVAL;
	}
	err = allocate_interfaces(ptm_rg);
	if (err) {
		dev_err(dev, "allocate_interfaces %d\n", err);
		return err;
	}

	for (i = 0; i < (int)res_if->num_if; i++) {
		node = of_parse_phandle(dev->of_node, PTM_CONTROL_DT_PHANDLES_NAME, i);
		if (!node)
			node = of_parse_phandle(dev->of_node, PTM_CONTROL_DT_FALLBACK_PHANDLES_NAME,
						i);

		if (!node)
			break;

		err = set_partition_control(ptm_rg, i, node);
		if (err) {
			dev_err(dev, "set_partition_control failed with error %d\n", err);
			return err;
		}

		node = of_parse_phandle(dev->of_node, PTM_CONFIG_DT_PHANDLES_NAME, i);
		if (!node)
			node = of_parse_phandle(dev->of_node, PTM_CONFIG_DT_FALLBACK_PHANDLES_NAME,
						i);

		if (!node) {
			dev_err(dev, "No node\n");
			return -EINVAL;
		}

		err = set_partition_config(ptm_rg, i, node);
		if (err) {
			dev_err(dev, "set_partition_config failed with error %d\n", err);
			return err;
		}
	}
	if (i == 0)
		dev_dbg(dev, "No DT phandles for partitions\n");
	else
		dev_dbg(dev, "Added %d partitions using DT phandles\n", i);

	return 0;
}

/**
 * get_resources_by_addr() - Extracts partition config using relative address.
 * @ptm_rg: pointer to the resource group internal data
 *
 * Uses the relative address of the partition configuration and control
 * nodes from the device tree to retrieve the resources.
 *
 * Return: 0 if success,
 *         -EINVAL arguments are invalid or not enough phandles in DT,
 *         -ENODEV partition control/config node not found in DT,
 *         -EPROBE_DEFER partition control/config driver not loaded.
 */
static int get_resources_by_addr(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct resource_interfaces *res_if;
	struct device *dev;
	struct device_node *gpu_if_axi;
	struct device_node *resource_group_node;
	struct device_node *child;
	u32 relative_addr = 0;
	u32 pt_index;
	u32 pt_num_config, pt_num_control;
	u32 pt_mask = 0;
	u32 pt_count = 0;
	int err;

	if (!ptm_rg)
		return -EINVAL;

	dev = ptm_rg->dev;
	res_if = &ptm_rg->res_if;

	/*
	 * Just the partition resources are necessary as they are the only
	 * resources which the driver modules interface directly with the
	 * Arbiter.
	 */
	err = rg_get_partition_mask(dev, &pt_mask);
	if (err) {
		dev_err(dev, "Partition mask get failed.\n");
		return err;
	}
	pt_count = hweight32(pt_mask);
	if (!pt_count) {
		dev_info(dev, "Partition mask returned zero.\n");
		return 0;
	}
	res_if->num_if = pt_count;
	err = allocate_interfaces(ptm_rg);
	if (err) {
		dev_err(dev, "Failed to allocate interfaces.\n");
		res_if->num_if = 0;
		return err;
	}

	resource_group_node = dev->of_node;
	if (!resource_group_node->parent ||
	    !of_device_is_compatible(resource_group_node->parent, PTM_INTERFACE_DT_NAME)) {
		dev_err(dev, "GPU Interface AXI bus not found.\n");
		return -ENODEV;
	}
	gpu_if_axi = dev->parent->of_node;

	pt_num_config = 0;
	pt_num_control = 0;
	/* Iterate over the bus to find Partition Config and Control nodes. */
	for_each_child_of_node(gpu_if_axi, child) {
		/* Check if the child is either Config or Control. */
		if (!of_device_is_compatible(child, PTM_CONFIG_DT_NAME) &&
		    !of_device_is_compatible(child, PTM_CONTROL_DT_NAME))
			continue;

		/* Get the relative addr of the node. */
		if (of_property_read_u32_index(child, "reg", 1, &relative_addr)) {
			dev_err(dev, "Failed to get relative addr for partition.\n");
			return -ENODEV;
		}

		/* Get the index of the partition. */
		pt_index = (relative_addr - PTM_PT_OFFSET) / PTM_PT_STRIDE;
		if (pt_index >= MALI_PTM_PARTITION_COUNT) {
			dev_err(dev, "Partition Config relative addr out of range.\n");
			return -ENODEV;
		}
		/* Check if the partition is part of this resource group. */
		if (!(pt_mask & (0x1 << pt_index)))
			continue;

		/* Update resources information */
		if (((relative_addr - PTM_PT_OFFSET) % PTM_PT_STRIDE) == 0) {
			err = set_partition_config(ptm_rg, pt_index, child);
			pt_num_config++;
		} else {
			err = set_partition_control(ptm_rg, pt_index, child);
			pt_num_control++;
		}

		if (err) {
			dev_err(dev, "Failed to set partition control/config.\n");
			return err;
		}
	}
	if (pt_num_config != pt_num_control) {
		dev_err(dev, "partition config and control not matching.\n");
		return -ENODEV;
	}
	res_if->num_if = pt_num_config;
	return 0;
}

/**
 * rg_get_resources() - Reads the device tree for resource configuration.
 * @ptm_rg: pointer to the resource group internal data
 *
 * Gets information about the resources allocated to this resource group from
 * the device tree.
 *
 * Return: 0 if success, or a Linux error code.
 */
static int rg_get_resources(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct resource_interfaces *res_if;
	struct device *dev;
	struct repartition_interface *repart_if;
	struct vm_assign_interface *vm_assign_if;
	uint32_t i;
	int err;

	if (!ptm_rg || !ptm_rg->dev->of_node)
		return -EINVAL;

	dev = ptm_rg->dev;
	res_if = &ptm_rg->res_if;

	err = get_resources_from_phandles(ptm_rg);
	if (err) {
		dev_err(dev, "Failed to get_resources_from_phandles.\n");
		goto pt_modules_put;
	}

	/* In case we have no phandles in device tree, fall back
	 * to using DT register addresses.
	 */
	if (res_if->num_if == 0) {
		err = get_resources_by_addr(ptm_rg);
		if (err)
			goto pt_modules_put;
	}

	/* Check if the resources information is consistent. */
	for (i = 0; i < res_if->num_if; ++i) {
		vm_assign_if = res_if->vm_assign[i];
		repart_if = res_if->repartition[i];

		if (!vm_assign_if || !repart_if) {
			dev_err(dev, "Repartition or VM Assign IF not found.\n");
			err = -ENODEV;
			goto pt_modules_put;
		}

		if ((repart_if->dev && !vm_assign_if->dev) ||
		    (!repart_if->dev && vm_assign_if->dev)) {
			dev_err(dev, "Repartition and VM Assign IFs must exist.\n");
			err = -ENODEV;
			goto pt_modules_put;
		}
	}

	return 0;

pt_modules_put:
	rg_release_resources(ptm_rg);

	return err;
}

/* Telechips : No need to set power in guest */
#if !defined(CONFIG_TCC_MALI_VZ) || \
	(defined(CONFIG_TCC_MALI_VZ) && defined(CONFIG_TCC807X_CA55_SUB))
/**
 * get_pwr_if() - Get the power control interfaces
 * @ptm_rg:   pointer to the resource group internal data
 *
 * Checks if the main Mali PTM node exists and updates the internal ptm_rg data
 * with the Power Control interfaces data.
 *
 * Return: 0 if success, or a Linux error code.
 */
static int get_pwr_if(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct device *dev;
	struct device_node *pwr_node = NULL;
	struct device *bus_dev;
	struct platform_device *pwr_pdev;
	struct mali_gpu_power_data *pwr_data;
	int err = 0;
	int max_parents = MAX_PARENTS;

	if (WARN_ON(!ptm_rg) || WARN_ON(!ptm_rg->dev))
		return -EINVAL;

	dev = ptm_rg->dev;
	/* Find mali-gpu-power device in the hierarchy.
	 * Depending on the configuration this could be parent
	 * or grandparent. Virtualized DT will be flat and not
	 * use this hierarchy. In this case a single power node
	 * is assumed.
	 */
	for (bus_dev = dev; max_parents > 0; max_parents--) {
		if (of_device_is_compatible(bus_dev->of_node, PWR_CTRL_DT_NAME)) {
			pwr_node = bus_dev->of_node;
			break;
		}
		bus_dev = bus_dev->parent;
		if (bus_dev == NULL) {
			/* Assume partial DT with single power node */
			pwr_node = of_find_compatible_node(NULL, NULL, PWR_CTRL_DT_NAME);
			break;
		}
	}

	if (!pwr_node) {
		dev_err(dev, "Power Control platform device not found\n");
		return -ENODEV;
	}

	pwr_pdev = of_find_device_by_node(pwr_node);
	if (!pwr_pdev) {
		dev_err(dev, "Failed to find GPU Power device\n");
		return -ENODEV;
	}

	if (!pwr_pdev->dev.driver || !pwr_pdev->dev.driver->owner ||
	    !try_module_get(pwr_pdev->dev.driver->owner)) {
		dev_err(dev, "GPU Power module not available\n");
		err = -EPROBE_DEFER;
		goto exit;
	}
	ptm_rg->pwr_if.dev = &pwr_pdev->dev;

	pwr_data = platform_get_drvdata(pwr_pdev);
	if (!pwr_data) {
		dev_err(dev, "GPU Power device not ready\n");
		err = -EPROBE_DEFER;
		goto cleanup_module;
	}
	ptm_rg->pwr_if.ops = &pwr_data->ops;

	return 0;

cleanup_module:
	module_put(ptm_rg->pwr_if.dev->driver->owner);
	ptm_rg->pwr_if.dev = NULL;
exit:
	put_device(&pwr_pdev->dev);
	return err;
}
#endif

/**
 * rg_arbiter_info_init() - Create an arbiter instance
 * @ptm_rg:  pointer to the resource group internal data
 *
 * This function attempt to create an arbiter instance and retrieve all
 * the required information that the arbiter needs
 *
 * Return: 0 if success, or a Linux error code.
 */
static int rg_arbiter_info_init(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct device *dev;
	int err = 0;

	if (!ptm_rg)
		return -EINVAL;

	dev = ptm_rg->dev;

	/* Get the Power Control platform device to provide to the Arbiter. */
#if !defined(CONFIG_TCC_MALI_VZ) || \
	(defined(CONFIG_TCC_MALI_VZ) && defined(CONFIG_TCC807X_CA55_SUB))
	if (get_pwr_if(ptm_rg))
		dev_dbg(dev, "Main PTM node not present\n");
#endif		

	/*
	 * Get the resources related to this resource group so that this
	 * information can be provided to the Arbiter.
	 */
	err = rg_get_resources(ptm_rg);
	if (err) {
		dev_err(dev, "Resources get failed\n");
		goto release_pwr;
	}

	/* create arbiter */
	err = arbiter_create(dev, ptm_rg->pwr_if, ptm_rg->res_if, &ptm_rg->arb_data);
	if (err) {
		dev_err(dev, "Failed to create arbiter\n");
		goto release_res;
	}

	return 0;

release_res:
	rg_release_resources(ptm_rg);
release_pwr:
	if (ptm_rg->pwr_if.dev) {
		module_put(ptm_rg->pwr_if.dev->driver->owner);
		put_device(ptm_rg->pwr_if.dev);
		ptm_rg->pwr_if.dev = NULL;
	}
	return err;
}

/**
 * rg_init_vm_priv_data() - Initializes the VM private data
 * @ptm_rg: pointer to the resource group internal data
 * @vm_id:  VM ID which the vm_priv_data should be initialized for
 *
 * Allocates and sets the dev and vm_id of the private VM data for a given VM ID
 *
 * Return: 0 if success, or a Linux error code.
 */
static int rg_init_vm_priv_data(struct mali_gpu_ptm_rg *ptm_rg, uint32_t vm_id)
{
	if (!ptm_rg || !ptm_rg->dev || vm_id > MAX_AW_NUM - 1)
		return -EINVAL;

	ptm_rg->ptm_rg_vm[vm_id] =
		devm_kzalloc(ptm_rg->dev, sizeof(struct mali_vm_priv_data), GFP_KERNEL);
	if (!ptm_rg->ptm_rg_vm[vm_id])
		goto fail_vm_alloc;

	ptm_rg->ptm_rg_vm[vm_id]->dev = ptm_rg->dev;
	ptm_rg->ptm_rg_vm[vm_id]->id = vm_id;

	return 0;

fail_vm_alloc:
	devm_kfree(ptm_rg->dev, ptm_rg->ptm_rg_vm[vm_id]);
	return -ENOMEM;
}

/**
 * rg_unregister_vms() - Unregister with the Arbiter all the VMs managed by this
 *                       Resource Group
 * @ptm_rg:  pointer to the resource group internal data
 *
 * Unregisters all VMs where each VM for the PTM is represented by an specific
 * AW. Therefore the VM ID matches with the AW ID.
 */
static void rg_unregister_vms(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct mali_vm_priv_data *vm;
	uint32_t vm_id = 0;

	if (!ptm_rg || !ptm_rg->arb_data->ops.unregister_vm)
		return;

	for (vm_id = 0; vm_id < MAX_AW_NUM; vm_id++) {
		if (ptm_rg->ptm_rg_vm[vm_id]) {
			vm = ptm_rg->ptm_rg_vm[vm_id];
			ptm_rg->arb_data->ops.unregister_vm(vm->arb_vm);
			devm_kfree(ptm_rg->dev, vm);
		}
	}
}

/**
 * rg_register_vms() - Register with the Arbiter all the VMs managed by this
 *                     Resource Group
 * @ptm_rg:  pointer to the resource group internal data
 *
 * Registers all VM where each VM for the PTM is represented by an specific AW.
 * Therefore the VM ID matches with the AW ID.
 *
 * Return: 0 if success, or a Linux error code.
 */
static int rg_register_vms(struct mali_gpu_ptm_rg *ptm_rg)
{
	struct device *rg_dev;
	uint32_t aw_mask;
	struct mali_vm_data vm_info;
	uint32_t vm_id = 0;
	int err = 0;

	if (!ptm_rg || !ptm_rg->arb_data->ops.register_vm)
		return -EINVAL;

	rg_dev = ptm_rg->dev;

	/* Commum public VM data */
	vm_info.dev = ptm_rg->dev;
	vm_info.ops.gpu_granted = ptm_rg->vm_ops.ops.gpu_granted;
	vm_info.ops.gpu_stop = ptm_rg->vm_ops.ops.gpu_stop;
	vm_info.ops.gpu_lost = ptm_rg->vm_ops.ops.gpu_lost;

	/* Get the assigned AWs to this RG */
	err = rg_get_aw_mask(rg_dev, &aw_mask);
	if (err) {
		dev_err(rg_dev, "Cannot get AW mask.\n");
		return err;
	}

	/* Register a VM per AW assigned */
	while (aw_mask) {
		if (aw_mask & 0x1) {
			/* Set the VM ID to be registered with the Arbiter */
			vm_info.id = vm_id;

			/* Set private VM data */
			err = rg_init_vm_priv_data(ptm_rg, vm_id);
			if (err) {
				dev_err(rg_dev, "Cannot initialize VM %d private data.\n", vm_id);
				goto fail_reg;
			}

			err = ptm_rg->arb_data->ops.register_vm(ptm_rg->arb_data, &vm_info,
								ptm_rg->ptm_rg_vm[vm_id],
								&ptm_rg->ptm_rg_vm[vm_id]->arb_vm);
			if (err) {
				dev_err(rg_dev, "Cannot register VM %d with Arbiter.\n", vm_id);
				goto fail_reg;
			}
		}
		aw_mask >>= 1;
		vm_id++;
	}

	return 0;

fail_reg:
	rg_unregister_vms(ptm_rg);
	return err;
}


/**
 * res_group_probe() - Initialize the res_group device.
 * @pdev: The platform device.
 *
 * Called when device is matched in device tree, allocate the resources
 * for the device and set up the proper callbacks in the arbiter_if struct.
 *
 * Return: 0 if success, or a Linux error code.
 */
static int res_group_probe(struct platform_device *pdev)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	struct device *dev;
	struct resource *reg_res;
	void __iomem *mem;
	int err = 0;
	int irq_req;
	int irq;

	if (!pdev)
		return -EINVAL;

	dev = &pdev->dev;
	/* Device data alloc. */
	ptm_rg = devm_kzalloc(dev, sizeof(struct mali_gpu_ptm_rg), GFP_KERNEL);
	if (!ptm_rg)
		return -ENOMEM;

	/* Assign IRQ. */
	irq = platform_get_irq(pdev, 0);
	if (irq < 0)
		goto out_rg_alloc;

	ptm_rg->rg_irq.irq = irq;
	ptm_rg->rg_irq.flags = irqd_get_trigger_type(irq_get_irq_data(irq));

	/* Get the memory region of the registers. */
	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!reg_res) {
		dev_err(dev, "Invalid register resource.\n");
		err = -ENOENT;
		goto out_rg_alloc;
	}

	/* Request the memory region. */
	if (!request_mem_region(reg_res->start, resource_size(reg_res), dev_name(dev))) {
		dev_err(dev, "Register window unavailable.\n");
		err = -EIO;
		goto out_rg_alloc;
	}

	/* ioremap to the virtual address. */
	mem = ioremap(reg_res->start, resource_size(reg_res));
	if (!mem) {
		dev_err(dev, "Can't remap register window.\n");
		err = -EINVAL;
		goto out_rg_ioremap;
	}

	/* Set device specific information on the ptm_rg. */
	ptm_rg->dev = dev;
	ptm_rg->res = reg_res;
	ptm_rg->mem = mem;
	mutex_init(&ptm_rg->mutex);

	/* Set the Arbiter message sending callbacks. */
	ptm_rg->vm_ops.ops.gpu_granted = on_gpu_granted;
	ptm_rg->vm_ops.ops.gpu_stop = on_gpu_stop;
	ptm_rg->vm_ops.ops.gpu_lost = on_gpu_lost;

	/* Set Resource Group specific calbacks. */
	ptm_rg->ptm_rg_ops.get_slice_tiler_type = rg_get_slice_tiler_type;
	ptm_rg->ptm_rg_ops.get_slice_mask = rg_get_slice_mask;
	ptm_rg->ptm_rg_ops.get_partition_mask = rg_get_partition_mask;
	ptm_rg->ptm_rg_ops.get_aw_mask = rg_get_aw_mask;
	ptm_rg->ptm_rg_ops.get_slices_core_mask = rg_get_slices_core_mask;
	ptm_rg->ptm_rg_ops.poweron_slices = rg_poweron_slices;
	ptm_rg->ptm_rg_ops.poweroff_slices = rg_poweroff_slices;
	ptm_rg->ptm_rg_ops.get_powered_slices_mask = rg_get_powered_slices_mask;
	ptm_rg->ptm_rg_ops.enable_slices = rg_enable_slices;
	ptm_rg->ptm_rg_ops.reset_slices = rg_disable_slices;
	ptm_rg->ptm_rg_ops.get_enabled_slices_mask = rg_get_enabled_slices_mask;

	/* Store the device data. */
	dev_set_drvdata(dev, &ptm_rg->ptm_rg_ops);

	/* Check Partition Manager configuration and assignment. */
	err = check_ptm_configuration(ptm_rg);
	if (err)
		goto out_rg_dev_id;

	/* Init work queue to process the messages received. */
	ptm_rg->ptm_rg_wq = alloc_ordered_workqueue("ptm_rg_wq", WQ_HIGHPRI);
	if (!ptm_rg->ptm_rg_wq) {
		dev_err(dev, "Failed to allocate the work queue.\n");
		err = -EINVAL;
		goto out_rg_dev_id;
	}
	INIT_WORK(&ptm_rg->ptm_rg_req_work, res_group_process_message);


	/* Get information and create the Arbiter */
	err = rg_arbiter_info_init(ptm_rg);
	if (err) {
		dev_err(dev, "Arbiter initialization failed\n");
		goto out_arb_init;
	}

	/* Register all the the VMs managed by the Resource Group */
	err = rg_register_vms(ptm_rg);
	if (err) {
		dev_err(dev, "VMs registration failed\n");
		goto out_reg_vms;
	}

	/*
	 * After initalizing the arbiter it is necessary to initialize the
	 * communication by reading any messages received before probe.
	 */
	if (initialize_communication(ptm_rg))
		dev_dbg(dev, "Error initializing the communication layer.\n");

	/* Request IRQ. */
	irq_req = request_irq(ptm_rg->rg_irq.irq, res_group_irq_handler,
			      ptm_rg->rg_irq.flags | IRQF_SHARED, dev_name(dev), ptm_rg);
	if (irq_req) {
		dev_err(dev, "Can't request interrupt.\n");
		err = irq_req;
		goto out_req_irq;
	}
	ptm_rg->edge_irq = EDGE_IRQ(ptm_rg->rg_irq.flags);

	/* Enable all IRQs - 16 bits. */
	iowrite32(MAX_AW_MASK, ptm_rg->mem + PTM_RESOURCE_IRQ_MASK);

	/* With the IRQs already enabled, initialize the Protocol Handshake. */
	if (initialize_handshake(ptm_rg))
		dev_dbg(dev, "Error initializing the protocol handshake.\n");

	dev_info(dev, "Probed\n");
	return err;

out_req_irq:
	rg_unregister_vms(ptm_rg);
out_reg_vms:
	if (ptm_rg->pwr_if.dev) {
		module_put(ptm_rg->pwr_if.dev->driver->owner);
		put_device(ptm_rg->pwr_if.dev);
	}
	rg_release_resources(ptm_rg);
out_arb_init:
	destroy_workqueue(ptm_rg->ptm_rg_wq);
	ptm_msg_handler_destroy(&ptm_rg->msg_handler);
out_rg_dev_id:
	dev_set_drvdata(dev, NULL);
	iounmap(mem);
out_rg_ioremap:
	release_mem_region(reg_res->start, resource_size(reg_res));
out_rg_alloc:
	devm_kfree(dev, ptm_rg);
	return err;
}

/**
 * res_group_remove() - Remove the res_group device.
 * @pdev: The platform device.
 *
 * Called when the device is being unloaded to do any cleanup.
 *
 * Return: Always returns 0.
 */
static int res_group_remove(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct mali_gpu_ptm_rg *ptm_rg;
	struct resource *reg_res;
	void __iomem *mem;

	ptm_rg = to_mali_gpu_ptm_rg(dev);
	if (!ptm_rg)
		return -ENODEV;

	reg_res = ptm_rg->res;
	mem = ptm_rg->mem;

	/* Disable all IRQs */
	iowrite32(0, ptm_rg->mem + PTM_RESOURCE_IRQ_MASK);

	/* Flush work and destroy the work queue. */
	flush_work(&ptm_rg->ptm_rg_req_work);
	destroy_workqueue(ptm_rg->ptm_rg_wq);

	/* Free IRQ. */
	free_irq(ptm_rg->rg_irq.irq, ptm_rg);

	/* Wait until all messages are sent */
	ptm_msg_handler_destroy(&ptm_rg->msg_handler);

	/* iounmap and release mem region of the device. */
	iounmap(mem);
	release_mem_region(reg_res->start, resource_size(reg_res));
	dev_set_drvdata(dev, NULL);


	/* Free the memory and unregister the VMs */
	rg_unregister_vms(ptm_rg);

	/* Destroy Arbiter */
	arbiter_destroy(dev, ptm_rg->arb_data);
	/* Release power control module */
	if (ptm_rg->pwr_if.dev) {
		module_put(ptm_rg->pwr_if.dev->driver->owner);
		put_device(ptm_rg->pwr_if.dev);
	}
	/* Release the resources of this resource group. */
	rg_release_resources(ptm_rg);
	/* Free the memory allocated to the device */
	devm_kfree(&pdev->dev, ptm_rg);

	return 0;
}

/**
 * get_slice_mask_from_clock_reg() - Translates clock register to slice mask.
 * @slice_clock_set_val: Slice clock register value.
 *
 * Get one bit per slice mask from two bit per slice register value.
 *
 * Return: slice mask.
*/
static uint32_t get_slice_mask_from_clock_reg(uint32_t slice_clock_set_val)
{
	int i;
	uint32_t slice_mask = 0;

	for (i = 0; i < PTM_NUM_OF_SLICES; i++) {
		if ((slice_clock_set_val >> (i *
					CLOCK_REG_BITS_PER_SLICE)) &
					SLICE_CLOCK_MASK_BIT)
			slice_mask |= (1 << i);
	}

	return slice_mask;
}

/**
 * gpu_resource_group_device_suspend - Suspend callback from the OS.
 *
 * @dev:  The device to suspend
 *
 * This is called by Linux when the device should suspend.
 *
 * Return: A standard Linux error code
 */
static int gpu_resource_group_device_suspend(struct device *dev)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	uint32_t slice_clock_set_val;
	
	if (!dev)
		return -ENODEV;
	
	ptm_rg = to_mali_gpu_ptm_rg(dev);

	if (!ptm_rg)
		return -ENODEV;
	
	ptm_rg->ptm_rg_irq_mask = ioread32(ptm_rg->mem + PTM_RESOURCE_IRQ_MASK);
  	
	mutex_lock(&ptm_rg->mutex);
  	slice_clock_set_val = ioread32(ptm_rg->mem +
  					PTM_RESOURCE_SLICE_CLOCK_SET);
	ptm_rg->slices = get_slice_mask_from_clock_reg(slice_clock_set_val);
	mutex_unlock(&ptm_rg->mutex);

	return 0;
}

/**
 * gpu_resource_group_device_resume - Resume callback from the OS.
 *
 * @dev:  The device to resume
 *
 * This is called by Linux when the device should resume from suspension.
 *
 * Return: A standard Linux error code
 */
static int gpu_resource_group_device_resume(struct device *dev)
{
	struct mali_gpu_ptm_rg *ptm_rg;
	int err = 0;

	if (!dev)
		return -ENODEV;
	
	ptm_rg = to_mali_gpu_ptm_rg(dev);

	if (!ptm_rg)
		return -ENODEV;

	iowrite32(ptm_rg->ptm_rg_irq_mask, ptm_rg->mem + PTM_RESOURCE_IRQ_MASK);

	err = rg_poweron_slices(dev, ptm_rg->slices);
	if (err) {
  		dev_err(dev, "Slice power on failed. slice_mask = 0x%x", ptm_rg->slices);
  		return err;
  	}

	err = rg_enable_slices(dev, ptm_rg->slices);
  	if (err) {
  		dev_err(dev, "Slice enable failed. slice_mask = 0x%x", ptm_rg->slices);
  		return err;
  	}
	
	return 0;
}

/* The power management operations for the platform driver.
 */
static const struct dev_pm_ops gpu_resource_group_pm_ops = {
	.suspend = gpu_resource_group_device_suspend,
	.resume = gpu_resource_group_device_resume,
};

static const struct of_device_id res_group_dt_match[] = { { .compatible = RES_GROUP_DT_NAME }, {} };

static struct platform_driver res_group_driver = {
	.probe = res_group_probe,
	.remove = res_group_remove,
	.driver = {
		.name = "mali_gpu_resource_group",
		.of_match_table = res_group_dt_match,
		.pm = &gpu_resource_group_pm_ops
	},
};

/**
 * mali_gpu_resource_group_init() - Register platform driver.
 *
 * Return: See definition of platform_driver_register().
 */
static int __init mali_gpu_resource_group_init(void)
{
	return platform_driver_register(&res_group_driver);
}
module_init(mali_gpu_resource_group_init);

/**
 * mali_gpu_resource_group_exit() - Unregister platform driver.
 */
static void __exit mali_gpu_resource_group_exit(void)
{
	platform_driver_unregister(&res_group_driver);
}
module_exit(mali_gpu_resource_group_exit);

MODULE_ALIAS("mali-gpu-resource-group");
MODULE_DESCRIPTION(
	"The Resource Group module is responsible for communicating with the Access Windows. It is also responsible for controlling the Power, Reset and Clock state of slices.");
MODULE_AUTHOR("ARM Ltd.");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");
