/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_comm.h"
#include "vpu_etc.h"
#include "vpu_rm.h"
#include "vpu_dbg_string.h"

// This header must located after "vpu_comm.h".
#include <dt-bindings/pmap/common/vpu_mem_size.h>

#define CREATE_TRACE_POINTS
#include <trace/events/tcc_vpu.h>

#define vpu_writel writel
#define vpu_readl readl

#ifdef CONFIG_VPU_TIME_MEASUREMENT
int vetc_GetTimediff_ms(struct timeval time1, struct timeval time2)
{
	int time_diff_ms = 0;

	time_diff_ms = (time2.tv_sec - time1.tv_sec) * 1000;
	time_diff_ms += (time2.tv_usec - time1.tv_usec) / 1000;

	return time_diff_ms;
}
EXPORT_SYMBOL(vetc_GetTimediff_ms);
#endif

//enum vpu_ip_type of vpu_internal_type.h
u8 vetc_reg_readb(int vpu_ip, u32 raw_base, void *ioaddr, int offset)
{
	u8 val = readb(ioaddr + offset);
	trace_tcc_vpu_rw(0, vpu_ip, raw_base + offset, 1, val);

	V_DBG(VPU_DBG_REG_DUMP, "[%s] READ 0x%08x--0x%08x 1 0x%08x", vmgr_get_ip_name(vpu_ip), (unsigned int)(raw_base + offset), (unsigned int)(raw_base + offset), val);
	return val;
}
EXPORT_SYMBOL(vetc_reg_readb);

u16 vetc_reg_readw(int vpu_ip, u32 raw_base, void *ioaddr, int offset)
{
	u16 val = readw(ioaddr + offset);
	trace_tcc_vpu_rw(0, vpu_ip, offset, 2, val);

	V_DBG(VPU_DBG_REG_DUMP, "[%s] READ 0x%08x--0x%08x 2 0x%08x", vmgr_get_ip_name(vpu_ip), (unsigned int)(raw_base + offset), (unsigned int)(raw_base + offset)+1, val);
	return  val;
}
EXPORT_SYMBOL(vetc_reg_readw);

u32 vetc_reg_readl(int vpu_ip, u32 raw_base, void *ioaddr, int offset)
{
	u32 val = readl(ioaddr + offset);
	trace_tcc_vpu_rw(0, vpu_ip, raw_base + offset, 4, val);

	V_DBG(VPU_DBG_REG_DUMP, "[%s] READ 0x%08x--0x%08x 4 0x%08x", vmgr_get_ip_name(vpu_ip), (unsigned int)(raw_base + offset), (unsigned int)(raw_base + offset)+3, val);
	return  val;
}
EXPORT_SYMBOL(vetc_reg_readl);

void vetc_reg_writeb(int vpu_ip, u32 raw_base, void *ioaddr, int offset, u8 val)
{
	V_DBG(VPU_DBG_REG_DUMP, "[%s] WRITE 0x%08x--0x%08x 1 0x%08x", vmgr_get_ip_name(vpu_ip), (unsigned int)(raw_base + offset), (unsigned int)(raw_base + offset), val);

	trace_tcc_vpu_rw(1, vpu_ip, raw_base + offset, 1, val);
	writeb(val, ioaddr + offset);
}
EXPORT_SYMBOL(vetc_reg_writeb);

void vetc_reg_writew(int vpu_ip, u32 raw_base, void *ioaddr, int offset, u16 val)
{
	V_DBG(VPU_DBG_REG_DUMP, "[%s] WRITE 0x%08x--0x%08x 2 0x%08x", vmgr_get_ip_name(vpu_ip), (unsigned int)(raw_base + offset), (unsigned int)(raw_base + offset)+1, val);

	trace_tcc_vpu_rw(1, vpu_ip, raw_base + offset, 2, val);
	writew(val, ioaddr + offset);
}
EXPORT_SYMBOL(vetc_reg_writew);

void vetc_reg_writel(int vpu_ip, u32 raw_base, void *ioaddr, int offset, u32 val)
{
	V_DBG(VPU_DBG_REG_DUMP, "[%s] WRITE 0x%08x--0x%08x 4 0x%08x", vmgr_get_ip_name(vpu_ip), (unsigned int)(raw_base + offset), (unsigned int)(raw_base + offset)+3, val);

	trace_tcc_vpu_rw(1, vpu_ip, raw_base + offset, 4, val);
	writel(val, ioaddr + offset);
}
EXPORT_SYMBOL(vetc_reg_writel);

unsigned int vetc_reg_read(void *base_addr, unsigned int offset)
{
	return vpu_readl((base_addr + offset));
}
EXPORT_SYMBOL(vetc_reg_read);

void vetc_reg_write(void *base_addr, unsigned int offset, unsigned int data)
{
	vpu_writel(data, (base_addr + offset));
}
EXPORT_SYMBOL(vetc_reg_write);

#undef DEBUG_DUMP
void vetc_dump_reg_all(char *base_addr, unsigned char *str)
{
#ifdef DEBUG_DUMP
	unsigned int i = 0;

	V_DBG(VPU_DBG_REG_DUMP, "%s", str);
	while (i < 0x200) {
		V_DBG(VPU_DBG_REG_DUMP,
			"0x%8p : 0x%8x 0x%8x 0x%8x 0x%8x",
			base_addr + i,
			vetc_reg_read(base_addr, i+0x0),
			vetc_reg_read(base_addr, i+0x4),
			vetc_reg_read(base_addr, i+0x8),
			vetc_reg_read(base_addr, i+0xC));
		i += 0x10;
	}
#endif
}
EXPORT_SYMBOL(vetc_dump_reg_all);

void vetc_reg_init(char *base_addr)
{
	unsigned int i = 0;

	while (i < 0x200U) {
		vpu_writel(0x00, (base_addr + i + 0x0U));
		vpu_writel(0x00, (base_addr + i + 0x4U));
		vpu_writel(0x00, (base_addr + i + 0x8U));
		vpu_writel(0x00, (base_addr + i + 0xCU));
		i += 0x10U;
	}

// To confirm if value are initialized!!
#ifdef DEBUG_DUMP
	while (i < 0x200U) {
		V_DBG(VPU_DBG_REG_DUMP,
			"0x%8x : 0x%8x 0x%8x 0x%8x 0x%8x",
				base_addr + i,
				vetc_reg_read(base_addr, i+0x0U),
				vetc_reg_read(base_addr, i+0x4U),
				vetc_reg_read(base_addr, i+0x8U),
				vetc_reg_read(base_addr, i+0xCU));
		i += 0x10U;
	}
#endif
}
EXPORT_SYMBOL(vetc_reg_init);

void *vetc_ioremap(unsigned int phy_addr, unsigned int size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	return ioremap_wc(phy_addr, size);
#else
	return ioremap_nocache(phy_addr, size);
#endif
}
EXPORT_SYMBOL(vetc_ioremap);

void vetc_iounmap(void *virt_addr)
{
	iounmap((void __iomem *)virt_addr);
}
EXPORT_SYMBOL(vetc_iounmap);

void *vetc_memcpy(void *dest, const void *src, unsigned int count,
		  unsigned int type)
{
	void *ret = NULL;

	if (vmem_is_cma_allocated_virt_region_cv(src, count) > 0) {
		type = 0U;
	}

	if (type == 1U) {
		(void)memcpy_fromio(dest, src, count);
	} else if (type == 2) {
		(void)memcpy_toio(dest, src, count);
	} else {
		ret = memcpy(dest, src, count);
	}

	return ret;
}
EXPORT_SYMBOL(vetc_memcpy);

void vetc_memset(void *ptr, int value, unsigned int num, unsigned int type)
{
	if (vmem_is_cma_allocated_virt_region(ptr, num) > 0) {
		type = 0U;
	}

	if (type == 1U) {
		(void)memset_io(ptr, value, num);
	} else {
		(void)memset(ptr, (unsigned char)((unsigned int)value & 0x0FFU), num);
	}
}
EXPORT_SYMBOL(vetc_memset);

void vetc_usleep(unsigned int uimin, unsigned int uimax)
{
	usleep_range((unsigned long)uimin, (unsigned long)uimax);
}
EXPORT_SYMBOL(vetc_usleep);

void vetc_mutex_lock(void *lock)
{
	struct mutex *mlock = lock;

	mutex_lock(mlock);
}
EXPORT_SYMBOL(vetc_mutex_lock);

void vetc_mutex_unlock(void *lock)
{
	struct mutex *mlock = lock;

	mutex_unlock(mlock);
}
EXPORT_SYMBOL(vetc_mutex_unlock);

char *vetc_strncpy(char *dest, const char *src, int len)
{
	char *ret = NULL;

	if (dest != NULL && src != NULL) {
		ret = dest;

		while (*src != '\0' && len--) {
			*dest = *src;
			dest++;
			src++;
		}

		*dest = '\0';
	}

	return ret;
}
EXPORT_SYMBOL(vetc_strncpy);

void vetc_vm_flags_set(struct vm_area_struct *vma, vm_flags_t flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0) || defined(ANDROID_VPU_KERNEL)
	vm_flags_set(vma, flags);
#else
	vma->vm_flags |= flags;
#endif
}
EXPORT_SYMBOL(vetc_vm_flags_set);

/* VPU Firmware */
#define VPU_FW_MAX (8)
unsigned char *vpu_fw_name[VPU_FW_MAX] = {
	"vpu_c7.bin",
	"vpu4k_d2.bin",
	"hevc_d2.bin",
	"hevc_e3.bin",
	"vpu_d8.bin",
	"vpu4k_e1.bin"
};

enum vfwidx {
	VPU_C7_FW,         // 00
	VPU4K_D2_FW,       // 01
	HEVC_D2_FW,        // 02
	HEVC_E3_FW,        // 03
	VPU_D8_FW,         // 04
	VPU4K_E1_FW,        // 05 ~ 06
	JPU_FW = VPU_FW_MAX
};

int vetc_prepare_firmware(struct platform_device *pdev, const enum vpu_ip_type ip_type, codec_addr_t *fw_addr)
{
	const struct firmware *fw = NULL;
	struct device *dev = NULL;
	struct device_node *np = NULL;
	struct reserved_mem *rmem = NULL;
	void __iomem *dst = NULL;
	unsigned char* fw_name = NULL;
	int ip_idx;
	int ret = 0;

	dev = &pdev->dev;

	switch (ip_type){
		case VPU_IP_C7:
		{
			ip_idx = VPU_C7_FW;
		}
		break;

		case VPU_IP_4KD2:
		{
			ip_idx = VPU4K_D2_FW;
		}
		break;

		case VPU_IP_HEVC_DEC:
		{
			V_DBG(VPU_DBG_INFO, "HEVC D2 is not supported yet.");
			ip_idx = HEVC_D2_FW;
		}
		break;

		case VPU_IP_HEVC_ENC:
		case VPU_IP_HEVC_ENC2:
		{
			ip_idx = HEVC_E3_FW;
		}
		break;

		case VPU_IP_JPU_C6:
		{
			V_DBG(VPU_DBG_INFO, "The JPU IP operates without firmware.");
			ip_idx = JPU_FW;
		}
		break;

		default:
		{
			ret = -EINVAL;
			dev_err(dev, "Not support video f/w functions\n");
			break;
		}
	}

	if ((ret != 0)
			|| (ip_idx == JPU_FW) /* The JPU IP operates without firmware. */
			|| (ip_idx == HEVC_D2_FW) /* HEVC D2 is not supported yet. */
			) {
		return ret;
	}

	fw_name =  vpu_fw_name[ip_idx];

	ret = request_firmware_direct(&fw, fw_name, dev);

	if (ret != 0) {
		dev_err(dev, "Failed to load firmware: %d\n", ret);
	} else {
		np = of_find_node_by_name(NULL, "video_sw");
		if (np != NULL) {
			rmem = of_reserved_mem_lookup(np);

			if (!rmem) {
				dev_err(dev, "Reserved memory not found\n");
				ret = -EINVAL;
			}
		} else {
			dev_err(dev, "Failed to find a device tree node named \"vpu_sw\".\n");
			ret = -EINVAL;
		}

		if ((np != NULL)
				&& (rmem != NULL)
				&& (ret == 0)
				&& (fw->size <= rmem->size)) {
			phys_addr_t startFWAddr = (rmem->base + rmem->size - VPU_FW_SIZE);
			*fw_addr = (startFWAddr + (512*1024*ip_idx));
			dst = vetc_ioremap(*fw_addr, fw->size);
		} else {
			dev_err(dev, "Firmware too large for reserved memory\n");
			ret = -EFBIG;
		}

		if ((ret == 0) && (dst != NULL)) {
			V_DBG(VPU_DBG_DETAIL, "vetc_memcpy called fw_addr[%d] 0x%x, fw size: %d(0x%x) rmem base 0x%x",
					ip_idx, *fw_addr, fw->size, fw->size, rmem->base);

			(void)vetc_memcpy(dst, fw->data, fw->size, 2); // Copy to I/O
		} else {
			dev_err(dev, "Failed to ioremap reserved memory\n");
			ret = -ENOMEM;
		}

		if (dst != NULL) {
			vetc_iounmap(dst);
		}

		release_firmware(fw);
	}

	return ret;

}
EXPORT_SYMBOL(vetc_prepare_firmware);
