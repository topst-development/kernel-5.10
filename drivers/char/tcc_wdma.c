// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) Telechips Inc.
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/dma-mapping.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/platform_device.h>
#include <linux/clk.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/limits.h>
#include <linux/uaccess.h>

#include <asm/div64.h>

#ifdef CONFIG_OF
#include <linux/of.h>
#include <linux/of_device.h>
#include <linux/of_gpio.h>
#include <linux/of_address.h>
#include <linux/of_irq.h>
#endif

#include <video/telechips/vioc_rdma.h>
#include <video/telechips/vioc_wdma.h>
#include <video/telechips/vioc_wmix.h>
#include <video/telechips/vioc_config.h>
#include <video/telechips/vioc_intr.h>
#include <video/telechips/vioc_global.h>
#include <video/telechips/vioc_scaler.h>
#include <video/telechips/vioc_wmix.h>
#include <video/telechips/vioc_disp.h>
#include <video/telechips/tcc_wdma_ioctrl.h>
#include <video/telechips/tcc_types.h>
#include <video/telechips/tcc_overlay_ioctl.h>

#include <linux/sched/xacct.h>
#include <linux/uio.h>
#include <linux/fsnotify.h>

#define TCC_WDMA_DRIVER_DATE	"20230207"
#define TCC_WDMA_DRIVER_MAJOR	2
#define TCC_WDMA_DRIVER_MINOR	1
#define TCC_WDMA_DRIVER_PATCH	0


//#define WDMA_DEBUG
#ifdef WDMA_DEBUG
#define wdma_dbg(msg, ...) (void)pr_info("[INF][WDMA][%s:%d]" msg "\n", __func__, __LINE__, ##__VA_ARGS__)
#else
#define wdma_dbg(msg...)
#endif

/*
 * NOTICE
 * ======
 *
 * Issue:
 * ------
 *    Android GKI kernel doesn't allow filp_open. (refer to symbols.deny)
 *
 * Description:
 * ------------
 *    1. WDMA driver uses filp_open because of screen_capture_store() function (sysfs interface).
 *    2. But it is a debugging function.
 *    4. Therefore, in the case of Android, this code is commented out.
 */
#ifndef CONFIG_ANDROID
#define TCC_WDMA_SYSFS_CAPTURE
#endif

/* To debug wdma image
 *  The image buffer set to 0x12 before writing the image data by WDMA block.
 */
//#define WDMA_IMAGE_DEBUG
#define CHECKING_NUM             (0x12)
#define CHECKING_START_POS(x, y) (x * (y / 3))
#define CHECKING_AREA(x, y)      (x * (y / 10))

#define NUM_OF_QUEUE_MIN	4
#define NUM_OF_QUEUE_MAX	340 // uint32_t offset / (3840 x 2160 * 3 / 2)
#define INVALID_QUEUE_INDEX	340

#if defined(CONFIG_ARCH_TCC897X)
#define WDMA_DEV_NAME		"/dev/wdma"
#else
#define WDMA_DEV_NAME		"/dev/wdma_drv@0"
#endif

#define MAX_IMAGE_NAME		60

enum {
	TO_BE_ACTIVATED_S,
	TO_BE_WRITTEN_S,
	WRITED_S,
	WDMA_STATE_MAX
};

struct wdma_buffer_list {
	uint32_t status;
	uint32_t base_Yaddr;
	uint32_t base_Uaddr;
	uint32_t base_Vaddr;
	char *vbase_Yaddr;
};

struct wdma_queue_list {
	uint32_t q_max_cnt;
	uint32_t q_index;//curreunt writing buffer index
	struct wdma_buffer_list *wbuf_list;
	struct vioc_wdma_frame_info *dst_frame_info;
};

struct tcc_wdma_dev_vioc {
	void __iomem *reg;
	unsigned int id;
	//unsigned int path;
};

struct tcc_wdma_vioc_block {
	void __iomem *reg; // virtual address
	unsigned int irq_num;
	unsigned int blk_num; //block number like dma number or mixer number
};

struct tcc_wdma_dev {
	struct vioc_intr_type *vioc_intr;
	unsigned int irq;

	struct miscdevice *misc;
	struct tcc_wdma_dev_vioc disp;
	struct tcc_wdma_dev_vioc rdma;
	struct tcc_wdma_dev_vioc sc;
	struct tcc_wdma_dev_vioc wdma;
	struct tcc_wdma_dev_vioc wmix;

	// wait for poll
	wait_queue_head_t poll_wq;
	spinlock_t poll_lock;

	// wait for ioctl command
	wait_queue_head_t cmd_wq;
	spinlock_t cmd_lock;

	struct mutex io_mutex;
	unsigned char block_operating;
	unsigned char block_waiting;
	unsigned char irq_reged;
	unsigned int dev_opened;

	unsigned char wdma_continuous;
	struct wdma_queue_list frame_list;
	struct clk *wdma_clk;
};


static irqreturn_t tcc_wdma_intr_handler(int irq, void *client_data);


static inline long tcc_wdma_msecs_to_jiffies(int msec)
{
	unsigned long ulmsecs;
	long lmsec;

	if (msec < 0) {
		lmsec = 0;
	} else {
		ulmsecs = msecs_to_jiffies((unsigned int)msec);

		if (ulmsecs > (ULONG_MAX >> 1)) {
			ulmsecs = (ULONG_MAX >> 1);
			lmsec = (long)ulmsecs;
		} else {
			lmsec = (long)ulmsecs;
		}
	}

	return lmsec;
}

static uint32_t tcc_wdma_get_frame_size(uint32_t uimg_format, uint32_t uiwidth, uint32_t uiheight)
{
	unsigned int uiframe_size = 0;

	switch ((TCC_LCDC_IMG_FMT_TYPE)uimg_format) {
		case TCC_LCDC_IMG_FMT_1BPP:
		case TCC_LCDC_IMG_FMT_2BPP:
		case TCC_LCDC_IMG_FMT_4BPP:
		case TCC_LCDC_IMG_FMT_8BPP:
			uiframe_size = (uiwidth * uiheight);
			break;
		case TCC_LCDC_IMG_FMT_RGB332:
			uiframe_size = ((uiwidth * uiheight) * 2);
			break;
		case TCC_LCDC_IMG_FMT_RGB444:
		case TCC_LCDC_IMG_FMT_RGB565:
		case TCC_LCDC_IMG_FMT_RGB555:
			uiframe_size = ((uiwidth * uiheight) * 3);
			break;
		case TCC_LCDC_IMG_FMT_RGB888:
		case TCC_LCDC_IMG_FMT_RGB666:
		case TCC_LCDC_IMG_FMT_ARGB6666_3:
			uiframe_size = ((uiwidth * uiheight) * 4);
			break;
		case TCC_LCDC_IMG_FMT_444SEP:
		case TCC_LCDC_IMG_FMT_UYVY:
		case TCC_LCDC_IMG_FMT_VYUY:
			uiframe_size = ((uiwidth * uiheight) * 4);
			break;
		case TCC_LCDC_IMG_FMT_YUV420SP:
		case TCC_LCDC_IMG_FMT_YUV420ITL0:
		case TCC_LCDC_IMG_FMT_YUV420ITL1:
			uiframe_size = (((uiwidth * 3 ) / 2) * uiheight);
			break;
		case TCC_LCDC_IMG_FMT_YUV422SP:
		case TCC_LCDC_IMG_FMT_YUV422ITL0:
		case TCC_LCDC_IMG_FMT_YUV422ITL1:
			uiframe_size = ((uiwidth * uiheight) * 2);
			break;
		default:
			uiframe_size = ((uiwidth * uiheight) * 4);
			break;
	}

	pr_info("Format: %d -> frame size: %u\n", uimg_format, uiframe_size);

	return uiframe_size;
}


static int32_t tcc_wdma_get_writable_buffer_idx(uint32_t *pidx,
																const struct wdma_queue_list *frame_list)
{
	int32_t ret = 0;
	uint32_t idx, curidx = 0;

	if (frame_list->q_index >= frame_list->q_max_cnt) {
		(void)pr_err("[ERR][WDMA][%d]Invalid idx as %u\n", __LINE__, frame_list->q_index);

		ret = -EINVAL;

		goto return_funcs;
	}

	curidx = frame_list->q_index;

	for (idx = 0U; idx < frame_list->q_max_cnt; idx++) {
		curidx++;
		if (curidx >= frame_list->q_max_cnt) {
			/* For KCS */
			curidx = 0;
		}

		if ((frame_list->wbuf_list[curidx].status == (uint32_t)TO_BE_ACTIVATED_S)
			|| (frame_list->wbuf_list[curidx].status == (uint32_t)WRITED_S)) {
			/* For KCS */
			break;
		}
	}

	*pidx = curidx;

	if (idx == frame_list->q_max_cnt) {
		(void)pr_err("[ERR][WDMA][%d]can't get idx\n", __LINE__);

		ret = -ENXIO;
	}

return_funcs:
	return ret;
}

static int32_t tcc_wdma_queue_get_idx_of_state(uint32_t *pidx,
																uint32_t wdma_state,
																const struct wdma_queue_list *frame_list)
{
	int32_t ret = 0;
	uint32_t idx, find_index = 0;

	if (wdma_state >= (uint32_t)WDMA_STATE_MAX){
		(void)pr_err("[ERR][WDMA][%d]Invalid state as %u\n", __LINE__, wdma_state);

		ret = -EINVAL;

		goto return_funcs;
	}

	if (wdma_state == (uint32_t)WRITED_S) {
		if (frame_list->q_index == 0U) {
			/* For KCS */
			find_index = (frame_list->q_max_cnt - 2U);
		} else if (frame_list->q_index == 1U) {
			/* For KCS */
			find_index = (frame_list->q_max_cnt - 1U);
		} else {
			/* For KCS */
			find_index = (frame_list->q_index - 2U);
		}

		if (frame_list->wbuf_list[find_index].status == (uint32_t)WRITED_S) {
			*pidx = find_index;

			goto return_funcs;
		}
	}

	for (idx = 0U; idx < frame_list->q_max_cnt; idx++) {
		if (frame_list->wbuf_list[idx].status == wdma_state) {
			/* For KCS */
			break;
		}
	}

	if (idx == frame_list->q_max_cnt) {
		(void)pr_err("[ERR][WDMA][%d]couldn't find buf idx\n", __LINE__);

		ret = -ENXIO;
	}

	*pidx = idx;

return_funcs:
	return ret;
}

static int32_t tcc_wdma_queue_set_state(struct wdma_queue_list *frame_list,
												uint32_t listidx,
												uint32_t wdma_state)
{
	int ret = 0;

	if (wdma_state >= (uint32_t)WDMA_STATE_MAX){
		(void)pr_err("[ERR][WDMA][%d]Invalid state as %u\n", __LINE__, wdma_state);

		ret = -EINVAL;

		goto return_funcs;
	}

	if (listidx >= (uint32_t)NUM_OF_QUEUE_MAX){
		(void)pr_err("[ERR][WDMA][%d]Invalid idx as %u\n", __LINE__, listidx);

		ret = -EINVAL;

		goto return_funcs;
	}

	frame_list->wbuf_list[listidx].status = wdma_state;
	if (wdma_state == (unsigned int)TO_BE_WRITTEN_S) {
		/* For KCS */
		frame_list->q_index = listidx;
	}

return_funcs:
	return ret;
}

static int32_t tcc_wdma_queue_config_list_members(uint32_t frame_size,
																struct wdma_queue_list *pframe_list,
																struct vioc_wdma_frame_info *pframe_infor)
{
	int32_t ret = 0;

	if(frame_size == 0U) {
		ret = -EINVAL;

		goto return_funcs;
	}

	pframe_list->q_max_cnt = (pframe_infor->buff_size / frame_size);

	if (pframe_list->q_max_cnt   < (uint32_t)NUM_OF_QUEUE_MIN) {
		(void)pr_err("[ERR][WDMA][%s]Buf underflow -> it has to be more than 4\n", __func__);

		ret = -EINVAL;

		goto return_funcs;
	}
		
	if ( pframe_list->q_max_cnt > (uint32_t)NUM_OF_QUEUE_MAX) {
		(void)pr_warn("[WARN][WDMA][%s]Buf overflow -> set to %d\n", __func__, NUM_OF_QUEUE_MAX);

		pframe_list->q_max_cnt = (uint32_t)NUM_OF_QUEUE_MAX;
	}

	pframe_list->dst_frame_info = kmalloc(sizeof(struct vioc_wdma_frame_info), GFP_KERNEL);

	if (pframe_list->dst_frame_info == NULL) {
		(void)pr_err("[ERR][WDMA][%s]queue list kmalloc() failed!\n", __func__);

		ret = -ENOMEM;

		goto return_funcs;
	}

	*pframe_list->dst_frame_info = *pframe_infor;
	pframe_infor->buffer_num = pframe_list->q_max_cnt;

	pframe_list->dst_frame_info->buffer_num = pframe_list->q_max_cnt;
	pframe_list->q_index = (pframe_list->q_max_cnt - 1U);

	pframe_list->wbuf_list = kmalloc_array(pframe_list->q_max_cnt,
									sizeof(struct wdma_buffer_list),
									GFP_KERNEL);

	if (pframe_list->wbuf_list == NULL) {
		(void)pr_err("[ERR][WDMA][%s]list alloc error\n", __func__);

		kfree(pframe_list->dst_frame_info);

		ret = -ENOMEM;

		goto return_funcs;
	}

return_funcs:
	return ret;
}

static int32_t wdma_queue_list_exit(const struct wdma_queue_list *frame_list)
{
	if (frame_list->wbuf_list != NULL) {
		/* For KCS */
		kfree(frame_list->wbuf_list);
	}

	if (frame_list->dst_frame_info != NULL) {
		/* For KCS */
		kfree(frame_list->dst_frame_info);
	}

	return 0;
}

static int32_t wdma_queue_list_init(struct wdma_queue_list *frame_list,
												struct vioc_wdma_frame_info *pframe_info)
{
	uint8_t frame_fmt;
	int32_t ret = 0;
	uint32_t idx, frame_size, buf_size;
	uint32_t addrY = 0, addrU = 0, addrV = 0;
	uint32_t base_addr, offset;

	frame_size = tcc_wdma_get_frame_size(pframe_info->frame_fmt, pframe_info->frame_x, pframe_info->frame_y);

	buf_size = (pframe_info->buff_size / 4);

	if (frame_size > buf_size) {
		(void)pr_err("[ERR][WDMA][%s]Frame size %u is larger than %u\n", __func__, frame_size, buf_size);

		ret = -EPERM;

		goto return_funcs;
	}

	ret = tcc_wdma_queue_config_list_members(frame_size, frame_list, pframe_info);
	if (ret != 0) {
		goto return_funcs;
	}

	for (idx = 0; idx < frame_list->q_max_cnt; idx++) {
		frame_list->wbuf_list[idx].status = TO_BE_ACTIVATED_S;

		if ((UINT_MAX / frame_size) < idx){
			(void)pr_warn("[WAR][WDMA][%s]Buf %d : offset overflow\n", __func__, idx);
			continue;
		}

		offset = (idx * frame_size);

		if ((UINT_MAX - pframe_info->buff_addr) < offset) {
			(void)pr_warn("[WAR][WDMA][%s]Buf %d : buf address overflow\n", __func__, idx);
			continue;
		}

		base_addr = pframe_info->buff_addr + offset;

		if (pframe_info->frame_fmt >= (unsigned int)TCC_LCDC_IMG_FMT_MAX) {
			(void)pr_warn("[WAR][WDMA][%s]Buf %d : invalid frame format as %u\n",
							__func__,
							idx,
							pframe_info->frame_fmt);
			continue;
		}

		frame_fmt = (unsigned char)pframe_info->frame_fmt;

		addrY = 0U;
		addrU = 0U;
		addrV = 0U;

		tcc_get_addr_yuv(frame_fmt,
							base_addr,
							pframe_info->frame_x,
							pframe_info->frame_y,
							0,
							0,
							&addrY,
							&addrU,
							&addrV);

		frame_list->wbuf_list[idx].base_Yaddr = addrY;
		frame_list->wbuf_list[idx].base_Uaddr = addrU;
		frame_list->wbuf_list[idx].base_Vaddr = addrV;

		wdma_dbg("Buf %d : Y(0x%08x), U(0x%08x), V(0x%08x)\n",
					idx,
					frame_list->wbuf_list[idx].base_Yaddr,
					frame_list->wbuf_list[idx].base_Uaddr,
					frame_list->wbuf_list[idx].base_Vaddr);
	}

	wdma_dbg("Start list :%d x %d, Num Of buffers : %d\n",
				frame_list->dst_frame_info->frame_y,
				frame_list->dst_frame_info->frame_x,
				frame_list->dst_frame_info->buffer_num);

return_funcs:
	return ret;
}

static int32_t tcc_wdma_check_intr_params(int32_t *pirq_num,
															int32_t *pirq_id,
															const struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;

	if (pwdma_data->irq > (UINT_MAX / 2U)) {
		(void)pr_err("[ERR][WDMA][%s]invalid irq Num. as %u\n", __func__, pwdma_data->irq);

		ret = -ENXIO;

		goto return_funcs;
	}

	if (pwdma_data->vioc_intr->id > (UINT_MAX / 2U)) {
		(void)pr_err("[ERR][WDMA][%s]invalid irq id as %u\n", __func__, pwdma_data->vioc_intr->id);

		ret = -ENXIO;

		goto return_funcs;
	}

	*pirq_num = (int)pwdma_data->irq;
	*pirq_id = (int)pwdma_data->vioc_intr->id;

return_funcs:
	return ret;
}

static int32_t tcc_wdma_wait_intrruptible(int32_t msec,
														struct tcc_wdma_dev *pwdma_data)
{
	int32_t int_ret = 0;
	int64_t num_of_jiffies;

	num_of_jiffies = tcc_wdma_msecs_to_jiffies(msec);

	int_ret = wait_event_interruptible_timeout(pwdma_data->cmd_wq,
												(pwdma_data->block_operating == 0U),
												num_of_jiffies);
	if (int_ret <= 0) {
		(void)pr_info("[INF][WDMA][%s]timed out(%ums) : block_operation(%d) - block_waiting(%d)\n",
						__func__,
						msec,
						pwdma_data->block_operating,
						pwdma_data->block_waiting);
	}

	return int_ret;
}

static void tcc_wdma_reenable_intr(int32_t iirq_id,
												int32_t iirq_num,
												const struct tcc_wdma_dev *pwdma_data)
{
	(void)vioc_intr_clear(iirq_id, pwdma_data->vioc_intr->bits);
	(void)vioc_intr_enable(iirq_num, iirq_id, pwdma_data->vioc_intr->bits);
}

static int32_t tcc_wdma_reset_intr(bool one_frame_cap,
												int32_t msec,
												struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;
	int32_t irq_num = 0, irq_id = 0;
	int64_t num_of_jiffies;

	ret = tcc_wdma_check_intr_params(&irq_num, &irq_id, pwdma_data);
	if (ret != 0) {
		goto return_funcs;
	}

	pwdma_data->vioc_intr->bits = (1U << (unsigned int)VIOC_WDMA_INTR_EOFR);

	tcc_wdma_reenable_intr(irq_id, irq_num, pwdma_data);

	if (one_frame_cap) {
		num_of_jiffies = tcc_wdma_msecs_to_jiffies(msec);

		ret = wait_event_interruptible_timeout(pwdma_data->poll_wq,
														(pwdma_data->block_operating == 0U),
														num_of_jiffies);
		if (ret <= 0) {
			/* For KCS */
			ret = -EINTR;
		}
	}

return_funcs:
	return ret;
}

static int32_t tcc_wdma_deinit_intr(const struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;
	int32_t irq_num = 0, irq_id = 0;

	ret = tcc_wdma_check_intr_params(&irq_num, &irq_id, pwdma_data);
	if (ret != 0) {
		goto return_funcs;
	}

	pwdma_data->vioc_intr->bits = (1U << (unsigned int)VIOC_WDMA_INTR_EOFR);

	(void)vioc_intr_clear(irq_id, pwdma_data->vioc_intr->bits);
	(void)vioc_intr_disable(irq_num, irq_id, pwdma_data->vioc_intr->bits);

return_funcs:
	return ret;

}

static int32_t tcc_wdma_init_intr(struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;
	int32_t irq_num = 0, irq_id = 0;

	ret = tcc_wdma_check_intr_params(&irq_num, &irq_id, pwdma_data);
	if (ret != 0) {
		goto return_funcs;
	}

	pwdma_data->vioc_intr->bits = (1U << (unsigned int)VIOC_WDMA_INTR_EOFR);

	tcc_wdma_reenable_intr(irq_id, irq_num, pwdma_data);

	ret = request_irq((unsigned int)irq_num,
						tcc_wdma_intr_handler,
						IRQF_SHARED,
						"wdma",
						pwdma_data);
	if (ret != 0){
		/* For KCS */
		(void)pr_err("[ERR][WDMA][%s]failed to aquire irq\n", __func__);
	}

return_funcs:
	return ret;
}

static int32_t tcc_wdma_config_wdma(uint32_t output_w,
												 uint32_t output_h,
												 uint32_t img_format,
												 const uint32_t abase_addr[3],
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
												 uint32_t contrast,
												 uint32_t bright,
												 uint32_t hue,
#endif
												 const struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;

	if (pwdma_data->wdma.reg != NULL ) {
		 VIOC_WDMA_SetImageSize(pwdma_data->wdma.reg, output_w, output_h);
		 VIOC_WDMA_SetImageFormat(pwdma_data->wdma.reg, img_format);
		 VIOC_WDMA_SetImageOffset(pwdma_data->wdma.reg, img_format, output_w);
		 VIOC_WDMA_SetImageBase(pwdma_data->wdma.reg,
									 abase_addr[0],
									 abase_addr[1],
									 abase_addr[2]);
	 
		 VIOC_WDMA_SetImageRGBSwapMode(pwdma_data->wdma.reg, 0);

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		if (pwdma_data->wdma_continuous == 0U) {
			VIOC_WDMA_SetImageEnhancer(pwdma_data->wdma.reg, contrast, bright, hue);
		}
#endif

		 VIOC_WDMA_SetImageEnable(pwdma_data->wdma.reg, pwdma_data->wdma_continuous);
	}

	 return ret;
}

static int32_t tcc_wdma_config_wdma_img(const struct tcc_wdma_dev *pwdma_data,
														uint32_t dst_img_format)
{
	int32_t ret = 0;
	uint32_t y2r_mode = 0x2, y2r_en = 0, r2y_mode = 0;
	uint32_t dd_rgb = 0;
	struct DisplayBlock_Info DDinfo;

	DDinfo.pCtrlParam.pxdw = 0U;

	if (pwdma_data->disp.reg != NULL) {
		VIOC_DISP_GetDisplayBlock_Info(pwdma_data->disp.reg, &DDinfo);
	}

	dd_rgb = VIOC_DISP_FMT_isRGB(DDinfo.pCtrlParam.pxdw);

	if (dst_img_format <= (unsigned int)TCC_LCDC_IMG_FMT_ARGB6666_3) {
		if ((bool)dd_rgb == false) {
			/* For KCS */
			y2r_en = 1U;
		}
	} else {
		if ((bool)dd_rgb) {
			/* For KCS */
			r2y_mode = 1U;
		}
	}

	if (pwdma_data->wdma.reg != NULL ) {
		VIOC_WDMA_SetImageY2RMode(pwdma_data->wdma.reg, y2r_mode);
		VIOC_WDMA_SetImageY2REnable(pwdma_data->wdma.reg, y2r_en);
		VIOC_WDMA_SetImageR2YEnable(pwdma_data->wdma.reg, r2y_mode);
	}

	return ret;
}

static int32_t tcc_wdma_config_wmix(const struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;

#if defined(CONFIG_ARCH_TCC750X)
	(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA00, 1 /* Mixing */);
	(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA03, 1 /* Mixing */);
#else
	if (get_vioc_index(pwdma_data->disp.id) == 0U) {
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA00, 1 /* Mixing */);
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA03, 1 /* Mixing */);
	} else {
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA04, 1 /* Mixing */);
		(void)VIOC_CONFIG_WMIXPath(VIOC_RDMA07, 1 /* Mixing */);
	}
#endif

	return ret;
}

static int32_t tcc_wdma_release_sc(const struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;

	if (pwdma_data->sc.reg != NULL) {
		/* KCS */
		(void)VIOC_CONFIG_PlugOut(pwdma_data->sc.id);
	}

	return ret;
}

static int32_t tcc_wdma_config_sc_bypass(uint32_t out_w,
														uint32_t out_h,
														const struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;
	uint32_t sc_bypass = 0;
	uint32_t wmix_h = 0, wmix_w = 0;

	if (pwdma_data->wmix.reg != NULL) {
		VIOC_WMIX_GetSize(pwdma_data->wmix.reg, &wmix_w, &wmix_h);
	}

	if (pwdma_data->sc.reg == NULL) {
		(void)pr_warn("[WARN][WDMA][%s]SC reg address isn't registered\n", __func__);

		if ((wmix_w != out_w) || (wmix_h != out_h)) {
			(void)pr_err("[ERR][WDMA][%s]SC isn't available -> Src %u x %u <-> Dst %u x %u\n",
								__func__,
								wmix_w,
								wmix_h,
								out_w,
								out_h);

			ret = -EFAULT;
		}

		goto return_funcs;
	}

	if ((wmix_w == out_w) && (wmix_h == out_h)) {
		(void)pr_info("[INF][WDMA][%s]Scaler bypass -> %u x %u\n", __func__, wmix_w, wmix_h);

		sc_bypass = 1U;
	}

	VIOC_SC_SetBypass(pwdma_data->sc.reg, sc_bypass);

return_funcs:
	return ret;
}

static int32_t tcc_wdma_config_sc(uint32_t out_w,
											uint32_t out_h,
											const struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;

	(void)tcc_wdma_config_sc_bypass(out_w, out_h, pwdma_data);

	if (pwdma_data->sc.reg != NULL) {
		VIOC_SC_SetDstSize(pwdma_data->sc.reg, out_w, out_h);
		VIOC_SC_SetOutSize(pwdma_data->sc.reg, out_w, out_h);
		VIOC_SC_SetOutPosition(pwdma_data->sc.reg, 0, 0);
		(void)VIOC_CONFIG_PlugIn(pwdma_data->sc.id, pwdma_data->wdma.id);
		VIOC_SC_SetUpdate(pwdma_data->sc.reg);
	}

	return ret;
}

static int32_t tcc_wdma_config_vioc_comp(uint32_t output_w,
														 uint32_t output_h,
														 uint32_t image_format,
														 const uint32_t base_addr[3],
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
														 uint32_t contrast,
														 uint32_t brightness,
														 uint32_t hue,
#endif
														 struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&(pwdma_data->cmd_lock), flags);

	(void)tcc_wdma_config_sc(output_w, output_h, pwdma_data);

	(void)tcc_wdma_config_wmix(pwdma_data);

	(void)tcc_wdma_config_wdma_img(pwdma_data, image_format);
	(void)tcc_wdma_config_wdma(output_w, output_h, image_format, base_addr,
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
								contrast, brightness, 	hue,
#endif
								pwdma_data);

	 spin_unlock_irqrestore(&(pwdma_data->cmd_lock), flags);

	 return ret;
}

 static int32_t tcc_wdma_config_count_start(struct tcc_wdma_dev *pwdma_data,
														 struct vioc_wdma_frame_info *cap_frame_info)
{
	int32_t ret = 0;
	uint32_t aBaseAddr[3];

	if ((cap_frame_info->frame_x == 0U) || (cap_frame_info->frame_y == 0U)) {
		(void)pr_err("ERR][WDMA][%s]size error as %d x %d\n",
						__func__,
						cap_frame_info->frame_x,
						cap_frame_info->frame_y);
		ret = -EINVAL;
	
		goto return_funcs;
	}

	ret = wdma_queue_list_init(&pwdma_data->frame_list, cap_frame_info);
	if (ret != 0) {
		goto return_funcs;
	}

	pwdma_data->wdma_continuous = 1;
	pwdma_data->frame_list.wbuf_list[0].status = TO_BE_WRITTEN_S;

	aBaseAddr[0] = pwdma_data->frame_list.wbuf_list[0].base_Yaddr;
	aBaseAddr[1] = pwdma_data->frame_list.wbuf_list[0].base_Uaddr;
	aBaseAddr[2] = pwdma_data->frame_list.wbuf_list[0].base_Vaddr;

	ret = tcc_wdma_config_vioc_comp(cap_frame_info->frame_x,
										cap_frame_info->frame_y,
										cap_frame_info->frame_fmt,
										aBaseAddr,
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
										0,
										0,
										0,
#endif
										pwdma_data);
	if (ret != 0) {
		goto return_funcs;
	}

	(void)tcc_wdma_reset_intr((bool)false, 0, pwdma_data);

return_funcs:
	return ret;
}

static int32_t tcc_wdma_get_yuv_addr(dma_addr_t dst_img_phy_addr,
													const struct tcc_wdma_dev *pwdma_data,
													struct VIOC_WDMA_IMAGE_INFO_Type *oneframe_image_infor)
{
	int32_t ret = 0;
	uint32_t DDevice = 0U;
	uint32_t addr_Y = 0U, addr_U = 0U, addr_V = 0U;
	uint32_t Wmix_Height = 0U, Wmix_Width = 0U;
	uint32_t img_phy_addr;

	if (pwdma_data->disp.reg != NULL) {
		/* KCS */
		DDevice = VIOC_DISP_Get_TurnOnOff(pwdma_data->disp.reg);
	}

	if (DDevice == 0U) {
		(void)pr_err("[ERR][WDMA][%s]Display turn off\n", __func__);

		ret = -ENXIO;

		goto return_funcs;
	}

	if (pwdma_data->wmix.reg != NULL) {
		/* KCS */
		VIOC_WMIX_GetSize(pwdma_data->wmix.reg, &Wmix_Width, &Wmix_Height);
	}

	if ((Wmix_Width == 0U) || (Wmix_Height == 0U)) {
		(void)pr_err("[ERR][WDMA][%s] W:%d H:%d \n", __func__, Wmix_Width, Wmix_Height);

		ret = -ENXIO;

		goto return_funcs;
	}

	oneframe_image_infor->ImgSizeWidth = Wmix_Width;
	oneframe_image_infor->ImgSizeHeight = Wmix_Height;

	img_phy_addr = (uint32_t)(dst_img_phy_addr & UINT_MAX);

	tcc_get_addr_yuv(oneframe_image_infor->ImgFormat,
						img_phy_addr,
						oneframe_image_infor->TargetWidth,
						oneframe_image_infor->TargetHeight,
						0,
						0,
						&addr_Y,
						&addr_U,
						&addr_V);

	if ((oneframe_image_infor->ImgFormat == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) ||
		(oneframe_image_infor->ImgFormat == (unsigned int)TCC_LCDC_IMG_FMT_RGB888)) {
		addr_U = GET_ADDR_YUV42X_spU(img_phy_addr,
										oneframe_image_infor->TargetWidth,
										oneframe_image_infor->TargetHeight);

		if (oneframe_image_infor->ImgFormat == (unsigned int)TCC_LCDC_IMG_FMT_YUV420SP) {
			addr_V = GET_ADDR_YUV420_spV(addr_U,
											oneframe_image_infor->TargetWidth,
											oneframe_image_infor->TargetHeight);
		} else {
			addr_V = GET_ADDR_YUV422_spV(addr_U,
											oneframe_image_infor->TargetWidth,
											oneframe_image_infor->TargetHeight);
		}
	}

	oneframe_image_infor->BaseAddress = addr_Y;
	oneframe_image_infor->BaseAddress1 = addr_U;
	oneframe_image_infor->BaseAddress2 = addr_V;

	wdma_dbg("ImgFormat:%d\n",oneframe_image_infor->ImgFormat);

#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
	wdma_dbg("Set effect->hue:%d, bright:%d contrast:%d\n",
				oneframe_image_infor->Hue,
				oneframe_image_infor->Bright,
				oneframe_image_infor->Contrast);
#endif

	wdma_dbg("src %d x %d <-> dst %d x %d\n",
				oneframe_image_infor->ImgSizeWidth,
				oneframe_image_infor->ImgSizeHeight,
				oneframe_image_infor->TargetWidth,
				oneframe_image_infor->TargetHeight);
	wdma_dbg("base0:0x%08x base1:0x%08x base2:0x%08x\n",
				oneframe_image_infor->BaseAddress,
				oneframe_image_infor->BaseAddress1,
				oneframe_image_infor->BaseAddress2);

return_funcs:
	return ret;
}

static int32_t tcc_wdma_get_target_img_infor(unsigned long vparg,
													const struct tcc_wdma_dev *pwdma_data,
													struct VIOC_WDMA_IMAGE_INFO_Type *cap_image_info)
{
	int32_t ret = 0;
	dma_addr_t dst_img_phy_addr;

	(void)memset((char *)cap_image_info, 0, sizeof(struct VIOC_WDMA_IMAGE_INFO_Type));

	if ((bool)copy_from_user((void *)cap_image_info,
								(const void __user *)vparg,
								sizeof(struct VIOC_WDMA_IMAGE_INFO_Type))) {
		(void)pr_err("[ERR][WDMA][%s]error from copy_from_user\n", __func__);
		ret = -EFAULT;

		goto return_funcs;
	}

	dst_img_phy_addr = (dma_addr_t)cap_image_info->BaseAddress;

	(void)tcc_wdma_get_yuv_addr(dst_img_phy_addr, pwdma_data, cap_image_info);

return_funcs:
	return ret;
}

static int32_t tcc_wdma_one_frame_capture(unsigned long vparg, struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;
	uint32_t aBaseAddr[3];
	struct VIOC_WDMA_IMAGE_INFO_Type cap_image_infor;

	ret = tcc_wdma_get_target_img_infor(vparg, pwdma_data, &cap_image_infor);
	if (ret != 0) {
		goto return_funcs;
	}

	aBaseAddr[0] = cap_image_infor.BaseAddress;
	aBaseAddr[1] = cap_image_infor.BaseAddress1;
	aBaseAddr[2] = cap_image_infor.BaseAddress2;

	ret = tcc_wdma_config_vioc_comp(cap_image_infor.TargetWidth,
										cap_image_infor.TargetHeight,
										cap_image_infor.ImgFormat,
										aBaseAddr,
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		 								cap_image_infor.Contrast,
		 								cap_image_infor.Bright,
		 								cap_image_infor.Hue,
#endif
										pwdma_data);
	if (ret != 0) {
		goto return_funcs;
	}

	(void)tcc_wdma_reset_intr((bool)true, 100, pwdma_data);

	(void)tcc_wdma_release_sc(pwdma_data);

	if ((bool)copy_to_user((void __user *)vparg, &cap_image_infor, sizeof(cap_image_infor))) {
		(void)pr_err("[ERR][WDMA][%s]error from copy_to_user\n", __func__);
		ret = -EFAULT;
	}

return_funcs:
	return ret;
}
static int32_t tcc_wdma_perform_WDMA_IOCTRL(struct tcc_wdma_dev *pwdma_data, unsigned long ularg)
{
	int ret = 0;

	mutex_lock(&pwdma_data->io_mutex);

	if ((bool)pwdma_data->block_operating) {
		pwdma_data->block_waiting = 1;

		(void)tcc_wdma_wait_intrruptible(200, pwdma_data);
	}

	pwdma_data->block_waiting = 0;
	pwdma_data->block_operating = 1;

	ret = tcc_wdma_one_frame_capture(ularg, pwdma_data);
	if (ret != 0) {
		/* For KCS */
		pwdma_data->block_operating = 0;
	}

	mutex_unlock(&pwdma_data->io_mutex);

	return ret;
}

static int32_t tcc_wdma_perform_WDRV_COUNT_START(struct tcc_wdma_dev *pwdma_data, unsigned long ularg)
{
	int32_t ret = 0;
	struct vioc_wdma_frame_info cap_frame_infor;

	mutex_lock(&pwdma_data->io_mutex);

	if ((bool)pwdma_data->block_operating) {
		pwdma_data->block_waiting = 1;

		(void)tcc_wdma_wait_intrruptible(200, pwdma_data);

		pwdma_data->block_operating = 0;
		pwdma_data->block_waiting = 0;
	}

	if ((bool)copy_from_user(&cap_frame_infor,
								(const void __user *)ularg,
								sizeof(struct vioc_wdma_frame_info))) {
		(void)pr_err("[ERR][WDMA][%s]error from copy_from_user\n", __func__);

		ret = -EFAULT;

		goto return_funcs;
	}

	ret = tcc_wdma_config_count_start(pwdma_data, &cap_frame_infor);
	if (ret != 0) {
		goto return_funcs;
	}

	if((bool)copy_to_user((struct vioc_wdma_frame_info *)ularg,
						&cap_frame_infor,
						sizeof(struct vioc_wdma_frame_info))) {
		//(void)pr_err("[ERR][WDMA][%s]error from copy_to_user\n", __func__);
		ret = -EFAULT;
	}

	mutex_unlock(&pwdma_data->io_mutex);

return_funcs:
	return ret;
}

static int32_t tcc_wdma_perform_GET_CUR_DATA(struct tcc_wdma_dev *pwdma_data, unsigned long ularg)
{
	int32_t ret = 0;
	uint32_t wdidx;
	struct vioc_wdma_get_buffer wbuffer;

	mutex_lock(&pwdma_data->io_mutex);

	(void)memset(&wbuffer, 0, sizeof(wbuffer));

	pwdma_data->block_operating = 1;
	pwdma_data->block_waiting = 1;

	(void)tcc_wdma_wait_intrruptible(50, pwdma_data);

	ret = tcc_wdma_queue_get_idx_of_state(&wdidx, WRITED_S, &pwdma_data->frame_list);
	if (ret != 0) {
		goto return_funcs;
	}

	if (wdidx > (UINT_MAX/2U)) {
		/* KCS */
		wbuffer.buff_index = 0;
	} else {
		/* KCS */
		wbuffer.buff_index = (int)wdidx;
	}

	pwdma_data->block_waiting = 0;

	if (wbuffer.buff_index >= 0) {
		wbuffer.buff_Yaddr = (unsigned int)pwdma_data->frame_list.wbuf_list[wbuffer.buff_index].base_Yaddr;
		wbuffer.buff_Uaddr = (unsigned int)pwdma_data->frame_list.wbuf_list[wbuffer.buff_index].base_Uaddr;
		wbuffer.buff_Vaddr = (unsigned int)pwdma_data->frame_list.wbuf_list[wbuffer.buff_index].base_Vaddr;
		wbuffer.frame_fmt = pwdma_data->frame_list.dst_frame_info->frame_fmt;
		wbuffer.frame_x = pwdma_data->frame_list.dst_frame_info->frame_x;
		wbuffer.frame_y = pwdma_data->frame_list.dst_frame_info->frame_y;

		wdma_dbg("Index:%d, Y:0x%08x U:0x%08x V:0x%08x, fmt:%d X:%d Y:%d\n",
				wbuffer.buff_index,
				wbuffer.buff_Yaddr,
				wbuffer.buff_Uaddr,
				wbuffer.buff_Vaddr,
				wbuffer.frame_fmt,
				wbuffer.frame_x,
				wbuffer.frame_y);
	}

	if ((bool)copy_to_user((struct vioc_wdma_get_buffer *)ularg,
							(struct vioc_wdma_get_buffer *)&wbuffer,
							sizeof(struct vioc_wdma_get_buffer))) {
		(void)pr_err("[ERR][WDMA][%s]error from copy_to_user\n", __func__);

		ret = -EFAULT;
	}

	mutex_unlock(&pwdma_data->io_mutex);
	
return_funcs:
	return ret;
}

static int32_t tcc_wdma_perform_COUNT_END(struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;

	mutex_lock(&pwdma_data->io_mutex);

	if (pwdma_data->wdma.reg != NULL ) {
		VIOC_WDMA_SetImageDisable(pwdma_data->wdma.reg);
	}

	pwdma_data->wdma_continuous = 0;

	pwdma_data->block_operating = 1;
	pwdma_data->block_waiting = 1;

	(void)tcc_wdma_wait_intrruptible(30, pwdma_data);

	pwdma_data->block_waiting = 0;
	pwdma_data->block_operating = 0;

	(void)tcc_wdma_deinit_intr(pwdma_data);

	ret = wdma_queue_list_exit(&pwdma_data->frame_list);

	mutex_unlock(&pwdma_data->io_mutex);

	return ret;
}

static int32_t tcc_wdma_update_continuous_buf(struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;
	uint32_t cur_id = 0, next_id;

	ret = tcc_wdma_queue_get_idx_of_state(&cur_id, TO_BE_WRITTEN_S, &pwdma_data->frame_list);
	if (ret != 0) {
		goto return_funcs;
	}

	ret = tcc_wdma_get_writable_buffer_idx(&next_id, &pwdma_data->frame_list);
	if (ret != 0) {
		goto return_funcs;
	}

	if (pwdma_data->wdma.reg != NULL ) {
		VIOC_WDMA_SetImageBase(pwdma_data->wdma.reg,
									pwdma_data->frame_list.wbuf_list[next_id].base_Yaddr,
									pwdma_data->frame_list.wbuf_list[next_id].base_Uaddr,
									pwdma_data->frame_list.wbuf_list[next_id].base_Vaddr);

		VIOC_WDMA_SetImageUpdate(pwdma_data->wdma.reg);
	}

	ret = tcc_wdma_queue_set_state(&pwdma_data->frame_list, next_id, TO_BE_WRITTEN_S);
	if (ret != 0) {
		goto return_funcs;
	}

	ret = tcc_wdma_queue_set_state(&pwdma_data->frame_list, cur_id, WRITED_S);

return_funcs:
	return ret;
}

static irqreturn_t tcc_wdma_intr_handler(int irq, void *client_data)
{
	int32_t ret = 0;
	int32_t irq_num = 0, irq_id = 0;
	irqreturn_t irq_ret = IRQ_HANDLED;

	struct tcc_wdma_dev *wdma_data = client_data;

	(void)irq;

	ret = tcc_wdma_check_intr_params(&irq_num, &irq_id, wdma_data);
	if (ret != 0) {
		irq_ret = IRQ_NONE;

		goto return_funcs;
	}

	if (!is_vioc_intr_activatied(irq_id, wdma_data->vioc_intr->bits)) {
		irq_ret = IRQ_NONE;

		goto return_funcs;
	}

	if (wdma_data->block_operating >= 1U) {
		/* For KCS */
		wdma_data->block_operating = 0;
	}

	wake_up_interruptible(&(wdma_data->poll_wq));

	if ((bool)wdma_data->block_waiting) {
		/* For KCS */
		wake_up_interruptible(&wdma_data->cmd_wq);
	}

	if ((bool)wdma_data->wdma_continuous) {
		ret = tcc_wdma_update_continuous_buf(wdma_data);
		if (ret != 0) {
			irq_ret = IRQ_NONE;

			goto return_funcs;
		}
	}

	(void)vioc_intr_clear(irq_id,   VIOC_WDMA_INT_MASK);

	irq_ret = IRQ_HANDLED;

return_funcs:
	return irq_ret;
}


static long tccxxx_wdma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int32_t ret = 0;

	struct miscdevice *misc = (struct miscdevice *)filp->private_data;

	struct tcc_wdma_dev *wdma_data = dev_get_drvdata(misc->parent);

	wdma_dbg("cmd(0x%x) - block_operating(0x%x) - block_waiting(0x%x)\n",
				cmd,
				wdma_data->block_operating,
				wdma_data->block_waiting);

	switch (cmd) {
	case TCC_WDMA_IOCTRL:
		ret = tcc_wdma_perform_WDMA_IOCTRL(wdma_data, arg);
		break;
	case TC_WDRV_COUNT_START:
		ret = tcc_wdma_perform_WDRV_COUNT_START(wdma_data, arg);
		break;
	case TC_WDRV_GET_CUR_DATA:
		ret = tcc_wdma_perform_GET_CUR_DATA(wdma_data, arg);
		break;
	case TC_WDRV_COUNT_END:
		ret = tcc_wdma_perform_COUNT_END(wdma_data);
		break;
	default:
		(void)pr_err("[ERR][WDMA][%s]not supported IOCTL(0x%x)\n", __func__, cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int tccxxx_wdma_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret = 0;

	(void)filp;

	vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);
	if (vma->vm_end >= vma->vm_start){
		ret = remap_pfn_range(vma,
								vma->vm_start,
								vma->vm_pgoff,
								vma->vm_end - vma->vm_start,
								vma->vm_page_prot);
		if (ret != 0) {
			goto err_remap;
		}
	}

	vma->vm_ops =  NULL;
	vma->vm_flags |= (unsigned long)VM_IO;
	vma->vm_flags |= (unsigned long)VM_DONTEXPAND | (unsigned long)VM_PFNMAP;

err_remap:
	return ret;
}

static unsigned int tccxxx_wdma_poll(struct file *filp, poll_table *wait)
{
	int ret = 0;
	unsigned long flags;

	struct miscdevice *misc = (struct miscdevice *)filp->private_data;

	struct tcc_wdma_dev *wdma_data = dev_get_drvdata(misc->parent);

	if (wdma_data == NULL) {
		(void)pr_err("[ERR][WDMA][%s]can't get wdma_data\n", __func__);
		ret = -EFAULT;
	}

	if (ret >= 0) {
		poll_wait(filp, &(wdma_data->poll_wq), wait);

		spin_lock_irqsave(&(wdma_data->poll_lock), flags);

		if (wdma_data->block_operating == 0U) {
			/* For KCS */
			ret = (int)((unsigned int)POLLIN|(unsigned int)POLLRDNORM);
		}

		spin_unlock_irqrestore(&(wdma_data->poll_lock), flags);
	}

	return (unsigned int)ret;
}

static int tccxxx_wdma_release(struct inode *pinode, struct file *filp)
{
	int32_t ret = 0;

	struct miscdevice *misc = (struct miscdevice *)filp->private_data;

	struct tcc_wdma_dev *pwdma_data = dev_get_drvdata(misc->parent);

	(void)pinode;

	wdma_dbg("%d : block_operating(%d) - block_waiting(%d) - irq_reged(%d)\n",
			pwdma_data->dev_opened,
			pwdma_data->block_operating,
			pwdma_data->block_waiting,
			pwdma_data->irq_reged);

	if (pwdma_data->dev_opened == 0U) {
		(void)pr_err("[ERR][WDMA][%s]no device is opened\n", __func__);

		ret = -ENXIO;

		goto return_funcs;
	} else {
		/* KCS */
		pwdma_data->dev_opened--;
	}

	if (pwdma_data->dev_opened == 0U) {
		if ((bool)pwdma_data->block_operating) {
			/* KCS */
			(void)tcc_wdma_wait_intrruptible(200, pwdma_data);
		}

		if ((bool)pwdma_data->irq_reged) {
			(void)irq_set_affinity_hint(pwdma_data->irq, NULL);
			(void)free_irq(pwdma_data->irq, pwdma_data);
			pwdma_data->irq_reged = 0;
		}

		pwdma_data->block_waiting = 0;
		pwdma_data->block_operating = 0;
	}

	if (pwdma_data->wdma_clk != NULL) {
		/* For KCS */
		clk_disable_unprepare(pwdma_data->wdma_clk);
	}

return_funcs:
	return ret;
}

static int tccxxx_wdma_open(struct inode *pinode, struct file *filp)
{
	int ret = 0;

	struct miscdevice *misc = (struct miscdevice *)filp->private_data;

	struct tcc_wdma_dev *pwdma_data = dev_get_drvdata(misc->parent);

	(void)pinode;

	wdma_dbg("%d times : block_operating(%d) - block_waiting(%d), irq(%d).\n",
			pwdma_data->dev_opened,
			pwdma_data->block_operating,
			pwdma_data->block_waiting,
			pwdma_data->irq_reged);

	if (pwdma_data->wdma_clk != NULL) {
		/* For KCS */
		(void)clk_prepare_enable(pwdma_data->wdma_clk);
	}

	if (pwdma_data->irq_reged == 0U) {
		ret = tcc_wdma_init_intr(pwdma_data);
		if (ret != 0){
			(void)pr_err("[ERR][WDMA][%s]failed to aquire irq\n", __func__);

			if (pwdma_data->wdma_clk != NULL) {
				/* For KCS */
				clk_disable_unprepare(pwdma_data->wdma_clk);
			}
		}

		pwdma_data->irq_reged = 1;
	}

	pwdma_data->dev_opened++;

	wdma_dbg("%d : block_operating(%d) - block_waiting(%d)\n",
			pwdma_data->dev_opened,
			pwdma_data->block_operating,
			pwdma_data->block_waiting);

	return ret;
}

#ifdef TCC_WDMA_SYSFS_CAPTURE
/*
*   Google doesn't allow TCC to use kernel_write() function for GKI implementation.
*   Therefore, TCC isn't able to use kernel_write() function for screen capture through sysfs.
*   __vfs_write() inside kernel_write() function is ported in tcc_wdma driver in order to keep screen capture through sysfs.
*/
static ssize_t screen_capture_vfs_write(struct file *filp, const char __user *buf, size_t len, loff_t *ppos)
{
	struct iovec iov = { .iov_base = (void __user *)buf, .iov_len = len };
	struct kiocb kiocb;
	struct iov_iter iter;
	ssize_t ret;

	init_sync_kiocb(&kiocb, filp);
	kiocb.ki_pos = (ppos ? *ppos : 0);
	iov_iter_init(&iter, WRITE, &iov, 1, len);

	ret = call_write_iter(filp, &kiocb, &iter);
	if ((ret > 0) && ppos) {
		*ppos = kiocb.ki_pos;
	}

	return ret;
}

static int32_t screen_capture_release_data(struct file *pfile,
															uint32_t image_size,
															dma_addr_t dst_img_phy_addr,
															void *dst_img_vir_addr,
															mm_segment_t old_fs,
															const struct miscdevice *pmisc)
{
	int32_t ret = 0;

	(void)vfs_fsync(pfile, 0);

	set_fs(old_fs);

	dma_free_coherent(pmisc->parent, image_size, dst_img_vir_addr, dst_img_phy_addr);

	(void)filp_close(pfile, NULL);

	return ret;
}

static int32_t screen_capture_write_data(char *pimage_name,
														uint32_t image_size,
														dma_addr_t dst_img_phy_addr,
														void *dst_img_vir_addr,
														const struct miscdevice *pmisc)
{
	int32_t ret = 0;
	ssize_t w_num;
	mm_segment_t oldfs;
	struct file *file_p;

	oldfs = get_fs();

	set_fs(KERNEL_DS);

	file_p = filp_open(pimage_name, O_CREAT | O_WRONLY, 0600);
	if (IS_ERR(file_p)) {
		(void)pr_err("[ERR][WDMA][%s]can't open %s\n", __func__, pimage_name);

		ret = -ENXIO;
		goto return_funcs;
	}

	if (file_p->f_op->write_iter == NULL) {
		/* KCS */
		(void)pr_err("[ERR][WDMA][%s]Target file ops isn't valid\n", __func__);
	} else {
		/* KCS */
		w_num = screen_capture_vfs_write(file_p, (char __user *)dst_img_vir_addr, image_size, &file_p->f_pos);
		if (w_num == 0) {
			(void)pr_err("[ERR][WDMA][%s]can't write capture data to file\n", __func__);
		}
	}

	(void)screen_capture_release_data(file_p,
											image_size,
											dst_img_phy_addr,
											dst_img_vir_addr,
											oldfs,
											pmisc);

return_funcs:
	return ret;
}

static int32_t screen_capture_wait_intrruptible(int32_t msec,
														struct tcc_wdma_dev *pwdma_data)
{
	int32_t int_ret = 0;
	int64_t num_of_jiffies;

	num_of_jiffies = tcc_wdma_msecs_to_jiffies(msec);

	int_ret = wait_event_interruptible_timeout(pwdma_data->cmd_wq,
												(pwdma_data->block_operating == 0U),
												num_of_jiffies);
	if (int_ret <= 0) {
		(void)pr_info("[INF][WDMA][%s]timed out(%ums) : block_operation(%d) - block_waiting(%d)\n",
						__func__,
						msec,
						pwdma_data->block_operating,
						pwdma_data->block_waiting);
	}

	return int_ret;
}

static int32_t screen_capture_alloc_mem(uint32_t image_size,
														const struct miscdevice *misc,
														dma_addr_t *pdst_img_phy_addr,
														void **dst_image_vir_addr)
{
	int32_t ret = 0;
	void *dst_image_virt_addr;
	dma_addr_t dst_image_phy_addr;

	dst_image_virt_addr = dma_alloc_coherent(misc->parent, image_size, &dst_image_phy_addr, GFP_KERNEL);
	if (dst_image_virt_addr == NULL) {
		(void)pr_err("[ERR][WDMA][%s]can't alloc memory\n", __func__);

		ret = -ENOMEM;

		goto return_funcs;
	}

	*pdst_img_phy_addr = dst_image_phy_addr;
	*dst_image_vir_addr = dst_image_virt_addr;

return_funcs:
	return ret;
}

static int32_t screen_capture_config_frame_infor(uint32_t *pimage_size,
															dma_addr_t *pdst_img_phy_addr,
															void **dst_image_vir_addr,
															struct tcc_wdma_dev **pwdma_data,
															struct miscdevice **pmisc,
															struct VIOC_WDMA_IMAGE_INFO_Type *cap_image_info)
{
	int32_t ret = 0;
	uint32_t wmix_h = 0, wmix_w = 0, image_size;
	void *dst_img_virt_addr;
	dma_addr_t dst_img_phy_addr;
	const struct file *filp;
	struct miscdevice *misc;
	struct tcc_wdma_dev *wdma_data;

	filp = filp_open(WDMA_DEV_NAME, O_RDWR, 0600);
	if (IS_ERR(filp)) {
		(void)pr_err("[ERR][WDMA][%s]can't open wdma device\n", __func__);

		ret = -ENXIO;

		goto return_funcs;
	}

	misc = (struct miscdevice *)filp->private_data;

	wdma_data = dev_get_drvdata(misc->parent);

	(void)memset((char *)cap_image_info, 0, sizeof(struct VIOC_WDMA_IMAGE_INFO_Type));

	if (wdma_data->wmix.reg != NULL) {
		/* KCS */
		VIOC_WMIX_GetSize(wdma_data->wmix.reg, &wmix_w, &wmix_h);
	}

	wmix_w = (wmix_w == 0) ? 1920U : wmix_w;
	wmix_h = (wmix_h == 0) ? 720U : wmix_h;

	wdma_data->wdma_continuous = 0;

	cap_image_info->ContinuousMode = 0;
	cap_image_info->TargetWidth = wmix_w;
	cap_image_info->TargetHeight = wmix_h;
	cap_image_info->ImgFormat = (unsigned int)TCC_LCDC_IMG_FMT_RGB888;

	image_size = (cap_image_info->TargetWidth * cap_image_info->TargetHeight * 4U);

	ret = screen_capture_alloc_mem(image_size, misc, &dst_img_phy_addr, &dst_img_virt_addr);
	if (ret != 0) {
		goto return_funcs;
	}

	(void)tcc_wdma_get_yuv_addr(dst_img_phy_addr, wdma_data, cap_image_info);

	*pimage_size = image_size;
	*pwdma_data = wdma_data;
	*pmisc = misc;
	*pdst_img_phy_addr = dst_img_phy_addr;
	*dst_image_vir_addr = dst_img_virt_addr;

	pr_info("\n[INF][WDMA][%s] Capturing screen image...", __func__);
	pr_info(" Input: %u x %u <-> Target: %u x %u - RGB888\n",
						wmix_w,
						wmix_h,
						cap_image_info->TargetWidth,
						cap_image_info->TargetHeight);

return_funcs:
	return ret;
}


/*
 * wdma screen capture
 */
static int tcc_wdma_screen_capture(char *Capture)
{
	int32_t ret = 0;
	uint32_t image_size;
	uint32_t aBaseAddr[3];
	void *dst_img_virt_addr;
	dma_addr_t dst_img_phy_addr;
	struct miscdevice *misc;
	struct tcc_wdma_dev *wdma_data;
	struct VIOC_WDMA_IMAGE_INFO_Type image_infor;

	ret = screen_capture_config_frame_infor(&image_size,
											&dst_img_phy_addr,
											&dst_img_virt_addr,
											&wdma_data,
											&misc,
											&image_infor);
	if (ret != 0) {
		goto return_funcs;
	}

	if ((bool)wdma_data->block_operating) {
		wdma_data->block_waiting = 1;

		(void)screen_capture_wait_intrruptible(200, wdma_data);
	}

	aBaseAddr[0] = image_infor.BaseAddress;
	aBaseAddr[1] = image_infor.BaseAddress1;
	aBaseAddr[2] = image_infor.BaseAddress2;

	ret = tcc_wdma_config_vioc_comp(image_infor.TargetWidth,
										image_infor.TargetHeight,
										image_infor.ImgFormat,
										aBaseAddr,
#if defined(CONFIG_ARCH_TCC899X) || defined(CONFIG_ARCH_TCC901X)
		 								image_infor.Contrast,
		 								image_infor.Bright,
		 								image_infor.Hue,
#endif
										wdma_data);
	if (ret != 0) {
		goto return_funcs;
	}

	(void)tcc_wdma_reset_intr((bool)true, 100, wdma_data);

	wdma_data->block_operating = 0;

	(void)tcc_wdma_release_sc(wdma_data);

	ret = screen_capture_write_data(Capture,
									image_size,
									dst_img_phy_addr,
									dst_img_virt_addr,
									misc);

return_funcs:
	return ret;
}

/*
 * sys-fs
 */
static char screencapture[MAX_IMAGE_NAME] = {0,};

static ssize_t screen_capture_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	(void)attr;
	(void)dev;

	return sprintf(buf, "%s\n", screencapture);
}

static ssize_t screen_capture_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t count)
{
	int ret = 0;
	ssize_t lret = 0;

	(void)attr;
	(void)dev;

	ret = sscanf(buf, "%s", screencapture);

	ret = tcc_wdma_screen_capture(screencapture);
	if (ret >= 0) {
		/* For KCS */
		(void)pr_info(" File saved : %s\n", screencapture);
	} else {
		/* For KCS */
		(void)pr_info(" Capture failure\n");
	}

	if(count <= (ULONG_MAX / 2UL)) {
		/* For KCS */
		lret = (ssize_t)count;
	}

	return lret;
}
static DEVICE_ATTR_RW(screen_capture);

static void tcc_wdma_attr_create(struct platform_device *pdev)
{
	(void)device_create_file(&pdev->dev, &dev_attr_screen_capture);
}
#endif	//#ifdef TCC_WDMA_SYSFS_CAPTURE

static int32_t tcc_wdma_init_os(struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;

	spin_lock_init(&(pwdma_data->poll_lock));

	spin_lock_init(&(pwdma_data->cmd_lock));

	mutex_init(&(pwdma_data->io_mutex));

	init_waitqueue_head(&(pwdma_data->poll_wq));

	init_waitqueue_head(&(pwdma_data->cmd_wq));

	return ret;
}



static const struct file_operations tcc_wdma_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = tccxxx_wdma_ioctl,
	.mmap = tccxxx_wdma_mmap,
	.open = tccxxx_wdma_open,
	.release = tccxxx_wdma_release,
	.poll = tccxxx_wdma_poll,
};

static int32_t tcc_wdma_init_drv(struct platform_device *pdev,
											struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;

	pwdma_data->misc->minor = MISC_DYNAMIC_MINOR;
	pwdma_data->misc->fops = &tcc_wdma_fops;
	pwdma_data->misc->name = pdev->name;
	pwdma_data->misc->parent = &pdev->dev;

	ret = misc_register(pwdma_data->misc);
	if (ret != 0) {
		/* For KCS */
		(void)pr_err("[ERR][WDMA][%s]fail to register ioctl\n", __func__);
	} else {
		(void)tcc_wdma_init_os(pwdma_data);

		platform_set_drvdata(pdev, (void *)pwdma_data);

		#ifdef TCC_WDMA_SYSFS_CAPTURE
		tcc_wdma_attr_create(pdev);
		#endif
	}

	return ret;
}

static int32_t tcc_wdma_parse_wdma_dt(const struct device_node *of_pdn,
													struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;
	uint32_t wdma_idx;
	struct device_node *wdma_node;

	wdma_node = of_parse_phandle(of_pdn, "wdmas", 0);
	if (wdma_node == NULL) {
		(void)pr_err("[ERR][WDMA][%s]can't find wdma drv node\n", __func__);

		goto return_funcs;
	}

	(void)of_property_read_u32_index(of_pdn, "wdmas", 1, &pwdma_data->wdma.id);
	if (ret < 0) {
		(void)pr_err("[ERR][WDMA][%s]can't find wdma id\n", __func__);

		goto return_funcs;
	}

	pwdma_data->wdma.reg = VIOC_WDMA_GetAddress(pwdma_data->wdma.id);

	wdma_idx = get_vioc_index(pwdma_data->wdma.id);

	pwdma_data->irq = irq_of_parse_and_map(wdma_node, (int)wdma_idx);
	pwdma_data->vioc_intr->id = ((unsigned int)VIOC_INTR_WD0 + wdma_idx);
	pwdma_data->vioc_intr->bits = (unsigned int)VIOC_WDMA_IREQ_EOFR_MASK;

return_funcs:
	return ret;
}

static int32_t tcc_wdma_parse_wmix_dt(const struct device_node *of_pdn,
													struct tcc_wdma_dev *pwdma_data)
{
	int ret = 0;

	ret = of_property_read_u32_index(of_pdn, "wmixs", 1, &pwdma_data->wmix.id);
	if (ret < 0) {
		/* For KCS */
		(void)pr_err("[ERR][WDMA][%s]can't find wmix id\n", __func__);
	} else {
		/* For KCS */
		pwdma_data->wmix.reg = VIOC_WMIX_GetAddress(pwdma_data->wmix.id);
	}

	return ret;
}

static int32_t tcc_wdma_parse_sc_dt(const struct device_node *of_pdn,
												struct tcc_wdma_dev *pwdma_data)
{
	int ret = 0;

	ret = of_property_read_u32_index(of_pdn, "scalers", 1, &pwdma_data->sc.id);
	if (ret < 0) {
		/* For KCS */
		(void)pr_err("[ERR][WDMA][%s]can't find scaler id\n", __func__);
	} else {
		/* For KCS */
		pwdma_data->sc.reg = VIOC_SC_GetAddress(pwdma_data->sc.id);
	}

	return ret;
}

static int32_t tcc_wdma_parse_disp_dt(const struct device_node *of_pdn,
													struct tcc_wdma_dev *pwdma_data)
{
	int ret = 0;

	ret = of_property_read_u32_index(of_pdn, "disp", 1, &pwdma_data->disp.id);
	if (ret < 0) {
		/* For KCS */
		(void)pr_err("[ERR][WDMA][%s]can't find disp id\n", __func__);
	} else {
		/* For KCS */
		pwdma_data->disp.reg = VIOC_DISP_GetAddress(pwdma_data->disp.id);
	}

	return ret;
}

static int tcc_wdma_parse_dt(const struct device_node *of_dn,
										struct tcc_wdma_dev *pwdma_data)
{
	int ret = 0;

	(void)tcc_wdma_parse_disp_dt(of_dn, pwdma_data);
	(void)tcc_wdma_parse_sc_dt(of_dn, pwdma_data);
	(void)tcc_wdma_parse_wmix_dt(of_dn, pwdma_data);
	(void)tcc_wdma_parse_wdma_dt(of_dn, pwdma_data);

	return ret;
}

static int32_t tcc_wdma_alloc_mem(struct tcc_wdma_dev *pwdma_data)
{
	int32_t ret = 0;

	pwdma_data->misc = kzalloc(sizeof(struct miscdevice), GFP_KERNEL);
	if (pwdma_data->misc == NULL) {
		(void)pr_err("[ERR][WDMA][%s]can't alloc misc data\n", __func__);

		kfree(pwdma_data);

		ret = -ENOMEM;
	} else {
		pwdma_data->vioc_intr = kzalloc(sizeof(struct vioc_intr_type), GFP_KERNEL);
		if (pwdma_data->vioc_intr == NULL) {
			(void)pr_err("[ERR][WDMA][%s]can't alloc vioc_intr data\n", __func__);

			kfree(pwdma_data->misc);
			kfree(pwdma_data);

			ret = -ENOMEM;
		}
	}

	return ret;
}

static int  tcc_wdma_probe(struct platform_device *pdev)
{
	int ret = 0;
	struct tcc_wdma_dev *wdma_data;

	wdma_dbg("Probe...\n");

	wdma_data = kzalloc(sizeof(struct tcc_wdma_dev), GFP_KERNEL);
	if (wdma_data == NULL) {
		(void)pr_err("[ERR][WDMA][%s]can't alloc wdma_data\n", __func__);

		goto return_funcs;
	}

	(void)memset(wdma_data, 0, sizeof(struct tcc_wdma_dev));

	(void)tcc_wdma_alloc_mem(wdma_data);

	(void)tcc_wdma_parse_dt(pdev->dev.of_node, wdma_data);

	(void)tcc_wdma_init_drv(pdev, wdma_data);

	pr_info("\n[WDMA][%s]TCC-WDMA Ver %d.%d.%d - %s build", 
					__func__,
					TCC_WDMA_DRIVER_MAJOR,
					TCC_WDMA_DRIVER_MINOR,
					TCC_WDMA_DRIVER_PATCH,
					TCC_WDMA_DRIVER_DATE);
	pr_info(" DISP id 0x%x, SC id 0x%x, Wmix id 0x%x, Wdma id 0x%x\n",
						wdma_data->disp.id,
						wdma_data->sc.id,
						wdma_data->wmix.id,
						wdma_data->wdma.id);

return_funcs:
	return ret;
}

static int tcc_wdma_remove(struct platform_device *pdev)
{
	const struct tcc_wdma_dev *pwdma_data;

	pwdma_data = (struct tcc_wdma_dev *)platform_get_drvdata(pdev);

	misc_deregister(pwdma_data->misc);

	kfree(pwdma_data->misc);
	kfree(pwdma_data);

	return 0;
}

#ifdef CONFIG_OF
static const struct of_device_id tcc_wdma_of_match[] = {
	{.compatible = "telechips,tcc_wdma"},
	{}
};
MODULE_DEVICE_TABLE(of, tcc_wdma_of_match);
#endif

static struct platform_driver tcc_wdma_driver = {
	.probe  = tcc_wdma_probe,
	.remove = tcc_wdma_remove,
	.driver = {
		.name   = "wdma",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = of_match_ptr(tcc_wdma_of_match),
#endif
	},
};

static int __init tcc_wdma_init(void)
{
	return platform_driver_register(&tcc_wdma_driver);
}

static void __exit tcc_wdma_exit(void)
{
	platform_driver_unregister(&tcc_wdma_driver);
}

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC WDMA Driver");
MODULE_LICENSE("GPL");

module_init(tcc_wdma_init);
module_exit(tcc_wdma_exit);
MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
