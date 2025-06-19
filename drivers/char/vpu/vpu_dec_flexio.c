// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"
#include "vpu_dec_flexio.h"

#include <linux/moduleparam.h>
#include <linux/device.h>
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
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/clk.h>
#include <linux/poll.h>
#include <linux/uaccess.h>
#include <linux/compat.h>
#include <linux/io.h>

#include "vpu_mgr.h"
#include "vpu_rm.h"

#define dprintk(msg...)  V_DBG(VPU_DBG_INFO, "VPU_DEC_FLEXIO: " msg)
#define detailk(msg...)  V_DBG(VPU_DBG_INFO, "VPU_DEC_FLEXIO: " msg)
#define err_dec(msg...)  V_DBG(VPU_DBG_ERROR, "VPU_DEC_FLEXIO [Err]: " msg)

#if (defined(CONFIG_VDEC_CNT_1) || defined(CONFIG_VDEC_CNT_2) || \
	defined(CONFIG_VDEC_CNT_3) || defined(CONFIG_VDEC_CNT_4) || \
	defined(CONFIG_VDEC_CNT_5))

extern void vdec_inter_add_list(struct vpu_decoder_data *vdata, int cmd, void *args);
extern bool vdec_dev_pre_init(struct vpu_decoder_data *vdata);
extern void vdec_init_list(struct vpu_decoder_data *vdata);
extern void vdec_dev_post_init(struct vpu_decoder_data *vdata);

//
// vpu v2: ip - init
//
static void v2_dec_flexip_init_insert(struct vpu_decoder_data *vdata,
									  struct v2hw_flex_io *fli)
{
	fli->v1_strt = (uint64_t)((uintptr_t)vdata);
	//(void)pr_info("%s: v1_strt(%#llx)", __func__, fli->v1_strt);

	/* add this unit into vpu work list */
	vdec_inter_add_list(vdata, V2D_IP_DRV_INI, (void *)fli);
}

static long v2dec_ioctl_ip_inidata(struct vpu_decoder_data *vdata, void *arg)
{
	struct v2hw_flex_io flx_meta;
	struct v2hw_flex_io *flx = &flx_meta;
	struct v2hw_flex_io *fli = NULL;
	size_t flx_index;

	/* sanity check: function arguments */
	if ((vdata == NULL) || (arg == NULL)) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* -------------------------------------------------------
	 * device initialize (partial)
	 * ------------------------------------------------------- */
	vdec_dev_pre_init(vdata);
	/* ------------------------------------------------------- */

	if (vdata->list_inited == false) {
		vdec_init_list(vdata);
	}

	/* copy from user: meta fields in flex io */
	// [FIXME] better to copy size(int): version field in flex meta only
	if (copy_from_user(flx, arg, sizeof(struct v2hw_flex_io)) != 0) {
		pr_err("[%s] copy_from_user failed", __func__);
		return -EFAULT;
	}

//	(void)pr_info("[%s][flx-peek] ver: %u, num: %u, max: %u", __func__,
//			flx->version, flx->numfields, flx->maxfields);

	/* assign flex array for init ip with size max
	 * : vpu mgr fills more of fli fields: bs|bw|ud hw buffers
	 */
	flx_index = V2D_FINDEX(V2D_IP_DRV_INI);
	V2_FLEXIO_ASSIGN(vdata->flx_list[flx_index], V2D_FI_INI_MAX);
	fli = vdata->flx_list[flx_index];
	if (fli == NULL) {
		pr_err("[%s] failed to kzalloc fli (size %u)", __func__, flx->numfields);
		return -EFAULT;
	}

	/* copy from user: all flex io (meta + array) */
	if (copy_from_user(fli, arg, fli->fx_size) != 0) {
		pr_err("[%s] copy_from_user failed", __func__);
		return -EFAULT;
	}

#if 0
	(void)printk(V2_FLXMETA_PRINTFMT(fli));
	for (flx_index = 0; flx_index < fli->numfields; flx_index++) {
		(void)pr_info("[%s][%zu] %d", __func__, flx_index, fli->fields[flx_index]);
	}
#endif

	/* peek codec type from flx to preset 'vdata->gsCodecType' */
	V2_FLEXIP_GET(fli, INI_FORMAT, vdata->gsCodecType);
	/* -------------------------------------------------------
	 * device initialize (partial)
	 * ------------------------------------------------------- */
	vdec_dev_post_init(vdata);
	/* ------------------------------------------------------- */

	/* bypass flex handle into vpu command list */
	v2_dec_flexip_init_insert(vdata, fli);

	return  0;
}

//
// vpu v2: ip - reset
//
static void v2_dec_flexip_reset_insert(struct vpu_decoder_data *vdata,
									   struct v2hw_flex_io *fli)
{
	fli->v1_strt = (uint64_t)((uintptr_t)vdata);
	//(void)pr_info("%s: v1_strt(%#llx)", __func__, fli->v1_strt);

	/* add this unit into vpu work list */
	vdec_inter_add_list(vdata, V2D_IP_DRV_RST, (void *)fli);
}

static long v2dec_ioctl_ip_rstdata(struct vpu_decoder_data *vdata)
{
	struct v2hw_flex_io *fli = NULL;
	size_t flx_index;

	/* sanity check: function arguments */
	if (vdata == NULL) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* assign flex array for init ip with size max */
	flx_index = V2D_FINDEX(V2D_IP_DRV_RST);
	V2_FLEXIO_ASSIGN(vdata->flx_list[flx_index], 0);
	fli = vdata->flx_list[flx_index];
	if (fli == NULL) {
		pr_err("[%s] failed to kzalloc fli (size %zu)", __func__,
				sizeof(struct v2hw_flex_io));
		return -EFAULT;
	}

	/* bypass flex handle into vpu command list */
	v2_dec_flexip_reset_insert(vdata, fli);

	return  0;
}

//
// vpu v2: iop - hw memory alloc
//
static long v2dec_ioctl_iop_hwbuf_asgn(struct vpu_decoder_data *vdata, void *arg)
{
	struct v2hw_flex_io flx_meta;
	struct v2hw_flex_io *flx = &flx_meta;
	struct v2hw_flex_io *fio = NULL;
	int32_t flx_type, flx_size;

	long ret;

	MEM_ALLOC_INFO_t mai;

	/* sanity check: function arguments */
	if (vdata == NULL) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* copy from user: meta fields in flex io */
	// [FIXME] better to copy size(int): version field in flex meta only
	if (copy_from_user(flx, arg, sizeof(struct v2hw_flex_io)) != 0) {
		pr_err("[%s] copy_from_user failed", __func__);
		return -EFAULT;
	}

	//(void)printk(V2_FLXMETA_PRINTFMT(flx));

	/* create local flex array for mem-asgn iop with size max */
	V2_FLEXIO_CREATE(fio, V2D_FI_HWBUF_MAX);
	if (fio == NULL) {
		pr_err("[%s] failed to kzalloc fio (size %zu)", __func__,
				sizeof(struct v2hw_flex_io));
		return -EFAULT;
	}

	/* copy from user: all flex io (meta + array) */
	if (copy_from_user(fio, arg, fio->fx_size) != 0) {
		pr_err("[%s] copy_from_user failed", __func__);
		V2_FLEXIO_DELETE(fio);
		return -EFAULT;
	}

	/* -----------------------------------------------
	 * convert flex fields into legacy v1 structure
	 * -----------------------------------------------
	 */

	/* [V2:KI:HWBUF:F:00] */
	flx_type = V2_VAL_HWBUF_MAX;
	V2_FLEXIP_GET(fio, HWBUF_TYPE, flx_type);

	switch (flx_type) {
	case V2_VAL_HWBUF_BS:
		mai.buffer_type = BUFFER_STREAM;
		break;
	case V2_VAL_HWBUF_FB:
		mai.buffer_type = BUFFER_FRAMEBUFFER;
		break;
	case V2_VAL_HWBUF_UD:
		mai.buffer_type = BUFFER_USERDATA;
		break;
	default:
		/* NOTE: other hw buffers - u/s doesn't care  */
		break;
	}

	/* [V2:KI:HWBUF:F:01] */
	flx_size = 0;
	V2_FLEXIP_GET(fio, HWBUF_SIZE, flx_size);
	mai.request_size = (uint32_t)flx_size;

	if (mai.request_size > 0u) {
		ret = vmem_proc_alloc_memory(vdata->gsCodecType, &mai,
				(vputype)vdata->gsDecType);

		if (ret == RETCODE_SUCCESS) {
			/* [V2:KI:HWBUF:F:00] */
			V2_FLEXIP_SET(fio, HWBUF_TYPE, flx_type);
			/* [V2:KI:HWBUF:F:01] */
			V2_FLEXIP_SET(fio, HWBUF_SIZE, flx_size);
			/* [V2:KI:HWBUF:F:02|03] */
			V2_FLEXIP_SET_ADDR(fio, HWBUF_PA, mai.phy_addr);
		} else {
			pr_err("failed to alloc hw buf: bitstream");
		}
	}

	if ((fio->fx_size > 0) && (fio->fx_size < UINT_MAX)) {
		/* copy to user: all flex io (meta + array) */
		if (copy_to_user(arg, fio, fio->fx_size) == 0) {
			ret = 0; // success
		} else {
			pr_err("[%s] copy_from_user failed", __func__);
			ret = -EFAULT;
		}
	} else {
		pr_err("[%s] Invalid fx_size value", __func__);
		ret = -EINVAL; // Return an error code for invalid input
	}

	/* delete local flex array for mem-asgn iop */
	V2_FLEXIO_DELETE(fio);

	return ret;
}

// vpu v2: ip - seqdata
static void v2_dec_flexip_seqhdr_insert(struct vpu_decoder_data *vdata,
										struct v2hw_flex_io *fli)
{
	fli->v1_strt = (uint64_t)((uintptr_t)vdata);
	//(void)pr_info("%s: v1_strt(%#llx)", __func__, fli->v1_strt);

	/* add this unit into vpu work list */
	vdec_inter_add_list(vdata, V2D_IP_DEC_SEQDATA, (void *)fli);
}

static long v2dec_ioctl_ip_seqdata(struct vpu_decoder_data *vdata, void *arg)
{
	struct v2hw_flex_io flx_meta;
	struct v2hw_flex_io *flx = &flx_meta;
	struct v2hw_flex_io *fli;
	size_t flx_index;

	/* sanity check: function arguments */
	if ((vdata == NULL) || (arg == NULL)) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* copy from user: meta fields in flex io */
	// [FIXME] better to copy size(int): version field in flex meta only
	if (copy_from_user(flx, arg, sizeof(struct v2hw_flex_io)) != 0) {
		pr_err("[%s] copy_from_user failed", __func__);
		return -EFAULT;
	}

	/* assign flex array for decseq ip to max (reason: d/d will add fields more) */
	flx_index = V2D_FINDEX(V2D_IP_DEC_SEQDATA);
	V2_FLEXIO_ASSIGN(vdata->flx_list[flx_index], V2D_FI_DECSEQ_MAX);
	fli = vdata->flx_list[flx_index];
	if (fli == NULL) {
		pr_err("[%s] failed to assign fli (num:%u, max:%u)",
				__func__, flx->numfields, flx->maxfields);
		return -EFAULT;
	}

	if ((flx->fx_size > 0) && (flx->fx_size < UINT_MAX)) {
		/* copy from user: all flex io (meta + array) */
		if (copy_from_user(fli, arg, flx->fx_size) != 0) {
			pr_err("[%s] copy_from_user failed", __func__);
			return -EFAULT;
		}
	} else {
		pr_err("[%s] Invalid fx_size value", __func__);
		return -EINVAL; // Return an error code for invalid input
	}

#if 0
	(void)printk(V2_FLXMETA_PRINTFMT(fli));
	for (flx_index = 0; flx_index < fli->numfields; flx_index++) {
		(void)pr_info("[%s][%zu] %#x", __func__, flx_index, fli->fields[flx_index]);
	}
#endif

	v2_dec_flexip_seqhdr_insert(vdata, fli);

	return 0;
}

// vpu v2: ip - frmdata
static void v2_dec_flexip_decfrm_insert(struct vpu_decoder_data *vdata,
										struct v2hw_flex_io *fli)
{
	//(void)printk("%s: v1_strt(%#llx)", __func__, fli->v1_strt);

	/* add this unit into vpu work list */
	vdec_inter_add_list(vdata, V2D_IP_DEC_FRMDATA, (void *)fli);
}

static long v2dec_ioctl_ip_frmdata(struct vpu_decoder_data *vdata, void *arg)
{
	struct v2hw_flex_io flx_meta;
	struct v2hw_flex_io *flx = &flx_meta;
	struct v2hw_flex_io *fli;
	size_t flx_index;

	struct v2hw_io_delay *dio;
	size_t push_index = 0u;
	size_t pop_index = 0u;

	bool ready = false;
	int32_t ra_mode = 0;

	/* sanity check: function arguments */
	if ((vdata == NULL) || (arg == NULL)) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* copy from user: meta fields in flex io */
	// [FIXME] better to copy size(int): version field in flex meta only
	if (copy_from_user(flx, arg, sizeof(struct v2hw_flex_io)) != 0) {
		pr_err("[%s] copy_from_user failed", __func__);
		return -EFAULT;
	}

	/* check the boolean field: delayed bs buffer input */
	if (V2_FLEXIP_B_CHECK(flx, DECFRM_B_DELAYFRM)) {
		size_t i;

		if (vdata->flx_io_delay == NULL) {
			pr_err("[%s][vtype:%d] create flex io delayer", __func__, vdata->gsDecType);
			V2_FLXDIO_CREATE(vdata->flx_io_delay);
		}
		dio = vdata->flx_io_delay;

		/* update dio state: delay -> ready */
		for (i = 0u; i < 2u; i++) {
			if (dio->ip_state[i] == V2_DIO_DELAY) {
				dio->ip_state[i] = V2_DIO_READY;
				pop_index = i;
				ready = true;
				break;
			}
		}

		/* update dio state: empty -> delay */
		for (i = 0u; i < 2u; i++) {
			if (dio->ip_state[i] == V2_DIO_EMPTY) {
				dio->ip_state[i] = V2_DIO_DELAY;
				push_index = i;
				break;
			}
		}

		//(void)pr_info("[%s] ready(%d), pop(%d), push(%d)", __func__, ready, pop_index, push_index);
	} else {
		ready = true;
	}

	/* assign flex array for decfrm ip */
	flx_index = V2D_FINDEX(V2D_IP_DEC_FRMDATA + push_index);
	V2_FLEXIO_ASSIGN(vdata->flx_list[flx_index], flx->numfields);
	fli = vdata->flx_list[flx_index];
	if (fli == NULL) {
		pr_err("[%s] failed to assign fli (num:%u, max:%u)", __func__,
				flx->numfields, flx->maxfields);
		return -EFAULT;
	}

	if ((flx->fx_size > 0) && (flx->fx_size < UINT_MAX)) {
		/* copy from user: all flex io (meta + array) */
		if (copy_from_user(fli, arg, flx->fx_size) != 0) {
			pr_err("[%s] copy_from_user failed", __func__);
			return -EFAULT;
		}

		/* register vdata address*/
		fli->v1_strt = (uint64_t)((uintptr_t)vdata);

		if (V2_FLEXIP_B_CHECK(fli, DECFRM_B_DELAYFRM)) {
			/* save frame search/skip mode of flex push queue */
			V2_FLEXIP_GET(fli, DECFRM_RA_MODE, ra_mode);
		}
	} else {
		pr_err("[%s] Invalid fx_size value", __func__);
		return -EINVAL; // Return an error code for invalid input
	}

#if 0
	(void)printk(V2_FLXMETA_PRINTFMT(fli));
	for (flx_index = 0; flx_index < fli->numfields; flx_index++) {
		(void)pr_info("[%s][%zu] %#x", __func__, flx_index, fli->fields[flx_index]);
	}
#endif

	if (ready) {
		if (dio != NULL) {
			/* update dio state: ready -> empty */
			if (dio->ip_state[pop_index] == V2_DIO_READY) {
				dio->ip_state[pop_index] = V2_DIO_EMPTY;
			}

			flx_index = V2D_FINDEX(V2D_IP_DEC_FRMDATA + pop_index);
			fli = vdata->flx_list[flx_index];

			if (V2_FLEXIP_B_CHECK(fli, DECFRM_B_DELAYFRM)) {
				/* move frame search/skip mode from push to pop queue */
				V2_FLEXIP_SET(fli, DECFRM_RA_MODE, ra_mode);
			}
		}

		v2_dec_flexip_decfrm_insert(vdata, fli);
	} else {
		if (dio != NULL) {
			if (dio->op_state != V2_DIO_READY) {
				(void)pr_info("[%s][vtype:%d] output is delayed (pop: %zu, push: %zu, op state: %d)",
						__func__, vdata->gsDecType, pop_index, push_index, dio->op_state);
				dio->op_state = V2_DIO_DELAY;
			}

			/* should wake up unexpected poll wait from user space immediately */
			if (vdata->vComm_data.count == 0) {
				vdata->vComm_data.count++;
			}
		} else {
			pr_err("[%s][%d] not ready !!!!!", __func__, vdata->gsDecType);
		}
	}

	return 0;
}

//
// vpu v2: ip - clear a fb slot
//
static void v2_dec_flexip_clear_insert(struct vpu_decoder_data *vdata,
									   struct v2hw_flex_io *fli)
{
	/* add this unit into vpu work list */
	vdec_inter_add_list(vdata, V2D_IP_FRM_CLEAR, (void *)fli);
}

static long v2dec_ioctl_ip_clrdata(struct vpu_decoder_data *vdata, void *arg)
{
	struct v2hw_flex_io *fli = NULL;
	size_t flx_index;

	/* sanity check: function arguments */
	if (vdata == NULL) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* assign flex array for clrdata ip with size max */
	flx_index = V2D_FINDEX(V2D_IP_FRM_CLEAR);
	V2_FLEXIO_ASSIGN(vdata->flx_list[flx_index], V2D_FI_CLEAR_MAX);
	fli = vdata->flx_list[flx_index];
	if (fli == NULL) {
		pr_err("[%s] failed to assign fli (num field: %u)",
				__func__, V2D_FI_CLEAR_MAX);
		return -EFAULT;
	}

	/* copy from user: all flex io (meta + array) */
	if (copy_from_user(fli, arg, fli->fx_size) != 0) {
		pr_err("[%s] copy_from_user failed", __func__);
		return -EFAULT;
	}

	/* bypass flex handle into vpu command list */
	v2_dec_flexip_clear_insert(vdata, fli);

	return  0;
}

//
// vpu v2: ip - set write position of ring buffer
//
static void v2_dec_flexip_setpos_insert(struct vpu_decoder_data *vdata,
										struct v2hw_flex_io *fli)
{
	fli->v1_strt = (uint64_t)((uintptr_t)vdata);
	//(void)pr_info("%s: v1_strt(%#llx)", __func__, fli->v1_strt);

	/* add this unit into vpu work list */
	vdec_inter_add_list(vdata, V2D_IP_RNG_SETPOS, (void *)fli);
}

static long v2dec_ioctl_ip_setpos(struct vpu_decoder_data *vdata, void *arg)
{
	struct v2hw_flex_io *fli = NULL;
	size_t flx_index;

	/* sanity check: function arguments */
	if (vdata == NULL) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* assign flex array for setpos ip with size max */
	flx_index = V2D_FINDEX(V2D_IP_RNG_SETPOS);
	V2_FLEXIO_ASSIGN(vdata->flx_list[flx_index], V2D_FI_RNG_SETPOS_MAX);
	fli = vdata->flx_list[flx_index];
	if (fli == NULL) {
		pr_err("[%s] failed to assign fli (num field: %u)",
				__func__, V2D_FI_RNG_SETPOS_MAX);
		return -EFAULT;
	}

	/* copy from user: all flex io (meta + array) */
	if (copy_from_user(fli, arg, fli->fx_size) != 0) {
		pr_err("[%s] copy_from_user failed", __func__);
		return -EFAULT;
	}

	/* bypass flex handle into vpu command list */
	v2_dec_flexip_setpos_insert(vdata, fli);

	return  0;
}

//
// vpu v2: ip - get read/write position of ring buffer
//
static void v2_dec_flexip_getpos_insert(struct vpu_decoder_data *vdata,
										struct v2hw_flex_io *fli)
{
	fli->v1_strt = (uint64_t)((uintptr_t)vdata);
	//(void)pr_info("%s: v1_strt(%#llx)", __func__, fli->v1_strt);

	/* add this unit into vpu work list */
	vdec_inter_add_list(vdata, V2D_IP_RNG_GETPOS, (void *)fli);
}

static long v2dec_ioctl_ip_getpos(struct vpu_decoder_data *vdata)
{
	struct v2hw_flex_io *fli = NULL;
	size_t flx_index;

	/* sanity check: function arguments */
	if (vdata == NULL) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* assign flex array for init ip with size max */
	flx_index = V2D_FINDEX(V2D_IP_RNG_GETPOS);
	V2_FLEXIO_ASSIGN(vdata->flx_list[flx_index], 0);
	fli = vdata->flx_list[flx_index];
	if (fli == NULL) {
		pr_err("[%s] failed to kzalloc fli (size %zu)", __func__,
				sizeof(struct v2hw_flex_io));
		return -EFAULT;
	}

	/* bypass flex handle into vpu command list */
	v2_dec_flexip_getpos_insert(vdata, fli);

	return  0;
}

//
// vpu v2: ip - flush fb slots
//
static void v2_dec_flexip_flush_insert(struct vpu_decoder_data *vdata,
									   struct v2hw_flex_io *fli)
{
	fli->v1_strt = (uint64_t)((uintptr_t)vdata);

	/* add this unit into vpu work list */
	vdec_inter_add_list(vdata, V2D_IP_FRM_FLUSH, (void *)fli);
}

static long v2dec_ioctl_ip_fludata(struct vpu_decoder_data *vdata, void *arg)
{
	struct v2hw_flex_io *fli = NULL;
	size_t flx_index;

	/* sanity check: function arguments */
	if (vdata == NULL) {
		(void)pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* assign flex array for init ip with size max */
	flx_index = V2D_FINDEX(V2D_IP_FRM_FLUSH);
	V2_FLEXIO_ASSIGN(vdata->flx_list[flx_index], V2D_FI_FLUSH_MAX);
	fli = vdata->flx_list[flx_index];
	if (fli == NULL) {
		(void)pr_err("[%s] failed to assign fli (num field: %u)",
				__func__, V2D_FI_FLUSH_MAX);
		return -EFAULT;
	}

	/* copy from user: all flex io (meta + array) */
	if (copy_from_user(fli, arg, fli->fx_size) != 0) {
		(void)pr_err("[%s] copy_from_user failed", __func__);
		return -EFAULT;
	}

	/* bypass flex handle into vpu command list */
	v2_dec_flexip_flush_insert(vdata, fli);

	return  0;
}

// vpu v2: ip - drndata
static void v2_dec_flexip_drain_insert(struct vpu_decoder_data *vdata,
										struct v2hw_flex_io *fli)
{
	fli->v1_strt = (uint64_t)((uintptr_t)vdata);
	//(void)pr_info("%s: v1_strt(%#llx)", __func__, fli->v1_strt);

	/* add this unit into vpu work list */
	vdec_inter_add_list(vdata, V2D_IP_FRM_DRAIN, (void *)fli);
}

static long v2dec_ioctl_ip_drndata(struct vpu_decoder_data *vdata, void *arg)
{
	struct v2hw_flex_io flx_meta;
	struct v2hw_flex_io *flx = &flx_meta;
	struct v2hw_flex_io *fli;
	struct v2hw_io_delay *dio;
	size_t flx_index;

	size_t pop_index = 0u;
	bool input_exists = false;

	/* sanity check: function arguments */
	if ((vdata == NULL) || (arg == NULL)) {
		(void)pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* copy from user: meta fields in flex io */
	// [FIXME] better to copy size(int): version field in flex meta only
	if (copy_from_user(flx, arg, sizeof(struct v2hw_flex_io)) != 0) {
		(void)pr_err("[%s] copy_from_user failed", __func__);
		return -EFAULT;
	}

	//(void)pr_info("[%s][flx-peek] ver: %u, num: %u",
	//               __func__, flx->version, flx->numfields);

	/* assign flex array for decfrm ip */
	flx_index = V2D_FINDEX(V2D_IP_FRM_DRAIN);
	V2_FLEXIO_ASSIGN(vdata->flx_list[flx_index], flx->numfields);
	fli = vdata->flx_list[flx_index];
	if (fli == NULL) {
		pr_err("[%s] failed to assign fli (num:%u, max:%u)", __func__, flx->numfields, flx->maxfields);
		return -EFAULT;
	}

	if ((flx->fx_size > 0) && (flx->fx_size < UINT_MAX)) {
		/* copy from user: all flex io (meta + array) */
		if (copy_from_user(fli, arg, flx->fx_size) != 0) {
			(void)pr_err("[%s] copy_from_user failed", __func__);
			return -EFAULT;
		}
	} else {
		(void)pr_err("[%s] Invalid fx_size value", __func__);
		return -EINVAL; // Return an error code for invalid input
	}

#if 0
	(void)printk(V2_FLXMETA_PRINTFMT(fli));
	for (flx_index = 0; flx_index < fli->numfields; flx_index++) {
		(void)pr_info("[%s][%zu] %#x", __func__, flx_index, fli->fields[flx_index]);
	}
#endif

	dio = vdata->flx_io_delay;
	if (dio != NULL) {
		/* update dio state: delay -> empty */
		size_t i;
		for (i = 0u; i < 2u; i++) {
			if (dio->ip_state[i] == V2_DIO_DELAY) {
				dio->ip_state[i] = V2_DIO_EMPTY;
				pop_index = i;
				input_exists = true;
				break;
			}
		}
	}

	if (input_exists) {
		flx_index = V2D_FINDEX(V2D_IP_DEC_FRMDATA + pop_index);
		fli = vdata->flx_list[flx_index];

		(void)pr_info("[%s][vtype:%d] pop delayed flx frame (index:%zu)",
				__func__, vdata->gsDecType, pop_index);

		v2_dec_flexip_decfrm_insert(vdata, fli);
	} else {
		v2_dec_flexip_drain_insert(vdata, fli);
	}

	return 0;
}

//
// vpu v2: op - vdata result only
//
static int v2dec_ioctl_op_result(struct vpu_decoder_data *vdata, void *arg)
{
	int32_t ret;
	struct v2hw_flex_io flx_meta;
	struct v2hw_flex_io *flo = &flx_meta;

	/* sanity check: function arguments */
	if ((vdata == NULL) || (arg == NULL)) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	vetc_memset(flo, 0x00, sizeof(struct v2hw_flex_io), 0);

	/* set flex result */
	flo->result = vdata->gsCommDecResult;

	/* copy to user: flex io (meta only) */
	if (copy_to_user(arg, flo, sizeof(flx_meta)) == 0) {
		ret = 0; // success
	} else {
		pr_err("%s: copy_from_user failed", __func__);
		ret = -EFAULT;
	}

	return ret;
}

// vpu v2: op - init
static long v2dec_ioctl_op_inidata(struct vpu_decoder_data *vdata, void *arg)
{
	long ret;
	struct v2hw_flex_io *flo;
	size_t flx_index;

	/* sanity check: function arguments */
	if ((vdata == NULL) || (arg == NULL)) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* get access point of flex handle for 'inidata' output process */
	flx_index = V2D_FINDEX(V2D_OP_DRV_INI);
	flo = vdata->flx_list[flx_index];
	if (flo == NULL) {
		pr_err("[%s] failed to get ap: flo (max. %u)", __func__, V2D_FO_INI_MAX);
		return -EFAULT;
	}

	/* set flex result */
	flo->result = vdata->gsCommDecResult;

	/* copy to user: all flex io (meta + array) */
	if (copy_to_user(arg, flo, flo->fx_size) == 0) {
		ret = 0; // success
	} else {
		pr_err("[%s] copy_from_user failed", __func__);
		ret = -EFAULT;
	}

	return ret;
}

// vpu v2: op - seqdata
static long v2dec_ioctl_op_seqdata(struct vpu_decoder_data *vdata, void *arg)
{
	long ret;
	struct v2hw_flex_io *flo;
	size_t flx_index;

	/* sanity check: function arguments */
	if ((vdata == NULL) || (arg == NULL)) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* get access point of flex handle for 'seqdata' output process */
	flx_index = V2D_FINDEX(V2D_OP_DEC_SEQDATA);
	flo = vdata->flx_list[flx_index];
	if (flo == NULL) {
		pr_err("[%s] failed to get ap: flo (max. %u)", __func__, V2D_FO_DECSEQ_MAX);
		return -EFAULT;
	}

	/* set flex result */
	flo->result = vdata->gsCommDecResult;

	/* copy to user: all flex io (meta + array) */
	if (copy_to_user(arg, flo, flo->fx_size) == 0) {
		ret = 0; // success
	} else {
		pr_err("[%s] copy_to_user failed", __func__);
		ret = -EFAULT;
	}

	return ret;
}

// vpu v2: op - frmdata
static long v2dec_ioctl_op_frmdata(struct vpu_decoder_data *vdata, void *arg)
{
	long ret;
	struct v2hw_flex_io *flo;
	size_t flx_index;

	struct v2hw_flex_io flx_meta;

	/* sanity check: function arguments */
	if ((vdata == NULL) || (arg == NULL)) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* get access point of flex handle for 'frmdata' output process */
	flx_index = V2D_FINDEX(V2D_OP_DEC_FRMDATA);
	flo = vdata->flx_list[flx_index];

	if (vdata->flx_io_delay != NULL) {
		switch (vdata->flx_io_delay->op_state) {
		case V2_DIO_DELAY:
			pr_err("[%s][vtype:%d] op_state: V2_DIO_DELAY", __func__, vdata->gsDecType);
			if (flo == NULL) {
				flo = &flx_meta;
				V2_FLEXIO_INIT(flo, 0u);
			}
			flo->result = V2_ERR_INPUT_UNDERRUN;
			vdata->flx_io_delay->op_state = V2_DIO_EMPTY;
			break;

		case V2_DIO_READY:
			pr_err("[%s][vtype:%d] op_state: V2_DIO_READY", __func__, vdata->gsDecType);
			vdata->flx_io_delay->op_state = V2_DIO_EMPTY;
			break;

		default:
			break;
		}
	}

	if (flo == NULL) {
		pr_err("[%s] failed to get ap: flo", __func__);
		return -EFAULT;
	}

	/* set flex result */
	flo->result = vdata->gsCommDecResult;

	/* copy to user: all flex io (meta + array) */
	if (copy_to_user(arg, flo, flo->fx_size) == 0) {
		ret = 0; // success
	} else {
		pr_err("[%s] copy_from_user failed", __func__);
		ret = -EFAULT;
	}

	return ret;
}

// vpu v2: op - set write position of ring buffer
static long v2dec_ioctl_op_setpos(struct vpu_decoder_data *vdata, void *arg)
{
	int32_t ret;
	struct v2hw_flex_io flx_meta;
	struct v2hw_flex_io *flo = &flx_meta;

	/* sanity check: function arguments */
	if ((vdata == NULL) || (arg == NULL)) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	vetc_memset(flo, 0x00, sizeof(struct v2hw_flex_io), 0);

	/* set flex result */
	flo->result = vdata->gsCommDecResult;

	/* copy to user: flex io (meta only) */
	if (copy_to_user(arg, flo, sizeof(flx_meta)) == 0) {
		ret = 0; // success
	} else {
		pr_err("%s: copy_from_user failed", __func__);
		ret = -EFAULT;
	}

	return ret;
}

// vpu v2: op - get read/write position of ring buffer
static long v2dec_ioctl_op_getpos(struct vpu_decoder_data *vdata, void *arg)
{
	long ret;
	struct v2hw_flex_io *flo;
	size_t flx_index;

	/* sanity check: function arguments */
	if ((vdata == NULL) || (arg == NULL)) {
		pr_err("[%s] invalid params", __func__);
		return -EFAULT;
	}

	/* get access point of flex handle for 'getpos' output process */
	flx_index = V2D_FINDEX(V2D_OP_RNG_GETPOS);
	flo = vdata->flx_list[flx_index];
	if (flo == NULL) {
		pr_err("[%s] failed to get ap: flo (max. %u)", __func__, V2D_OP_RNG_GETPOS);
		return -EFAULT;
	}

	/* set flex result */
	flo->result = vdata->gsCommDecResult;

	/* copy to user: all flex io (meta + array) */
	if (copy_to_user(arg, flo, flo->fx_size) == 0) {
		ret = 0; // success
	} else {
		pr_err("[%s] copy_to_user failed", __func__);
		ret = -EFAULT;
	}

	return ret;
}

long vdec_ioctl_flexio(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long lret = 0;
	struct miscdevice *misc = (struct miscdevice *)filp->private_data;
	struct vpu_decoder_data *vdata = dev_get_drvdata(misc->parent);
	union {
		unsigned long ul_data;
		unsigned int *pui_data;
		int *pi_data;	//NULL
		void *pv_data;
		const void *pcv_data;
		MEM_ALLOC_INFO_t *pmai_data;
		const MEM_ALLOC_INFO_t *pcmai_data;
		MEM_ALLOC_INFO_EX_t *pmie_data;
	} uarg;

	uarg.pi_data = NULL;
	uarg.ul_data = arg;

	switch (cmd) {

	case V2D_IP_DRV_INI:
		lret = v2dec_ioctl_ip_inidata(vdata, uarg.pv_data);
		break;

	case V2D_OP_DRV_INI:
		lret = v2dec_ioctl_op_inidata(vdata, uarg.pv_data);
		break;

	case V2D_IP_DEC_SEQDATA:
		lret = v2dec_ioctl_ip_seqdata(vdata, uarg.pv_data);
		break;

	case V2D_OP_DEC_SEQDATA:
		lret = v2dec_ioctl_op_seqdata(vdata, uarg.pv_data);
		break;

	case V2D_IP_FRM_DRAIN:
		lret = v2dec_ioctl_ip_drndata(vdata, uarg.pv_data);
		break;

	case V2D_IP_DEC_FRMDATA:
		lret = v2dec_ioctl_ip_frmdata(vdata, uarg.pv_data);
		break;

	case V2D_OP_DEC_FRMDATA:
		lret = v2dec_ioctl_op_frmdata(vdata, uarg.pv_data);
		break;

	case V2D_IP_FRM_CLEAR:
		lret = v2dec_ioctl_ip_clrdata(vdata, uarg.pv_data);
		break;

	case V2D_OP_FRM_CLEAR:
		lret = v2dec_ioctl_op_result(vdata, uarg.pv_data);
		break;

	case V2D_IP_FRM_FLUSH:
		lret = v2dec_ioctl_ip_fludata(vdata, uarg.pv_data);
		break;

	case V2D_OP_FRM_FLUSH:
		lret = v2dec_ioctl_op_result(vdata, uarg.pv_data);
		break;

	case V2D_IP_RNG_SETPOS:
		lret = v2dec_ioctl_ip_setpos(vdata, uarg.pv_data);
		break;

	case V2D_OP_RNG_SETPOS:
		lret = v2dec_ioctl_op_setpos(vdata, uarg.pv_data);
		break;

	case V2D_IP_RNG_GETPOS:
		lret = v2dec_ioctl_ip_getpos(vdata);
		break;

	case V2D_OP_RNG_GETPOS:
		lret = v2dec_ioctl_op_getpos(vdata, uarg.pv_data);
		break;

	case V2D_IP_DRV_RST:
		lret = v2dec_ioctl_ip_rstdata(vdata);
		break;

	case V2D_OP_DRV_RST:
		lret = v2dec_ioctl_op_result(vdata, uarg.pv_data);
		break;

	case V2D_IP_HWBUF_ASGN:
		lret = v2dec_ioctl_iop_hwbuf_asgn(vdata, uarg.pv_data);
		break;

	default:
		err_dec("[%s] Unsupported ioctl[%d]!!!",
			vdata->misc->name, cmd);
		break;
	}

	return lret;
}

#endif /*CONFIG_VDEC_CNT_X*/
