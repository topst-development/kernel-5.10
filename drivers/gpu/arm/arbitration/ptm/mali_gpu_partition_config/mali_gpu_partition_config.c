// SPDX-License-Identifier: GPL-2.0 WITH Linux-syscall-note OR MIT

/*
 * (C) COPYRIGHT 2023 Telechips Inc
 * (C) COPYRIGHT 2020-2023 ARM Limited or its affiliates. All rights reserved.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of_address.h>
#include <linux/iopoll.h>
#include <linux/of_platform.h>
#include <mali_gpu_partition_config.h>

/* Added by Telechips to support Suspend to RAM */
#if defined(CONFIG_TCC_MALI_VZ) && !defined(CONFIG_TCC807X_CA55_SUB)
#define TELECHIPS_MALI_GPU_3D_CFG_DT_NAME "telechips,mali-gpu-3d-cfg"
#define GPU_3DENGINE_CLKMASK                    (0x0)
#define GPU_3DENGINE_CLKMASK_STATUSCHECK        (1)
#define GPU_3DENGINE_CLKMASK_STATUSCHECK_MASK            \
                (0x1 << GPU_3DENGINE_CLKMASK_STATUSCHECK)
#endif

/* Partition config version against which was implemented this module */
#define MALI_PARTITION_CONFIG_IMPLEMENTATION_VERSION 1
#if MALI_PARTITION_CONFIG_IMPLEMENTATION_VERSION != MALI_PARTITION_CONFIG_VERSION
#error " Unsupported partition config API version."
#endif

/* Device tree compatible ID of the Partition Config module */
#define PTM_CONFIG_DT_NAME "arm,mali-gpu-partition-config"

#define REG_POLL_SLEEP_US 1
#define REG_POLL_TIMEOUT_US 40
#define PTM_DEVICE_ID 0x00
#define PTM_SLICE_MODE_NEW_OFFSET 0x0018
#define PTM_SLICE_MODE_UPDATE_OFFSET 0x001C
#define PTM_SLICE_MODE_ACK_OFFSET 0x0020
#define PTM_SLICE_MODE_ACK_STATUS_MASK 0x01
#define PTM_SLICE_MODE_ACK_ERROR_MASK 0x02
#define PTM_SLICE_MODE_DISABLED 0x0
#define PTM_SLICE_MODE_PRIMARY 0x1
#define PTM_SLICE_MODE_SECONDARY 0x2
#define PTM_NUM_OF_SLICES 0x8
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
#define PTM_SLICE_BITS_PER_MODE 0x2
#define PTM_SLICE_MODE_UPDATE_TRIGGER_VALUE 0x1

/**
 * struct mali_gpu_part_cfg - Partition config data
 * @mem:           Mapped memory from physical address retrieved from dt
 * @reg_resource:  Resource contains address range retrieved from device tree
 * @dev:           Domain device set in during probe
 * @prt_cfg_ops:   partition config callbacks
 * @slices:        Slices representations
 */
struct mali_gpu_part_cfg {
	void __iomem *mem;
	struct resource *reg_resource;
	struct device *dev;
	struct part_cfg_ops prt_cfg_ops;
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
 * is_slice_mask_valid() - Check if slice mask is valid.
 * @slices:   Bit mask of slices to configure.
 *
 * Check if there is no gaps of enabled slices in the mask
 * and that it's non-zero
 *
 * Return:    0 if successful, 1 if not.
 */
static bool is_slice_mask_valid(uint32_t slices)
{
	bool ones_start = false;
	bool ones_end = false;
	uint8_t i;

	for (i = 0; i < PTM_NUM_OF_SLICES; i++) {
		if (slices & (1 << i)) {
			if (ones_end)
				return false;
			if (!ones_start)
				ones_start = true;
		} else {
			if (ones_start) {
				ones_start = false;
				ones_end = true;
			}
		}
	}
	return true;
}

/**
 * to_mali_gpu_part_cfg() - gets pointer to mali_gpu_part_cfg
 * @dev: Pointer to the partition config device.
 *
 * Gets pointer to mali_gpu_part_cfg structure from device data
 *
 * Return: mali_gpu_part_cfg* if successful, otherwise NULL
 */
static struct mali_gpu_part_cfg *to_mali_gpu_part_cfg(struct device *dev)
{
	struct part_cfg_ops *data;

	data = dev_get_drvdata(dev);
	if (likely(data)) {
		return container_of(data, struct mali_gpu_part_cfg, prt_cfg_ops);
	}

	dev_err(dev, "Partition Config interface missing.\n");

	return NULL;
}

/**
 * mali_gpu_partition_assign_slices() - Assign slices to the partition.
 * @dev:      Pointer to the partition config module device.
 * @slices:   Bit mask of slices to configure.
 *
 * The slice mode will be automatically set (i.e. the Master slice will be the
 * lowest index slice in the range).
 *
 * Return:
 * * 0		- success.
 * * -EINVAL	- invalid argument.
 * * -ENODEV	- expected interface not available.
 * * -EIO	- failed with hardware fault.
 */
static int mali_gpu_partition_assign_slices(struct device *dev, uint32_t slices)
{
	int error;
	uint32_t slice_mode_ack;
	uint32_t slice_mode_wr = 0;
	uint32_t i = 0;
	struct mali_gpu_part_cfg *partition_config;
	uint8_t slice_mode;
	bool first_slice = true;

	if (!dev) {
		pr_err("%s : Invalid argument.\n", __func__);
		return -EINVAL;
	}

	partition_config = to_mali_gpu_part_cfg(dev);
	if (partition_config == NULL)
		return -ENODEV;

	if (!is_slice_mask_valid(slices)) {
		dev_err(dev, "Invalid slice mask.\n");
		return -EINVAL;
	}

	for (i = 0; i < PTM_NUM_OF_SLICES; i++) {
		slice_mode = PTM_SLICE_MODE_DISABLED;
		if (slices & (1 << i)) {
			if (first_slice) {
				slice_mode = PTM_SLICE_MODE_PRIMARY;
				first_slice = false;
			} else {
				slice_mode = PTM_SLICE_MODE_SECONDARY;
			}
		}
		slice_mode_wr |= (slice_mode << (i * PTM_SLICE_BITS_PER_MODE));
	}

	iowrite32(slice_mode_wr, partition_config->mem + PTM_SLICE_MODE_NEW_OFFSET);
	iowrite32(PTM_SLICE_MODE_UPDATE_TRIGGER_VALUE,
		  partition_config->mem + PTM_SLICE_MODE_UPDATE_OFFSET);

	/* PTM_SLICE_MODE_ACK.STATUS bit is set automatically by the HW
	 * whenever the update starts. The HW will automatically clear this bit
	 * when the process complete.
	 * In case of error PTM_SLICE_MODE_ACK.ERROR will be set.
	 */
	error = readx_poll_timeout(ioread32, partition_config->mem + PTM_SLICE_MODE_ACK_OFFSET,
				   slice_mode_ack,
				   !(slice_mode_ack & PTM_SLICE_MODE_ACK_STATUS_MASK),
				   REG_POLL_SLEEP_US, REG_POLL_TIMEOUT_US);

	/* This will be ETIMEDOUT if the GPU failed to ACK, but as this is a
	 * serious hardware error, the error code is replaced
	 */
	if (error) {
		dev_err(dev, "Slice mode update timed out.\n");
		error = -EIO;
	}

	if (!error && (slice_mode_ack & PTM_SLICE_MODE_ACK_ERROR_MASK)) {
		dev_err(dev, "Slice mode update failed.\n");
		error = -EIO;
	}
	
	partition_config->slices = slices;
	return error;
}


/**
 * pm_config_probe() - Initialize the pm_config device
 * @pdev: The platform device
 *
 * Called when device is matched in device tree, allocate the resources
 * for the device and make necessary mapping to access registers.
 *
 * Return: 0 if success, or a Linux error code
 */
static int pm_config_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct mali_gpu_part_cfg *partition_config;
	struct resource *reg_resource;

	if (!pdev)
		return -EINVAL;

	reg_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!reg_resource) {
		dev_err(&pdev->dev, "Invalid register resource\n");
		return -ENOENT;
	}

	partition_config = devm_kzalloc(&pdev->dev, sizeof(struct mali_gpu_part_cfg), GFP_KERNEL);

	if (!partition_config)
		return -ENOMEM;

	if (!request_mem_region(reg_resource->start, resource_size(reg_resource),
				"mali_pm_config")) {
		dev_err(&pdev->dev, "Requesting Memory region failed\n");
		ret = -EIO;
		goto req_memory_fail;
	}

	partition_config->mem = ioremap(reg_resource->start, resource_size(reg_resource));

	if (partition_config->mem == NULL) {
		dev_err(&pdev->dev, "Remapping Memory region failed\n");
		ret = -EIO;
		goto ioremap_fail;
	}

	/* Check the partition manager ID. */
	ret = check_ptm_version(&pdev->dev, ioread32(partition_config->mem + PTM_DEVICE_ID));
	if (ret)
		goto unsupported_version_fail;

	partition_config->dev = &pdev->dev;
	partition_config->reg_resource = reg_resource;
	partition_config->prt_cfg_ops.assign_slices = mali_gpu_partition_assign_slices;

	dev_set_drvdata(&pdev->dev, &partition_config->prt_cfg_ops);

	dev_info(partition_config->dev, "Probed\n");
	return 0;

unsupported_version_fail:
	iounmap(partition_config->mem);
ioremap_fail:
	release_mem_region(reg_resource->start, resource_size(reg_resource));
req_memory_fail:
	devm_kfree(&pdev->dev, partition_config);

	return ret;
}

/**
 * pm_config_remove() - Remove the pm_config device
 * @pdev: The platform device
 *
 * Called when the device is being unloaded to do any cleanup
 *
 * Return: 0 if success, or a Linux error code
 */
static int pm_config_remove(struct platform_device *pdev)
{
	struct part_cfg_ops *ops;
	struct mali_gpu_part_cfg *partition_config;

	if (!pdev)
		return -EINVAL;

	ops = dev_get_drvdata(&pdev->dev);
	if (ops == NULL) {
		dev_err(&pdev->dev, "Retrieving ptm config device failed\n");
		return -ENODEV;
	}

	partition_config = container_of(ops, struct mali_gpu_part_cfg, prt_cfg_ops);
	if (partition_config == NULL) {
		dev_err(&pdev->dev, "Retrieving ptm partition config failed\n");
		return -ENODEV;
	}

	release_mem_region(partition_config->reg_resource->start,
			   resource_size(partition_config->reg_resource));
	iounmap(partition_config->mem);

	platform_set_drvdata(pdev, NULL);
	devm_kfree(&pdev->dev, partition_config);

	return 0;
}

static int gpu_partition_cfg_device_suspend(struct device *dev)
{
/* Added by Telechips to support Suspend to RAM */
#if defined(CONFIG_TCC_MALI_VZ) && !defined(CONFIG_TCC807X_CA55_SUB)
	struct device_node *tcc_3d_cfg_np = NULL;
	void __iomem *base_addr;	
	u32 val = 0;
	
	tcc_3d_cfg_np = of_find_compatible_node(NULL, NULL, 
						TELECHIPS_MALI_GPU_3D_CFG_DT_NAME);
						
	if (tcc_3d_cfg_np == NULL) {
		dev_err(dev, "tcc_3d_cfg node not found \n");
		return 0;
	} else {
		base_addr = (void __iomem *)of_iomap(tcc_3d_cfg_np, 0);
		if (!base_addr) {
			dev_err(dev, "Failed to get the register address of tcc_3d_cfg\n");
			return 0;
		}		
	}

	val = ioread32(base_addr + GPU_3DENGINE_CLKMASK);
	iowrite32((val | GPU_3DENGINE_CLKMASK_STATUSCHECK_MASK), base_addr + GPU_3DENGINE_CLKMASK);
	iounmap(base_addr);
#endif	
	return 0;
}


/**
 * gpu_partition_cfg_device_resume - Resume callback from the OS.
 *
 * @dev:  The device to resume
 *
 * This is called by Linux when the device should resume from suspension.
 *
 * Return: A standard Linux error code
 */
static int gpu_partition_cfg_device_resume(struct device *dev)
{
	struct part_cfg_ops *ops;
	struct mali_gpu_part_cfg *ptm_pc_data;
	int err = 0;
/* Added by Telechips to support Suspend to RAM */
#if defined(CONFIG_TCC_MALI_VZ) && !defined(CONFIG_TCC807X_CA55_SUB)
	struct device_node *tcc_3d_cfg_np = NULL;
	void __iomem *base_addr;	
	u32 val = 0;

	tcc_3d_cfg_np = of_find_compatible_node(NULL, NULL, 
						TELECHIPS_MALI_GPU_3D_CFG_DT_NAME);
						
	if (tcc_3d_cfg_np == NULL) {
		dev_err(dev, "tcc_3d_cfg node not found \n");
		return 0;
	} else {
		base_addr = (void __iomem *)of_iomap(tcc_3d_cfg_np, 0);
		if (!base_addr) {
			dev_err(dev, "Failed to get the register address of tcc_3d_cfg\n");
			return 0;
		}		
	}
	
	do {
		val = ioread32(base_addr + GPU_3DENGINE_CLKMASK);
		msleep(1);
	} while(val & GPU_3DENGINE_CLKMASK_STATUSCHECK_MASK);

	iounmap(base_addr);
#endif

	if (!dev)
		return -ENODEV;

	ops = dev_get_drvdata(dev);
	if (!ops)
		return -ENODEV;

	ptm_pc_data = to_mali_gpu_part_cfg(dev);
	if (!ptm_pc_data)
		return -ENODEV;

	err = mali_gpu_partition_assign_slices(dev, ptm_pc_data->slices);
	if(err)
	{
		dev_err(dev, "[%s]Partition slice config failed\n", __func__);
		return err;
	}

	return 0;
}

/* The power management operations for the platform driver.
 */
static const struct dev_pm_ops gpu_partition_cfg_pm_ops = {
	.suspend = gpu_partition_cfg_device_suspend,
	.resume = gpu_partition_cfg_device_resume,
};

static const struct of_device_id pm_config_dt_match[] = { { .compatible = PTM_CONFIG_DT_NAME },
							  {} };

static struct platform_driver pm_config_driver = {
	.probe = pm_config_probe,
	.remove = pm_config_remove,
	.driver = {
		.name = "mali_pm_config",
		.of_match_table = pm_config_dt_match,
		.pm = &gpu_partition_cfg_pm_ops,
	},
};

/**
 * mali_pm_config_init() - Register platform driver
 *
 * Return: See definition of platform_driver_register()
 */
static int __init mali_pm_config_init(void)
{
	return platform_driver_register(&pm_config_driver);
}
module_init(mali_pm_config_init);

static void __exit mali_pm_config_exit(void)
{
	platform_driver_unregister(&pm_config_driver);
}
module_exit(mali_pm_config_exit);

MODULE_ALIAS("mali-gpu-partition-config");
MODULE_DESCRIPTION("Mali Partition Manager config module");
MODULE_AUTHOR("ARM Ltd.");
MODULE_LICENSE("Dual MIT/GPL");
MODULE_VERSION("1.0");
