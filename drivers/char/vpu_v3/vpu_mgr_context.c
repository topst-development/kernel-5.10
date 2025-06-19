// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_mgr_context.h"
#include "vpu_mgr.h"
#include "vpu_etc.h"
#include "vpu_dbg.h"
#include "vpu_rm.h"
#include "vpu_dbg_string.h"

#define dlog_vmctx(msg...)  	V_DBG(VPU_DBG_INFO,  "[VPU_MGR_CTX][LOG]:" msg)
#define detail_vmctx(msg...)  	V_DBG(VPU_DBG_DETAIL,  "[VPU_MGR_CTX][DETAIL]::" msg)
#define seq_vmctx(msg...)     	V_DBG(VPU_DBG_CMD,  "[VPU_MGR_CTX][SEQ]:" msg)
#define err_vmctx(msg...)      	V_DBG(VPU_DBG_ERROR, "[VPU_MGR_CTX][ERR]:" msg)

DEFINE_MUTEX(vpumgr_mutex);
static vpu_mgr_t *vpu_mgr_array[VPU_IP_MAX];

enum vpu_ip_type vmgr_find_ip_type(unsigned int drv_id, const enum vpu_codec_id codec_id, enum vpu_op_type op_type, int forced_index)
{
	int mgr_idx;
	int pcap_idx;
	int same_ip_index = 0;
	unsigned long min_pixel_product = ULONG_MAX;

	enum vpu_ip_type ip_type = VPU_IP_UNKNOWN;
	vpu_mgr_t *mgr_ctx = NULL;

	vetc_mutex_lock(&vpumgr_mutex);

	dlog_vmctx("%s:%u, codec:%s(%d), force_index:%d", vmgr_get_optype_name(op_type), drv_id, vmgr_get_codec_name(codec_id), codec_id, forced_index);

	//retrieve information by iterating through each IP
	for (mgr_idx = 0; mgr_idx < VPU_IP_MAX; mgr_idx++) {
		mgr_ctx = vpu_mgr_array[mgr_idx];
		if (mgr_ctx != NULL) {
			vpu_ip_cap_t *pcap = NULL;

			detail_vmctx("%s:%u, try search in IP:%s(%d) codec type:%s(%d)", vmgr_get_optype_name(op_type), drv_id, vmgr_get_ip_name(mgr_idx), mgr_idx, vmgr_get_codec_name(codec_id), codec_id);

			//retrieve information based on the encoder/decoder type
			if (op_type == VPU_OP_TYPE_DEC) {
				pcap = mgr_ctx->each_ip->dec_capa;
				detail_vmctx("%s:%u, IP:%s(%d) get dec capa:%p", vmgr_get_optype_name(op_type), drv_id, vmgr_get_ip_name(mgr_idx), mgr_idx, pcap);
			} else {
				pcap = mgr_ctx->each_ip->enc_capa;
				detail_vmctx("%s:%u, IP:%s(%d) get enc capa:%p", vmgr_get_optype_name(op_type), drv_id, vmgr_get_ip_name(mgr_idx), mgr_idx, pcap);
			}

			//pcap of encoder or decoder
			if (pcap != NULL) {
				//the maximum size of the capabilities array is MAX_SUPPORT_CODEC(64)
				for (pcap_idx = 0; pcap_idx < MAX_SUPPORT_CODEC; pcap_idx++) {
					const char *codec_name;

					detail_vmctx("%s:%u, pcap idx:%d, string:%s", vmgr_get_optype_name(op_type), drv_id, pcap_idx, pcap[pcap_idx].support_codec);

					//empty cap
					if (pcap[pcap_idx].support_codec == NULL) {
						detail_vmctx("%s:%u, IP:%s(%d) no more supported codecs", vmgr_get_optype_name(op_type), drv_id, vmgr_get_ip_name(mgr_idx), mgr_idx);
						break;
					}

					//The codec_id is the ID of the codec to be used. It finds the IP with the corresponding codec ID
					//search in support_codec string with name
					codec_name = vmgr_get_codec_name(codec_id);
					if (codec_name != NULL) {
						if (strstr(pcap[pcap_idx].support_codec, codec_name) != NULL) {
							unsigned long pixel_product = vmgr_get_accumulated_pixelproduct(mgr_ctx);
							detail_vmctx("%s:%u, found codec with type:%s(%d) in IP:%s(%d), pixel_product:%lu, min_pixel_product:%lu, same_ip_index:%d",
									vmgr_get_optype_name(op_type), drv_id, vmgr_get_codec_name(codec_id), codec_id, vmgr_get_ip_name(mgr_idx), mgr_idx, pixel_product, min_pixel_product, same_ip_index);

							if (forced_index >= 0) {
								if (forced_index == same_ip_index) {
									detail_vmctx("%s:%u, use force index:%d, selected vpu_ip:%s", vmgr_get_optype_name(op_type), drv_id, forced_index, vmgr_get_ip_name(mgr_idx));
									ip_type = mgr_idx;
									break;
								}

								//aassigns the index value of the IPs that support the same codec
								same_ip_index++;
							} else {
								//if multiple VPU IPs support the codec, the one with the most available resources is found and allocated
								//pixel_product is the currently used resource (width x height x fps),
								// and min_pixel_product is the smallest pixel product value found in the current search loop.
								if (pixel_product < min_pixel_product) {
									//found codec with IP
									ip_type = mgr_idx;
									min_pixel_product = pixel_product;

									dlog_vmctx("%s:%u, update codec with type:%s(%d) in IP:%s(%d), pixel_product:%lu, min_pixel_product:%lu",
										vmgr_get_optype_name(op_type), drv_id, vmgr_get_codec_name(codec_id), codec_id, vmgr_get_ip_name(mgr_idx), mgr_idx, pixel_product, min_pixel_product);
								} else {
									detail_vmctx("%s:%u, skip this codec", vmgr_get_optype_name(op_type), drv_id);
								}
							}
						}
					}
				}

				if (forced_index >= 0) {
					if (ip_type != VPU_IP_UNKNOWN) {
						detail_vmctx("%s:%u, already found vpu_ip", vmgr_get_optype_name(op_type), drv_id);
						break;
					}
				}
			}
		}
	}

	if (ip_type > VPU_IP_UNKNOWN) {
		dlog_vmctx("%s:%u, choose IP:%s with codec:%s(%d)", vmgr_get_optype_name(op_type), drv_id, vmgr_get_ip_name(ip_type), vmgr_get_codec_name(codec_id), codec_id);
	} else {
		err_vmctx("%s:%u, could not find codec type:%s(%d)", vmgr_get_optype_name(op_type), drv_id, vmgr_get_codec_name(codec_id), codec_id);
	}

	vetc_mutex_unlock(&vpumgr_mutex);

	return ip_type;
}

EXPORT_SYMBOL(vmgr_find_ip_type);

int vmgr_get_capability(enum vpu_op_type op_type, vpu_capability_t *capability)
{
	int mgr_idx;
	int pcap_idx;
	vpu_mgr_t *mgr_ctx = NULL;

	unsigned int cap_idx = 0U;

	vetc_mutex_lock(&vpumgr_mutex);
	for (mgr_idx = 0; mgr_idx < VPU_IP_MAX; mgr_idx++) {
		mgr_ctx = vpu_mgr_array[mgr_idx];
		if (mgr_ctx != NULL) {
			vpu_ip_cap_t *pcap = NULL;

			dlog_vmctx("in IP:%s(%d)", vmgr_get_ip_name(mgr_idx), mgr_idx);

			//search in capabilities
			if (op_type == VPU_OP_TYPE_DEC) {
				pcap = mgr_ctx->each_ip->dec_capa;
				//dlog_vmctx("IP:%s(%d) get dec capa:%p", vmgr_get_ip_name(mgr_idx), mgr_idx, pcap);
			} else {
				pcap = mgr_ctx->each_ip->enc_capa;
				//dlog_vmctx("IP:%s(%d) get enc capa:%p", vmgr_get_ip_name(mgr_idx), mgr_idx, pcap);
			}

			if (pcap != NULL) {
				for (pcap_idx = 0; pcap_idx < MAX_SUPPORT_CODEC; pcap_idx++) {
					//dlog_vmctx("pcap idx:%d, string:%s", pcap_idx, pcap[pcap_idx].support_codec);

					if (pcap[pcap_idx].support_codec != NULL) {
						//copy each caps
						detail_vmctx("cap_idx:%d, support_codec:%s", pcap_idx, pcap[pcap_idx].support_codec);
						capability->codec_caps[cap_idx].codec_id = pcap[pcap_idx].codec_id;
						(void)vetc_strncpy(capability->codec_caps[cap_idx].support_codec, pcap[pcap_idx].support_codec, VPU_V3_MAX_CAP_STR - 1);
						(void)vetc_strncpy(capability->codec_caps[cap_idx].support_profile, pcap[pcap_idx].support_profile, VPU_V3_MAX_CAP_STR - 1);
						(void)vetc_strncpy(capability->codec_caps[cap_idx].support_level, pcap[pcap_idx].support_level, VPU_V3_MAX_CAP_STR - 1);
						capability->codec_caps[cap_idx].max_width = pcap[pcap_idx].max_width;
						capability->codec_caps[cap_idx].max_height = pcap[pcap_idx].max_height;
						capability->codec_caps[cap_idx].max_fps = pcap[pcap_idx].max_fps;
						capability->codec_caps[cap_idx].support_buffer_mode = mgr_ctx->each_ip->buffer_mode;
						cap_idx++;
					} else {
						break;
					}
				}
			}

			//dlog_vmctx("couldn't find in IP:%s(%d) codec type:%s(%d), try next IP", vmgr_get_name(mgr_idx), mgr_idx, vmgr_get_codec_name(codecType), codecType);
		}
	}

	vetc_mutex_unlock(&vpumgr_mutex);

	capability->num_of_codec_cap = cap_idx;
	return 0;
}

EXPORT_SYMBOL(vmgr_get_capability);

int vmgr_set_context(const enum vpu_ip_type ip_type, void *ctx)
{
	int ret = 0;

	if ((ip_type > VPU_IP_UNKNOWN) && (ip_type < VPU_IP_MAX)) {
		vetc_mutex_lock(&vpumgr_mutex);
		vpu_mgr_array[ip_type] = (vpu_mgr_t *)ctx;
		detail_vmctx("ip:%s(%d), context:0x%x", vmgr_get_ip_name(ip_type), ip_type, ctx);
		vetc_mutex_unlock(&vpumgr_mutex);
	} else {
		ret = -1;
	}

	return ret;
}

EXPORT_SYMBOL(vmgr_set_context);

void *vmgr_get_context(const enum vpu_ip_type ip_type)
{
	vpu_mgr_t *ctx = NULL;
	if ((ip_type > VPU_IP_UNKNOWN) && (ip_type < VPU_IP_MAX)) {
		vetc_mutex_lock(&vpumgr_mutex);
		ctx = vpu_mgr_array[ip_type];
		vetc_mutex_unlock(&vpumgr_mutex);
	}

	return ctx;
}

EXPORT_SYMBOL(vmgr_get_context);

