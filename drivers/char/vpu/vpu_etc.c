// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_etc.h"
#include <linux/io.h>
#include <linux/version.h>
//#include <asm/io.h>
#include <linux/delay.h>
#include <soc/telechips/chipinfo.h>
#include "vpu_comm.h"
#include "vpu_buffer.h"

#define vpu_writel writel
#define vpu_readl readl

#ifdef CONFIG_VPU_TIME_MEASUREMENT
long long vetc_GetTimediff_us(long long time1, long long time2)
{
	long long time_diff_us = 0;

	time_diff_us = time1 - time2;

	return time_diff_us;
}
EXPORT_SYMBOL(vetc_GetTimediff_us);


long long vetc_GetKtime(void)
{
	long long retTime = 0LL;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	struct timeval currTime;

	do_gettimeofday(&currTime);
	retTime = (long long) ((currTime.tv_sec * 1000) + (currTime.tv_usec / 1000));
#else
	retTime = ktime_to_us(ktime_get());
#endif

	return retTime;
}
EXPORT_SYMBOL(vetc_GetKtime);

void printMeasurementTime(void *pHandle, vputype type, int isDec, long long time_gap_us)
{
	struct mgr_data_t* mgr_data = (struct mgr_data_t *) pHandle;
	struct TimeInfo *iTime = &mgr_data->iTime[type];

	unsigned int proc_base_cnt;
	unsigned int accumulated_proc_time;
	unsigned int accumulated_frame_cnt;
	unsigned int proc_time_30frames;

	proc_base_cnt = iTime->proc_base_cnt;

	iTime->accumulated_frame_cnt++;
	iTime->proc_time[proc_base_cnt] = time_gap_us;
	iTime->proc_time_30frames += time_gap_us;
	iTime->accumulated_proc_time += time_gap_us;

	accumulated_proc_time = iTime->accumulated_proc_time;
	accumulated_frame_cnt = iTime->accumulated_frame_cnt;
	proc_time_30frames = iTime->proc_time_30frames;

	if (proc_base_cnt != 0
			&& proc_base_cnt % 29 == 0) {
		V_DBG(VPU_DBG_PERF,
				"Type[%2u] [%s] Cnt[%4u] Avr. us (Cur:%2u.%02u / T:%2u.%02u): "
				"%2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, "
				"%2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, "
				"%2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u, %2u",
				type,
				(isDec == 1) ? "DEC" : "ENC",
				iTime->print_out_index,
				proc_time_30frames/30,
				((proc_time_30frames % 30) * 100) / 30,
				accumulated_proc_time
				/accumulated_frame_cnt,
				((accumulated_proc_time % accumulated_frame_cnt)*100) / accumulated_frame_cnt,
				iTime->proc_time[0], iTime->proc_time[1], iTime->proc_time[2],
				iTime->proc_time[3], iTime->proc_time[4], iTime->proc_time[5],
				iTime->proc_time[6], iTime->proc_time[7], iTime->proc_time[8],
				iTime->proc_time[9], iTime->proc_time[10],iTime->proc_time[11],
				iTime->proc_time[12],iTime->proc_time[13],iTime->proc_time[14],
				iTime->proc_time[15],iTime->proc_time[16],iTime->proc_time[17],
				iTime->proc_time[18],iTime->proc_time[19],iTime->proc_time[20],
				iTime->proc_time[21],iTime->proc_time[22],iTime->proc_time[23],
				iTime->proc_time[24],iTime->proc_time[25],iTime->proc_time[26],
				iTime->proc_time[27],iTime->proc_time[28],iTime->proc_time[29]);

		iTime->proc_base_cnt = 0;
		iTime->proc_time_30frames = 0;
		iTime->print_out_index++;
	} else {
		iTime->proc_base_cnt++;
	}
}
EXPORT_SYMBOL(printMeasurementTime);
#endif

unsigned int vetc_reg_read(void *base_addr, unsigned int offset)
{
	return vpu_readl((base_addr + offset));
}
EXPORT_SYMBOL(vetc_reg_read);

void vetc_reg_write(void *base_addr, unsigned int offset, unsigned int datas)
{
	vpu_writel(datas, (base_addr + offset));
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
	LOG_COVERITY("%p,%p", base_addr, str);
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

void *vetc_ioremap(phys_addr_t phy_addr, unsigned int size)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,10,0)
	return ioremap(phy_addr, size);
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

	if (vmem_is_cma_allocated_cv_virt_region(src, count) > 0) {
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

unsigned int vetc_get_chip_name(void)
{
	unsigned int chip_name = 0;
	chip_name = (unsigned int)get_chip_name();

	return chip_name;
}
EXPORT_SYMBOL(vetc_get_chip_name);

unsigned int vetc_get_chip_family(void)
{
	unsigned int chip_family = 0;
	chip_family = (unsigned int)get_chip_family();

	return chip_family;
}
EXPORT_SYMBOL(vetc_get_chip_family);

unsigned int vetc_get_chip_rev(void)
{
	unsigned int chip_rev = 0;
	chip_rev = (unsigned int)get_chip_rev();

	return chip_rev;
}
EXPORT_SYMBOL(vetc_get_chip_rev);

int  vetc_check_ip_enabled(int ip_type)
{
	int ret = 0;
	unsigned int chip_name;
	unsigned int chip_rev;
	unsigned int chip_family;

	chip_family = vetc_get_chip_family();
	chip_name = vetc_get_chip_name();
	chip_rev = vetc_get_chip_rev();

	VPU_UNUSED_PARAMETER(ip_type);

	V_DBG(VPU_DBG_ILV_INFO, "This board chip name is TCC0x%04x(rev:0x%02x, 0x%04x)",
			chip_name, chip_rev, chip_family);

#if defined(CONFIG_ARCH_TCC750X)
	if ((ip_type == (int)vip_wave420l)
			&& (chip_name == 0x7509U)) {  /*replaced from system_rev*/
		V_DBG(VPU_DBG_ERROR, "TCC0x%04x is not support HEVC ENCODER IP.", chip_name);
		ret = -ENODEV;
	}
#endif

#if defined(CONFIG_ARCH_TCC803X)
	if ((ip_type == (int)vip_wave410)
			&& ((chip_name == 0x8035U) || (chip_name == 0x8036U))) {  /*replaced from system_rev*/
		V_DBG(VPU_DBG_ERROR, "TCC0x%04x is not support HEVC Decoder IP.", chip_name);
		ret = -ENODEV;
	}
#endif

	return ret;
}
EXPORT_SYMBOL(vetc_check_ip_enabled);

void vetc_vm_flags_set(struct vm_area_struct *vma, vm_flags_t flags) {
#ifdef __ANDROID_COMMON_KERNEL__
		vm_flags_set(vma, flags);
#else
		vma->vm_flags |= flags;
#endif
}
