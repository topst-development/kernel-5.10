// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/memblock.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/of.h>
#include <linux/of_fdt.h>
#include <linux/of_address.h>
#include <linux/of_reserved_mem.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/dma-mapping.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/version.h>

#ifdef CONFIG_PROC_FS
#include <linux/list_sort.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#endif

#ifdef CONFIG_OPTEE
#include <linux/tee_drv.h>
#include <linux/time.h>
#endif

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
MODULE_IMPORT_NS(DMA_BUF);
#endif

#include "vpu_comm.h"
#include "vpu_devices.h"
#include "vpu_rm.h"
// This header must located after "vpu_comm.h".
#include <dt-bindings/pmap/common/vpu_mem_size.h>

#undef VLOG_TAG
#define VLOG_TAG "[vrm]"

#define VRM_FREED                (~((u64)0U))
#define VRM_DATA_NA              (~((u32)0U))

#ifdef CONFIG_PROC_FS
#define PROC_VRM_CMD_GET         (0x1001)
#define PROC_VRM_CMD_RELEASE     (0x1002)

static struct mutex vrm_mutex;
static char gMemConfigDone;	// = 0;

static const char *vname[MAX_VRM_VMEM] = {
	"video",                 // 00
	"video_rear",            // 01
	"video_ext",             // 02
	"video_ext_rear",        // 03
	"video_ext2",            // 04
	"enc_main",              // 05
	"enc_ext",               // 06
	"enc_ext2",              // 07
	"enc_ext3",              // 08
	"enc_ext4",              // 09
	"enc_ext5",              // 10
	"enc_ext6",              // 11
	"enc_ext7",              // 12
	"enc_ext8",              // 13
	"enc_ext9",              // 14
	"enc_ext10",             // 15
	"enc_ext11",             // 16
	"enc_ext12",             // 17
	"enc_ext13",             // 18
	"enc_ext14",             // 19
	"enc_ext15",             // 20
	"video_sw"               // 21
};

enum vnidx {
	video = 0,               // 00
	video_rear,              // 01
	video_ext,               // 02
	video_ext_rear,          // 03
	video_ext2,              // 04
	enc_main,                // 05
	enc_ext,                 // 06
	enc_ext2,                // 07
	enc_ext3,                // 08
	enc_ext4,                // 09
	enc_ext5,                // 10
	enc_ext6,                // 11
	enc_ext7,                // 12
	enc_ext8,                // 13
	enc_ext9,                // 14
	enc_ext10,               // 15
	enc_ext11,               // 16
	enc_ext12,               // 17
	enc_ext13,               // 18
	enc_ext14,               // 19
	enc_ext15,               // 20
	video_sw                 // 21
};

static const char *vswmname[MAX_VRM_VSW] = {
	"D_USERDATA_0",          // 00
	"D_USERDATA_1",          // 01
	"D_USERDATA_2",          // 02
	"D_USERDATA_3",          // 03
	"D_USERDATA_4",          // 04
	"E_SEQ_H_0",             // 05
	"E_SEQ_H_1",             // 06
	"E_SEQ_H_2",             // 07
	"E_SEQ_H_3",             // 08
	"E_SEQ_H_4",             // 09
	"E_SEQ_H_5",             // 10
	"E_SEQ_H_6",             // 11
	"E_SEQ_H_7",             // 12
	"E_SEQ_H_8",             // 13
	"E_SEQ_H_9",             // 14
	"E_SEQ_H_10",            // 15
	"E_SEQ_H_11",            // 16
	"E_SEQ_H_12",            // 17
	"E_SEQ_H_13",            // 18
	"E_SEQ_H_14",            // 19
	"E_SEQ_H_15",            // 20
	"WAVE420L",              // 21
	"WAVE512",               // 22
	"WAVE410",               // 23
	"G2V2_VP9",              // 24
	"JPU",                   // 25
	"CODA960",               // 26
	"WAVE420L2"              // 27
};

enum vswmidx {
	d_userdata_0 = 0,        // 00
	d_userdata_1,            // 01
	d_userdata_2,            // 02
	d_userdata_3,            // 03
	d_userdata_4,            // 04
	e_seq_h_0,               // 05
	e_seq_h_1,               // 06
	e_seq_h_2,               // 07
	e_seq_h_3,               // 08
	e_seq_h_4,               // 09
	e_seq_h_5,               // 10
	e_seq_h_6,               // 11
	e_seq_h_7,               // 12
	e_seq_h_8,               // 13
	e_seq_h_9,               // 14
	e_seq_h_10,              // 15
	e_seq_h_11,              // 16
	e_seq_h_12,              // 17
	e_seq_h_13,              // 18
	e_seq_h_14,              // 19
	e_seq_h_15,              // 20
	wave420l,                // 21
	wave512,                 // 22
	wave410,                 // 23
	g2v2_vp9,                // 24
	jpu,                     // 25
	coda960,                 // 26
	wave420l2                // 27
};

struct user_vrm {
	char name[VRM_NAME_LEN];
	// FIXME: Update the type to u64 with user-level library.
	u32 base;
	u32 size;
};
#endif

static bool vrm_list_sorted;
static u64 vrm_total_size;

struct vpmap_entry {
	struct vpmap info;
	struct vpmap_entry *parent;
	struct list_head list;
};

struct vrm_entry {
	struct vrm info;
	struct vrm_entry *parent;
	struct list_head list;
};

struct free_entry {
	u32 used;
	void *addr;
};

#ifdef CONVERT_VPU_INSTANCE_NUM_FROM_VRM
struct vrm_vpu_inst_used {
	u32 used; //instance used flag (1:used)
	u32 idx;  //instance index
	u32 vrm_idx;  //index received from vrm
};
#define VRM_ENC_MAX 16
#define VRM_DEC_MAX 5
static struct vrm_vpu_inst_used gs_stEncUsed[VRM_ENC_MAX];
static struct vrm_vpu_inst_used gs_stDecUsed[VRM_DEC_MAX];
#endif

static struct vpmap_entry vrm_tb[MAX_VRMS];
static struct vpmap_entry secure_area_tb[MAX_VRM_GROUPS];

static struct vrm_entry vmem_tb[MAX_VRM_VMEM];
static struct vrm_entry vswm_tb[MAX_VRM_VSW];

static struct free_entry vfree_tb[VPU_MAX][MAX_FREE_VA];

static LIST_HEAD(vpmap_list_head);
static LIST_HEAD(vrm_list_head);

static inline struct vpmap_entry *vpmap_entry_of(struct vpmap *info)
{
	return container_of(info, struct vpmap_entry, info);
}

static inline struct vrm_entry *vrm_entry_of(struct vrm *info)
{
	return container_of(info, struct vrm_entry, info);
}

static void *vrm_get_va(phys_addr_t pa, u32 size)
{
	void *va = NULL;
	u64 i = 0;

	if (size < UINT_MAX) {
		i = size + (u64)pa;
		V_DBG(VPU_DBG_MEMORY, "physical region [0x%x - 0x%x]!!", pa, i);
		va = (void *) vetc_ioremap((phys_addr_t) pa, PAGE_ALIGN(size));
		if (va == NULL) {
			V_DBG(VPU_DBG_ERROR, "fail to ioremap for 0x%x w/ %u.",
					pa, size);
		}
	}

	return va;
}

static void vrm_release_va(void *va, phys_addr_t pa, u64 size)
{
	u64 i = 0;

	if (size <= UINT_MAX) {
		i = size + (u64)pa;
		V_DBG(VPU_DBG_MEMORY, "physical region [0x%x - 0x%x]!!", pa, i);

		iounmap(va);
	}
}

/**
 * @brief Get the string representation of the given VPU type.
 *
 * This function returns the string representation of the provided VPU type.
 *
 * @param type The VPU type.
 * @return The string representation of the VPU type. If the type is not recognized, "UNKNOWN" is returned.
 */
char *vpu_vputype_to_string(vputype type)
{
	static char *type_strings[] = {
		"vpu_vdec",
		"vpu_vdec_ext",
		"vpu_vdec_ext2",
		"vpu_vdec_ext3",
		"vpu_vdec_ext4",
		"vpu_venc",
		"vpu_venc_ext",
		"vpu_venc_ext2",
		"vpu_venc_ext3",
		"vpu_venc_ext4",
		"vpu_venc_ext5",
		"vpu_venc_ext6",
		"vpu_venc_ext7",
		"vpu_venc_ext8",
		"vpu_venc_ext9",
		"vpu_venc_ext10",
		"vpu_venc_ext11",
		"vpu_venc_ext12",
		"vpu_venc_ext13",
		"vpu_venc_ext14",
		"vpu_venc_ext15",
		"UNKNOWN"
	};

	if (type >= VPU_DEC && type < VPU_MAX)
		return type_strings[type];
	else
		return type_strings[VPU_MAX];
}


/*
 * Search for vpmap info. from telechips,pmap-name in the reserved memory
 * area of DT in pmap linked list.
 * param : name (= telechips,pmap-name defined in node)
 *
 */
static struct vpmap *vrm_find_info_by_name(const char *name)
{
	struct vpmap_entry *entry = NULL;

	list_for_each_entry(entry, &vpmap_list_head, list) {
		if (entry != NULL) {
			int cmp = strncmp(name, entry->info.name, VRM_NAME_LEN);
			if (cmp == 0) {
				return &entry->info;
			}
		}
	}

	return NULL;
}

/*
 * Search for vrm index from name defined in vname list.
 *
 * param : name (= string in vname struct)
 *
 */
static s32 vrm_find_vidx_by_name(const char *name)
{
	s32 i = -1;
	for (i = 0; i < (s32)MAX_VRM_VMEM; i++) {
		int cmp = strncmp(name, vname[i], VRM_NAME_LEN);
		if (cmp == 0) {
			V_DBG(
				VPU_DBG_MEMORY,
				"[DEBUG][VRM] name=%s, index=%d.",
				name,
				i
			);
			break;
		}
	}

	return (i >= (s32)MAX_VRM_VMEM) ? -1 : i;
}

/*
 * Search for vrm info from index defined in vnidx list.
 *
 * param : idx (= index in vnidx enum struct)
 *
 */
static struct vrm *vrm_find_vinfo_by_vidx(s32 idx)
{
	struct vrm_entry *entry = NULL;
	struct vrm *res = NULL;

	if ((idx >= (s32)video) && (idx < (s32)MAX_VRM_VMEM)) {
		entry = &vmem_tb[idx];
		if (entry->info.base != VRM_FREED) {
			V_DBG(
				VPU_DBG_MEMORY,
				"[DEBUG][VRM] name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=0x%p, groups=%lu, flags=%u, rc=%u.",
				entry->info.name,
				entry->info.size,
				entry->info.used,
				entry->info.base,
				entry->info.pa,
				entry->info.va,
				entry->info.groups,
				entry->info.flags,
				entry->info.rc
			);
			res = &entry->info;
		} else {
			V_DBG(VPU_DBG_ERROR, "Invaild vpu info.");
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "Invaild index (%d).", idx);
	}
	return res;
}

/*
 * Search for vrm info from index defined in vswmidx enum list.
 *
 * param : idx (= index in vswmidx enum struct)
 *
 */
static struct vrm *vrm_find_vswminfo_by_vidx(s32 idx)
{
	struct vrm_entry *entry = NULL;
	struct vrm *res = NULL;

	if ((idx >= 0) && (idx < (s32)MAX_VRM_VSW)) {
		entry = &vswm_tb[idx];
		if (entry->info.base != VRM_FREED) {
			V_DBG(
				VPU_DBG_MEMORY,
				"[DEBUG][VRM] name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=%p, groups=%u, flags=%u, rc=%u.",
				entry->info.name,
				entry->info.size,
				entry->info.used,
				entry->info.base,
				entry->info.pa,
				entry->info.va,
				entry->info.groups,
				entry->info.flags,
				entry->info.rc
			);
			res = &entry->info;
		} else {
			V_DBG(VPU_DBG_ERROR, "Invaild vswm info.");
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "Invaild index (%d).", idx);
	}
	return res;
}

/*
 * Search for size from index (for IP supported or for encoder or decoder)
 * defined in vswmidx enum list.
 * param : idx (= index in vswmidx enum struct)
 *
 */
static u32 vrm_find_vswm_size_by_idx(s32 idx)
{
	u32 size = 0U;
	u32 uidx = 0U;
	u32 eseqh0 = (s32)e_seq_h_0;
	u32 dusrdat0 = (s32)d_userdata_0;
	u32 maxcnt_enc = (s32)VPU_ENC_MAX_CNT;
	u32 maxcnt_dec = (s32)VPU_INST_MAX;

	if ((idx >= 0) && (idx < (s32)MAX_VRM_VSW)) {
		uidx = (u32)idx;
		switch (uidx) {
		case (u32)wave420l:
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
			size = VPU_HEVC_ENC_WORK_BUF_SIZE;
#endif
			break;
		case (u32)wave420l2:
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_2ND_VPU_HEVC_ENC
			size = VPU_HEVC_ENC_WORK_BUF_SIZE;
#endif
			break;
		case (u32)wave512:
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
			size = WAVExxx_WORK_BUF_SIZE;
#endif
			break;
		case (u32)wave410:
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
			size = WAVExxx_WORK_BUF_SIZE;
#endif
			break;
		case (u32)g2v2_vp9:
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
			size = G2V2_VP9_WORK_BUF_SIZE;
#endif
			break;
		case (u32)jpu:
#ifdef CONFIG_SUPPORT_TCC_JPU
			size = JPU_WORK_BUF_SIZE;
#endif
			break;
		case (u32)coda960:
			size = VPU_WORK_BUF_SIZE;
			break;
		default:
			if (maxcnt_enc == 0) {
				maxcnt_enc = 1;
			}
			if ((uidx >= eseqh0) && (uidx < (eseqh0 + maxcnt_enc))) {
				size = VPU_ENC_HEADER_BUF_SIZE;
			} else if ((uidx >= dusrdat0) && (uidx < (dusrdat0 + maxcnt_dec))) {
				size = USER_DATA_BUF_SIZE;
			} else {
				VPU_DONOTHING(uidx);
			}

			break;
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "Invaild index (%d).", idx);
		//size = -EINVAL;  <- this value is something wrong, why the returen value is nagative??
		size = -EINVAL;
	}
	return size;
}

/*
 * Search for index defined in vswmidx enum list
 * from video codec index.
 * param : idx (= video codec index defined in TCCxxxx_VPU_CIDEC_COMMON.h)
 *
 */
static s32 vrm_find_ipidx_from_codec(s32 codec)
{
	enum vswmidx idx;
	s32 res = -EFAULT;

	if ((codec >= STD_AVC) && (codec <= STD_HEVC_ENC2)) { //STD_AVC 0 , STD_HEVC_ENC2 18
		switch (codec) {
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_VPU_HEVC_ENC
		case STD_HEVC_ENC:
			idx = wave420l;
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE420L_2ND_VPU_HEVC_ENC
		case STD_HEVC_ENC2:
			idx = wave420l2;
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_JPU
		case STD_MJPG:
			idx = jpu;
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE410_HEVC
		case STD_HEVC:
			idx = wave410;
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_WAVE512_4K_D2
		case STD_HEVC:
		case STD_VP9:
			idx = wave512;
			break;
#endif
#ifdef CONFIG_SUPPORT_TCC_G2V2_VP9
		case STD_VP9:
			idx = g2v2_vp9;
			break;
#endif
		default:
			idx = coda960;
			break;
		}
		res = (s32)idx;
	} else {
		V_DBG(VPU_DBG_ERROR, "Invaild codec index (%d).", codec);
	}

	return res;
}

static inline u64 vrm_get_base(struct vpmap *info)
{
	struct vpmap_entry *entry = vpmap_entry_of(info);
	struct vpmap_entry *iter = NULL;
	u64 base = 0U;
	bool escape = (bool)false;

	for (iter = entry; (iter != NULL) && (!escape); iter = iter->parent) {
		if (iter->info.base != VRM_FREED) {
			V_DBG(VPU_DBG_PMAP,
				  "[DEBUG][VRM] name=%s, size=%lu, base=0x%lx, iter_base=0x%lx, groups=%lu, flags=%u",
				  iter->info.name, iter->info.size, base,
				  iter->info.base, iter->info.groups,
				  iter->info.flags);

			if ((U64_MAX - base) >= iter->info.base) {
				base += iter->info.base;
			} else {
				/* XXX: Should not happen */
				base = 0U;
				escape = (bool)true;
			}
		} else {
			base = 0U;
			escape = (bool)true;
		}
	}

	return base;
}

static struct vrm_entry *vrm_alloc_vswm_from_base(struct vrm *info)
{
	u32 i;
	struct vrm_entry *entry;
	u64 pab = info->base;
	bool escape = (bool)false;

	for (i = 0; (i < (u32)MAX_VRM_VSW) && (!escape); i++) {
		u32 ui_size;
		entry = &vswm_tb[i];

		if (entry->info.va != NULL) {
			V_DBG(VPU_DBG_ERROR,
				"Is %u 'th va already remapped?", i);
			continue;
		}

		if (strncpy(entry->info.name, vswmname[i], VRM_NAME_LEN) != NULL) {
			entry->info.name[VRM_NAME_LEN - 1U] = '\0';

			entry->info.base = pab;
			entry->info.pa = entry->info.base;

			entry->info.groups = info->groups;
			entry->info.flags = info->flags;

			entry->info.size = vrm_find_vswm_size_by_idx((s32)i);

			if (entry->info.size == 0U) {
				continue;
			}

			if (entry->info.size <= UINT_MAX) {
				ui_size = (u32)entry->info.size;

				entry->info.va = vrm_get_va(
					(phys_addr_t)entry->info.base, ui_size);
				if (entry->info.va != NULL) {
					V_DBG(VPU_DBG_MEMORY,
							"%s (%d 'th) in VPU's FW memory succeeded in remapping va=0x%p w/ base=0x%lx, size=%u in video_sw area!!",
							entry->info.name, i,
							entry->info.va, pab,
							entry->info.size);

					if (pab > __UINT64_MAX__ - ((u64)entry->info.size)) {
						V_DBG(VPU_DBG_ERROR,
						  "the info size(%d) + info base(%d) is overflowed over UINT_MAX", (u64)entry->info.size, pab);
					} else {
						pab += (u64)entry->info.size;
					}
				} else {
					V_DBG(VPU_DBG_ERROR,
						  "VPU failed to get %u 'th virtual address in video_sw.", i);
					entry = NULL;
					escape = (bool)true;
				}
			} else {
				V_DBG(VPU_DBG_ERROR,
					  "the info size is overflowed over UINT_MAX", i);
				entry = NULL;
				escape = (bool)true;
			}

		} else {
			V_DBG(VPU_DBG_ERROR,
				  "VPU failed to get %u 'th name in video_sw.", i);
			entry = NULL;
			escape = (bool)true;
		}
	}

	return entry;
}

static s32 vrm_update_info(struct device_node *np, struct vpmap *info,
			   struct vrm *infov)
{
	s32 ret = 0;
	struct vpmap_entry *entry = vpmap_entry_of(info);
	struct vrm_entry *ventry = NULL;

	if (infov->base != VRM_FREED) {
		ventry = vrm_entry_of(infov);

		V_DBG(
			VPU_DBG_MEMORY,
			"[DEBUG][VRM] iname=%s, name=%s, isize=%lu, size=%lu, ibase=0x%lx, base=0x%lx, igroups=%u, groups=%u, iflags=%u, flags=%u",
			infov->name,
			ventry->info.name,
			infov->size,
			ventry->info.size,
			infov->base,
			ventry->info.base,
			infov->groups,
			ventry->info.groups,
			info->flags,
			ventry->info.flags
		);
	}

	if (vrm_is_secured(info)) {
		if (info->base >= entry->parent->info.base) {
			info->base -= entry->parent->info.base;
		} else {
			/* XXX: Should not happen */
			ret = -EFAULT;
		}

		if ((U64_MAX - entry->parent->info.size) >= info->size) {
			entry->parent->info.size += info->size;
		} else {
			/* XXX: Should not happen */
			ret = -EFAULT;
		}
	} else if (vrm_is_shared(info)) {
		struct device_node *sn;

		struct vpmap *shared;
		struct vrm *vshared;

		const char *name;
		const __be32 *prop;
		s32 len;
		s32 cells;
		u64 offset = 0;
		u64 size = 0;

		sn = of_parse_phandle(np, "telechips,pmap-shared", 0);
		ret = of_property_read_string(sn, "telechips,pmap-name", &name);
		if (ret != 0) {
			return -1;
		}

		shared = vrm_find_info_by_name(name);
		if (shared == NULL) {
			return -1;
		}

		prop = of_get_property(np, "telechips,pmap-offset", &len);
		cells = of_n_addr_cells(np);
		if ((prop != NULL) && (cells == (len/(s32)sizeof(u32)))) {
			offset = of_read_number(prop, of_n_addr_cells(np));
		}

		prop = of_get_property(np, "telechips,pmap-shared-size", &len);
		cells = of_n_size_cells(np);
		if ((prop != NULL) && (cells == (len/(s32)sizeof(u32)))) {
			size = of_read_number(prop, of_n_size_cells(np));
		}

		if ((ventry != NULL) && vrm_is_vstored(info)) {
			s32 idx = vrm_find_vidx_by_name(name);
			if ((idx < 0) || (idx >= (s32)MAX_VRM_VMEM)) {
				return -1;
			}

			vshared = vrm_find_vinfo_by_vidx(idx);
			if (vshared == NULL) {
				return -1;
			}

			ventry->info.base = vshared->base + vshared->size;
			ventry->info.pa = ventry->info.base;
			ventry->info.size = size;
			ventry->info.flags |= VRM_FLAG_VREAREND;

			if ((size > 0U) && vrm_is_vshared(vshared)) {
				struct vrm *vnext =
					vrm_find_vinfo_by_vidx(idx + 1);
				if (vnext == NULL) {
					return -1;
				}

				ventry->info.sidx[idx] = 1;
				ventry->info.sidx[idx + 1] = 1;

				idx = vrm_find_vidx_by_name(
					ventry->info.name);
				if (idx < 0) {
					return -1;
				}

				vshared->sidx[idx] = 1;

				vnext->base = vshared->base + offset;
				vnext->pa = vnext->base;
				vnext->size = offset;

				V_DBG(
					VPU_DBG_MEMORY,
					"[DEBUG][VRM] idx=%d, name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=%p, groups=%u, rc=%u, flags=%u",
					idx,
					vnext->name,
					vnext->size,
					vnext->used,
					vnext->base,
					vnext->pa,
					vnext->va,
					vnext->groups,
					vnext->rc,
					vnext->flags
				);
			}

			V_DBG(
				VPU_DBG_MEMORY,
				"[DEBUG][VRM] sname=%s, vname=%s, iname=%s, ssize=%lu, vsize=%lu, isize=%lu, sbase=0x%lx, vbase=0x%lx, ibase=0x%lx, offset=0x%lx, pa=0x%lx, va=%p, used=%u, sgroups=%u, vgroups=%u, igroups=%u, sflags=%u, vflags=%u, iflags=%u\n",
				name,
				ventry->info.name,
				info->name,
				shared->size,
				ventry->info.size,
				info->size,
				shared->base,
				ventry->info.base,
				info->base,
				offset,
				ventry->info.pa,
				ventry->info.va,
				ventry->info.used,
				shared->groups,
				ventry->info.groups,
				info->groups,
				shared->flags,
				ventry->info.flags,
				info->flags
			);
		}

		entry->parent = vpmap_entry_of(shared);
		info->base = offset;
		info->size = size;

		V_DBG(
			VPU_DBG_PMAP,
			"[DEBUG][VRM] name=%s, size=%lu, base=0x%lx, groups=%lu, flags=%u",
			info->name,
			info->size,
			info->base,
			info->groups,
			info->flags
		);
	} else {
		if ((ventry != NULL) &&
			vrm_is_vstored(info) &&
			vrm_is_vswed(info)) {
			ventry->parent = vrm_alloc_vswm_from_base(infov);

			if (ventry->parent == NULL) {
				return -1;
			}

			V_DBG(
				VPU_DBG_MEMORY,
				"[DEBUG][VRM] name=%s, vname=%s, size=%lu, vsize=%lu, base=0x%lx, vbase=0x%lx, groups=%u, vgroups=%u, flags=%u, vflags=%u",
				entry->info.name,
				ventry->info.name,
				entry->info.size,
				ventry->info.size,
				entry->info.base,
				ventry->info.base,
				entry->info.groups,
				ventry->info.groups,
				entry->info.flags,
				ventry->info.flags
			);
		}
	}

	/* Remove pmap with size 0 from entry list */
	if (entry->info.size == 0U) {
		list_del(&entry->list);
	}

	return ret;
}

static s32 vrm_get_info_internal(struct vpmap *info)
{
	const struct vpmap_entry *entry = vpmap_entry_of(info);

	while (entry->parent != NULL) {
		info = &entry->parent->info;
		entry = vpmap_entry_of(info);
	}

	if (info->rc < UINT_MAX) {
		++info->rc;
	}

	return 1;
}

static void vrm_release_info_internal(struct vpmap *info)
{
	while (info->rc != 0U) {
		const struct vpmap_entry *entry = vpmap_entry_of(info);

		if (entry->parent != NULL) {
			info = &entry->parent->info;
		} else {
			break;
		}
	}

	if (info->rc > 0U) {
		--info->rc;
	}
}

s32 vrm_get_info(const char *name, struct vpmap *mem)
{
	s32 ret;
	struct vpmap *info = vrm_find_info_by_name(name);

	if (mem != NULL) {
		mem->base = 0;
		mem->size = 0;
	}

	if ((mem != NULL) && (info != NULL)) {
		ret = vrm_get_info_internal(info);

		if (ret != 0) {
			(void)memcpy(mem, info, sizeof(struct vpmap));
			mem->base = vrm_get_base(info);

			ret = 1;
		} else {
			ret = -1;
		}
	} else {
		ret = 0;
	}
	return ret;
}
EXPORT_SYMBOL(vrm_get_info);

s32 vrm_release_info(const char *name)
{
	struct vpmap *info = vrm_find_info_by_name(name);
	s32 ret = 0;

	if (info != NULL) {
		vrm_release_info_internal(info);
		ret = 1;
	} else {
		ret = 0;
	}

	return ret;
}
EXPORT_SYMBOL(vrm_release_info);

 static s32 vrm_alloc_procmem(
			s32 codec,
			MEM_ALLOC_INFO_t *alloc_info,
			vputype type
		)
{
	struct vrm *info = NULL;
	MEM_ALLOC_INFO_t *uinfo = NULL;

	s32 bufidx = 0;
	s32 idx = (s32) type;
	s32 ipidx = vrm_find_ipidx_from_codec(codec);

	if (alloc_info == NULL) {
		V_DBG(VPU_DBG_ERROR, "info. to allocate memory is wrong.");
		return -EFAULT;
	}

	if ((ipidx < (s32)wave420l) || (ipidx > (s32)wave420l2)) {
		V_DBG(
			VPU_DBG_ERROR,
			"codec type[%d] is wrong and ip[%d] is not set for dev idx[%d].",
			codec,
			ipidx,
			idx
		);
		return -EFAULT;
	}

	if ((idx < (s32)VPU_DEC) || (idx >= (s32)VPU_MAX)) {
		V_DBG(VPU_DBG_ERROR, "dev idx[%d] is wrong.", idx);
		return -EFAULT;
	}

	V_DBG(
		VPU_DBG_MEMORY,
		" [DEBUG][VRM] run-time memory allocation w/ ip[%d], buffer[%d], dev idx[%d] enter!",
		ipidx,
		alloc_info->buffer_type,
		idx
	);

	uinfo = alloc_info;
	bufidx = (s32) uinfo->buffer_type;

	switch (bufidx) {
	case (s32)BUFFER_WORK:
		info = vrm_find_vswminfo_by_vidx(ipidx);
		break;

	case (s32)BUFFER_USERDATA:     // for decoder
	case (s32)BUFFER_SEQHEADER:    // for encoder
		info = vrm_find_vswminfo_by_vidx(idx);
		break;

	default:
		info = vrm_find_vinfo_by_vidx(idx);
		break;
	}

	if (info == NULL) {
		return -EFAULT;
	}

	if (vrm_is_vrearend(info)) {
		info->ip = (u32) ipidx;
		info->pa -= uinfo->request_size;
		info->used += uinfo->request_size;

		uinfo->phy_addr =
			(phys_addr_t)((info->pa > UINT_MAX) ? UINT_MAX :
									info->pa);
	} else {
		uinfo->phy_addr =
			(phys_addr_t)((info->pa > UINT_MAX) ? UINT_MAX :
									info->pa);

		switch (bufidx) {
		case (s32)BUFFER_WORK:
			if (info->rc == 0U) {
				V_DBG(
					VPU_DBG_MEM_USAGE,
					"Alloc work buffer ipdix :%d\n", ipidx);
			}
			break;
		case (s32)BUFFER_USERDATA: 		// for decoder
		case (s32)BUFFER_SEQHEADER: 	// for encoder
			info->pa += info->size;
			info->used += info->size;
			break;
		default:
			info->ip = (u32) ipidx;
			info->pa += uinfo->request_size;
			info->used += uinfo->request_size;
			break;
		}
	}

	if (uinfo->phy_addr == 0U) {
		V_DBG(
			VPU_DBG_ERROR,
			"[DEBUG][VRM] vpu-%d failed to get the physical memory (0x%x) for buffer type-%d.",
			idx,
			uinfo->phy_addr,
			bufidx
		);

		return -EFAULT;
	}

	if (vrm_is_vswed(info)) {
		uinfo->request_size =
			(unsigned int)((info->size > UINT_MAX) ? UINT_MAX :
									   info->size);
	} else {
		s32 framebuffertype = (s32)BUFFER_FRAMEBUFFER;

		if (bufidx != framebuffertype) {
			struct free_entry *entry = NULL;
			s32 i = 0;

			info->va = vrm_get_va(
					uinfo->phy_addr,
					uinfo->request_size
				 );
			if (info->va == NULL) {
				return -EFAULT;
			}

			for (i = 0; i < MAX_FREE_VA; i++) {
				entry = &vfree_tb[idx][i];

				if (entry->used == 0U) {
					entry->used = 1U;
					entry->addr = info->va;
					break;
				}
			}
		}
	}

	uinfo->kernel_remap_addr = info->va;

	if (info->rc < UINT_MAX) {
		if (vrm_is_vswed(info)) {
			++info->rc;
		} else {
			info->rc = 1U;
		}
	}

	V_DBG(
		VPU_DBG_MEM_USAGE,
		"[DEBUG][VRM] buf type=%d, name=%s, size=%lu, usize=%lu, used=%lu, base=0x%lx, pa=0x%lx, upa=0x%lx, va=0x%p, uva=0x%p, groups=%u, flags=%u, rc=%u, codec=%d.",
		uinfo->buffer_type,
		info->name,
		info->size,
		uinfo->request_size,
		info->used,
		info->base,
		info->pa,
		uinfo->phy_addr,
		info->va,
		uinfo->kernel_remap_addr,
		info->groups,
		info->flags,
		info->rc,
		info->ip
	);

	return 0;
}

static s32 vrm_free_procmem(vputype type)
{
	s32 idx = (s32) type;
	u32 ip = 0;
	s32 i = 0;

	struct vrm *info = NULL;
	struct vrm *swinfo = NULL;
	struct free_entry *entry = NULL;

	if ((idx < (s32)VPU_DEC) || (idx >= (s32)VPU_MAX)) {
		return -EFAULT;
	}

	info = vrm_find_vinfo_by_vidx(idx);
	if (info == NULL) {
		return -EFAULT;
	}

	V_DBG(
		VPU_DBG_MEM_USAGE,
		"[DEBUG][VRM] name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=0x%p, groups=%u, flags=%u, rc=%u, ip=%u.",
		info->name,
		info->size,
		info->used,
		info->base,
		info->pa,
		info->va,
		info->groups,
		info->flags,
		info->rc,
		info->ip
	);

	if (info->rc == 0U) {
		V_DBG(
			VPU_DBG_INFO,
			"dev idx[%d] is already freed (rc=%u).",
			idx,
			info->rc
		);
		return 0;
	}

	ip = info->ip;

	for (i = 0; i < (s32)MAX_FREE_VA; i++) {
		entry = &vfree_tb[idx][i];

		if (entry->used == 1U) {
			entry->used = 0U;
			(void) vrm_release_va(entry->addr, (phys_addr_t)((info->pa > UINT_MAX) ? UINT_MAX :
									info->pa),
									info->used);
		}
	}

	if (vrm_is_vrearend(info)) {
		info->pa += info->used;
		info->used = 0;
	} else {
		info->pa -= info->used;
		info->used = 0;
	}

	info->va = NULL;

	if (info->base != info->pa) {
		V_DBG(
			VPU_DBG_ERROR,
			"the memory address is wrong for dev idx[%d] (base=0x%lx != pa=0x%lx).",
			idx,
			info->base,
			info->pa
		);
		return -EINVAL;
	}

	if (ip == VRM_DATA_NA) {
		V_DBG(VPU_DBG_ERROR, "video ip is not set for vpu-%d.", idx);
		return -EFAULT;
	}

	if ((ip > 0U) && (ip < MAX_VRM_VSW)) {
		info->ip = VRM_DATA_NA;
	}

	if (info->rc > 0U) {
		--info->rc;
	}

	/*
	* To free video_sw memory used for one of encoders or decoders.
	*/
	swinfo = vrm_find_vswminfo_by_vidx((s32)idx);
	if (swinfo == NULL) {
		return -EFAULT;
	}

	V_DBG(
		VPU_DBG_MEM_USAGE,
		"[[DEBUG][VRM] name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=0x%p, groups=%u, flags=%u, rc=%u, ip=%u.",
		swinfo->name,
		swinfo->size,
		swinfo->used,
		swinfo->base,
		swinfo->pa,
		swinfo->va,
		swinfo->groups,
		swinfo->flags,
		swinfo->rc,
		swinfo->ip
	);
#if 0
	if (swinfo->rc == 0U) {
		V_DBG(VPU_DBG_ERROR, "vswm has not used for vpu-%d.", idx);
		return -EFAULT;
	}

	if (swinfo->rc > 0U) {
		--swinfo->rc;
		if (swinfo->rc > 0U) {
			/* should not happen */
			V_DBG(VPU_DBG_ERROR,
				"the same vswm is used for other vpu device.");
			return EFAULT;
		}
	}

	swinfo->pa -= swinfo->used;
	swinfo->used -= swinfo->size;

	if ((swinfo->base != swinfo->pa) ||
		(swinfo->used != 0U)) {
		V_DBG(
			VPU_DBG_ERROR,
			"vswm-%d failed to be freed (base=0x%lx, pa=0x%lx, size=%lu, used=%lu).",
			idx,
			swinfo->base,
			swinfo->pa,
			swinfo->size,
			swinfo->used
		);
		return -EFAULT;
	}
#else
	if (swinfo->rc != 0U) {
		if (swinfo->rc > 0U) {
			--swinfo->rc;
			if (swinfo->rc > 0U) {
				/* should not happen */
				V_DBG(VPU_DBG_ERROR,
						"the same vswm is used for other vpu device.");
				return EFAULT;
			}
		}

		swinfo->pa -= swinfo->used;
		swinfo->used -= swinfo->size;

		if ((swinfo->base != swinfo->pa) ||
				(swinfo->used != 0U)) {
			V_DBG(
					VPU_DBG_ERROR,
					"vswm-%d failed to be freed (base=0x%lx, pa=0x%lx, size=%lu, used=%lu).",
					idx,
					swinfo->base,
					swinfo->pa,
					swinfo->size,
					swinfo->used
				 );
			return -EFAULT;
		}
	}
#endif
	/*
	 * To free video ip used.
	 */
	swinfo = vrm_find_vswminfo_by_vidx(
		((ip > INT_MAX_U) ? INT_MAX_S : ((int)ip)));
	if (swinfo == NULL) {
		return -EFAULT;
	}

	V_DBG(
		VPU_DBG_MEM_USAGE,
		"[DEBUG][VRM] name=%s, size=%lu, base=0x%lx, va=0x%p, groups=%u, flags=%u, rc=%u, ip=%u.",
		swinfo->name,
		swinfo->size,
		swinfo->base,
		swinfo->va,
		swinfo->groups,
		swinfo->flags,
		swinfo->rc,
		swinfo->ip
	);
#if 0
	if (swinfo->rc == 0U) {
		V_DBG(VPU_DBG_ERROR, "video ip  has not used for vpu-%d.", idx);
		return -EFAULT;
	}

	if (swinfo->rc > 0U) {
		--swinfo->rc;
		if (swinfo->rc > 0U) {
			V_DBG(VPU_DBG_ERROR,
				"video ip is used for other vpu device.");
			return 0;
		}
	}

	swinfo->pa -= swinfo->used;
	swinfo->used -= swinfo->size;

	if ((swinfo->base != swinfo->pa) ||
	   (swinfo->used != 0U)) {
		V_DBG(
			VPU_DBG_ERROR,
			"vswm-%d failed to be freed (base=0x%lx, pa=0x%lx, size=%lu, used=%lu).",
			idx,
			swinfo->base,
			swinfo->pa,
			swinfo->size,
			swinfo->used
		);
		return -EFAULT;
	}

#else
	if (swinfo->rc != 0U) {
		if (swinfo->rc > 0U) {
			--swinfo->rc;
			if (swinfo->rc > 0U) {
				V_DBG(VPU_DBG_INFO,
						"video ip is used for other vpu device.");
				return 0;
			}
		}

		swinfo->used = 0LLU;

		if ((swinfo->base != swinfo->pa) ||
				(swinfo->used != 0U)) {
			V_DBG(
					VPU_DBG_ERROR,
					"vswm-%d failed to be freed (base=0x%lx, pa=0x%lx, size=%lu, used=%lu).",
					idx,
					swinfo->base,
					swinfo->pa,
					swinfo->size,
					swinfo->used
				 );
			return -EFAULT;
		}
	}

#endif
	return 0;
}

s32 vrm_get_freemem(s32 idx)
{
	s64 freed = 0;
	s32 ret = 0;
	struct vrm *vshared = NULL;

	struct vrm *info = vrm_find_vinfo_by_vidx(idx);
	if (info != NULL) {
		// [Coverity]It's hard to cast when it's calculated...
		freed = (info->size - info->used);
		if (vrm_is_vshared(info) || vrm_is_vrearend(info)) {
			for (idx = 0; idx < (s32)MAX_VRM_VMEM; idx++) {
				if (info->sidx[idx] == 1) {
					vshared = vrm_find_vinfo_by_vidx(idx);
					if (vshared != NULL) {
						if (freed >= vshared->used) {
							freed -= vshared->used;
						} else {
							freed = -1;
						}
					} else {
						freed = -1;
					}
				}
			}
		}

		if (freed < 0) {
			ret = -ENOMEM;
		} else {
			ret = (s32)freed;
		}
	} else {
		ret = -EFAULT;
	}
	return ret;
}

s32 vrm_alloc_count(s32 idx)
{
	struct vrm *info;
	s32 ret, ivideo, ivideo_sw;

	ivideo = (s32)video;
	ivideo_sw = (s32)video_sw;

	if ((idx >= ivideo) && (idx < ivideo_sw)) {
		info = vrm_find_vinfo_by_vidx(idx);

		if (info != NULL) {
			if (info->rc < UINT_MAX) {
				ret = (int)info->rc;
			} else {
				ret = -EFAULT;
			}
		} else {
			ret = -EFAULT;
		}
	} else {
		ret = 0;
	}
	return ret;
}

s32 vrm_get_instance(s32 idx)
{
	s32 i, ret, nInstance = -1;
	s32 imax_vrm_vmem = (s32)MAX_VRM_VMEM;
	struct vrm *info;

	if ((idx >= 0) && (idx < imax_vrm_vmem)) {
		if (vrm_get_freemem(idx) >= 0) {
			info = vrm_find_vinfo_by_vidx(idx);
			if (info != NULL) {
				if ((info->rc == 0U) && (info->reserved != 0U)) {
					V_DBG(VPU_DBG_ERROR, "name:%s, idx:%d, rc is %u, but already reversed: %u",
							info->name, idx, info->rc, info->reserved);
				}
				if ((info->rc == 0U) && (info->reserved == 0U)) {
					nInstance = idx;
					info->reserved = 1U;
				} else {
					for (i = 0; i < imax_vrm_vmem; i++) {
						info = vrm_find_vinfo_by_vidx(i);
						if (info != NULL) {
							if ((info->rc == 0U) && (info->reserved != 0U)) {
								V_DBG(VPU_DBG_ERROR, "name:%s, idx:%d, rc is %u, but already reversed: %u",
										info->name, idx, info->rc, info->reserved);
							}
							if ((info->rc == 0U) && (info->reserved == 0U)) {
								nInstance = i;
								info->reserved = 1U;
								break;
							}
						}
					}
				}

				V_DBG(VPU_DBG_INSTANCE, "Instance-#%d is taken (required-#%d).",
						idx, nInstance);

				ret = nInstance;
			} else {
				ret = -EFAULT;
			}
		} else {
			V_DBG(VPU_DBG_ERROR,
					"vpu failed to get new instance for decoder.");
			ret = -ENOMEM;
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "Invaild instance number.");
		ret = -EFAULT;
	}
	return ret;
}

unsigned int vrm_check_instance_available(void)
{
	s32 i;
	s32 imax_vrm_vmem = (s32)MAX_VRM_VMEM;
	u32 freed = 0;
	struct vrm *info;

	for (i = 0; i < imax_vrm_vmem; i++) {
		info = vrm_find_vinfo_by_vidx(i);
		if (info != NULL) {
			if (info->rc == 0U) {
				if (freed < __UINT32_MAX__) {
					freed++;
				}
			}
		}
	}

	V_DBG(VPU_DBG_INSTANCE, "there are #%d instance numbers available.", freed);

	return freed;
}

static void vrm_clear_instance(s32 idx)
{
	struct vrm *info;
	s32 imax_vrm_vmem = (s32)MAX_VRM_VMEM;

	if ((idx >= 0) && (idx < imax_vrm_vmem)) {
		info = vrm_find_vinfo_by_vidx(idx);
		if (info != NULL) {
			if (info->rc != 0U) {
				V_DBG(VPU_DBG_ERROR,
						"vpu failed to clear instance-#%d for decoder.",
						idx);
			}
			info->reserved = 0U;
			V_DBG(VPU_DBG_INSTANCE, "vpu instance-#%d is cleared.", idx);
		}
	} else {
		V_DBG(VPU_DBG_ERROR, "Invaild instance number.");
	}
}
//*/

#define vrm_get_order(info) \
	(((info)->groups == VRM_DATA_NA) ? 2 : (vrm_is_shared(info) ? 1 : 0))
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
static int vrm_compare(void *p, const struct list_head *a, const struct list_head *b)
#else
static int vrm_compare(void *p, struct list_head *a, struct list_head *b)
#endif
{
	struct vpmap *pa = &list_entry(a, struct vpmap_entry, list)->info;
	struct vpmap *pb = &list_entry(b, struct vpmap_entry, list)->info;

	s32 order_a;
	s32 order_b;

	u64 base_a;
	u64 base_b;

	if ((pa == NULL) || (pb == NULL)) {
		/* XXX: Should not happen */
		V_DBG (VPU_DBG_ERROR, "[DEBUG][VRM] p=0x%lx", p);
		return 0;
	}

	order_a = vrm_get_order(pa);
	order_b = vrm_get_order(pb);

	if (order_a != order_b) {
		return (order_a < order_b) ? -1 : 1;
	}

	base_a = vrm_get_base(pa);
	base_b = vrm_get_base(pb);

	return (base_a <= base_b) ? -1 : 1;
}

#undef vrm_get_order

static int proc_vrm_show(struct seq_file *m, void *v)
{
	struct vpmap_entry *entry = NULL;

	s32 shared_info = 0;
	s32 secured_info = 0;

	LOG_COVERITY("%p", v);

	if (vrm_list_sorted == (bool)false) {	//if (!vrm_list_sorted) {
		list_sort(NULL, &vpmap_list_head, &vrm_compare);
		vrm_list_sorted = (bool)true;
	}

	/* format:   -10s       -10s        3s  3s    s */
	seq_puts(m, "base_addr  virt_addr          size       used       flags    ref   shared   name\n");

	list_for_each_entry(entry, &vpmap_list_head, list) {
		struct vrm *vinfo = NULL;
		struct vpmap *info = &entry->info;

		s32 i;
		s32 imax_vrm_vmem = (s32)MAX_VRM_VMEM;
		int is_vshared  = 0;
		//char *is_vshared = vrm_is_vshared(info) ? "*" : " ";

		u64 base = vrm_get_base(info);

		if (info != NULL) {
			is_vshared = vrm_is_vshared(info) ? 1 : 0;
		}

		if (base == 0U) {
			continue;
		}

		if ((shared_info == 0) && vrm_is_shared(info)) {
			seq_puts(m, " ======= Shared Area Info. =======\n");
			shared_info = 1;
		}

		if ((secured_info == 0) && (info->groups == VRM_DATA_NA)) {
			seq_puts(m, " ======= Secured Area Info. =======\n");
			secured_info = 1;
		}

		if ((secured_info == 0) && vrm_is_vstored(info)) {
			s32 idx = vrm_find_vidx_by_name(info->name);
			if ((idx < 0) || (idx >= imax_vrm_vmem)) {
				return -1;
			}

			vinfo = vrm_find_vinfo_by_vidx(idx);
			if (vinfo == NULL) {
				return -1;
			}

			is_vshared = 0;
			if (vrm_is_vshared(vinfo)) {
				is_vshared = 1;
			}
			if (vrm_is_vrearend(vinfo)) {
				is_vshared  = 1;
			}

			seq_printf(m,
				"0x%8llx 0x%16p 0x%8.8llx 0x%8.8llx   %-3u     %-3u    %s      %s\n",
				vinfo->base,
				vinfo->va,
				vinfo->size,
				vinfo->used,
				vinfo->flags,
				vinfo->rc,
				(is_vshared == 1) ? "*" : " ",
				vinfo->name
			);

			if (vrm_is_vshared(vinfo)) {
				vinfo = vrm_find_vinfo_by_vidx(idx + 1);
				if (vinfo == NULL) {
					return -1;
				}

				is_vshared = vrm_is_vrearend(vinfo) ? 1 : 0;

				seq_printf(m,
					"0x%8llx 0x%16p 0x%8.8llx 0x%8.8llx   %-3u     %-3u    %s      %s\n",
					vinfo->base,
					vinfo->va,
					vinfo->size,
					vinfo->used,
					vinfo->flags,
					vinfo->rc,
					(is_vshared == 1) ? "*" : " ",
					vinfo->name
				);
			}
		} else {
			seq_printf(m, "0x%8llx 0x%16llx 0x%8.8llx 0x%8.8llx   %-3u     %-3u    %s      %s\n",
				base,
				0LLU,
				info->size,
				0LLU,
				info->flags,
				info->rc,
				(is_vshared == 1) ? "*" : " ",
				info->name
			);
		}

		if (vrm_is_vswed(info)) {
			seq_puts(m, " --- Enter video_sw Area Info. ---\n");

			for (i = 0; i < MAX_VRM_VSW; i++) {
				vinfo = vrm_find_vswminfo_by_vidx(i);
				if (vinfo == NULL) {
					return -EFAULT;
				}

				if (vinfo->size == 0U) {
					continue;
				}

				seq_printf(m,
					"0x%8llx 0x%16p 0x%8.8llx 0x%8.8llx   %-3u     %-3u    %s      %s\n",
					vinfo->base,
					vinfo->va,
					vinfo->size,
					vinfo->used,
					vinfo->flags,
					vinfo->rc,
					(is_vshared == 1) ? "*" : " ",
					vinfo->name
				);
			}

			seq_puts(m, " --- Out   video_sw Area Info. ---\n");
		}
	}

	seq_puts(m, " ======= Total Area Info. =======\n");
	seq_printf(m, "0x00000000                    0x%8.8llx (total)\n", vrm_total_size);

	return 0;
}

static int proc_vrm_open(struct inode *st_inode, struct file *st_file)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
	return single_open(st_file, &proc_vrm_show, st_inode->i_private);
#else
	return single_open(st_file, &proc_vrm_show, PDE_DATA(st_inode));
#endif
}

static long proc_vrm_ioctl(struct file *f, unsigned int cmd, unsigned long arg)
{
	struct user_vrm __user *uinfo = (struct user_vrm __user *) arg;
	struct user_vrm ret_info;
	struct vpmap info;
	s32 ret = -1;
	ulong copied;

	LOG_COVERITY("%p", f);

	copied = copy_from_user(&ret_info, uinfo, sizeof(struct user_vrm));
	if (copied == 0UL) {
		switch (cmd) {
		case PROC_VRM_CMD_GET:
			ret = vrm_get_info(ret_info.name, &info);

			ret_info.base = (info.base > UINT_MAX) ? UINT_MAX : (unsigned int) info.base;
			ret_info.size = (info.size > UINT_MAX) ? UINT_MAX : (unsigned int) info.size;
			break;
		case PROC_VRM_CMD_RELEASE:
			ret = vrm_release_info(ret_info.name);
			break;
		default:
			/* Nothing to do */
			break;
		}

		copied = copy_to_user(uinfo, &ret_info, sizeof(struct user_vrm));
		if (copied != 0UL) {
			ret =  -1;
		}
	} else {
		ret = -1;
	}
	return ret;
}

static const struct file_operations vdev_rm_fops = {
	.owner              = THIS_MODULE,
	.open               = proc_vrm_open,
	.read               = seq_read,
	.llseek             = seq_lseek,
	.release            = single_release,
	.unlocked_ioctl     = proc_vrm_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl       = proc_vrm_ioctl,
#endif
};

static struct miscdevice vrm_misc_device = {
	MISC_DYNAMIC_MINOR,
	VRM_NAME,
	&vdev_rm_fops,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
static const struct proc_ops vrm_fops = {
	.proc_open = proc_vrm_open,
	.proc_read = seq_read,
	.proc_lseek = seq_lseek,
	.proc_release = single_release,
};
#endif

int vrm_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;

	struct vpmap_entry *entry;
	struct vrm_entry *ventry;

#if defined(CONFIG_PROC_FS)
	struct proc_dir_entry *dir;
#endif
	static s32 num_vrms_left = (s32) MAX_VRMS;

	u32 i;

	vrm_list_sorted = (bool)false;
	vrm_total_size = (u64)0U;

	V_DBG(VPU_DBG_MEM_SEQ, "[DEBUG][VRM] enter");

#if defined(CONFIG_PROC_FS)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
	dir = proc_create("vrm", 0x124, NULL, &vrm_fops);	//MISRA C-2012 Literals and Constants (MISRA C-2012 Rule 7.1) 1. misra_c_2012_rule_7_1_violation: Octal constant 0444 used.
#else
	dir = proc_create("vrm", 0x124, NULL, &vdev_rm_fops);	//MISRA C-2012 Literals and Constants (MISRA C-2012 Rule 7.1) 1. misra_c_2012_rule_7_1_violation: Octal constant 0444 used.
#endif
	if (dir == NULL) {
		return -ENOMEM;
	}
#endif

	/*
	 * Initialize vpmap, secure area and vpu tables as 0  and set initial value for
	 * each secure area.
	 */
	(void)memset(vrm_tb, 0, sizeof(vrm_tb));
	(void)memset(secure_area_tb, 0, sizeof(secure_area_tb));

	(void)memset(vmem_tb, 0, sizeof(vmem_tb));
	(void)memset(vswm_tb, 0, sizeof(vswm_tb));

	(void)memset(vfree_tb, 0, sizeof(vfree_tb));

	for (i = 0; i < MAX_VRM_GROUPS; i++) {
		entry = &secure_area_tb[i];
		(void)sprintf(entry->info.name, "secure_area%d", (s32)i + 1);
		entry->info.base = VRM_FREED;
		entry->info.groups = VRM_DATA_NA;
	}

	for (i = 0; i < MAX_VRM_VMEM; i++) {
		ventry = &vmem_tb[i];
		ventry->info.base = VRM_FREED;
		ventry->info.groups = VRM_DATA_NA;
	}

	for (i = 0; i < MAX_VRM_VSW; i++) {
		ventry = &vswm_tb[i];
		ventry->info.base = VRM_FREED;
		ventry->info.groups = VRM_DATA_NA;
		ventry->info.ip = VRM_DATA_NA;
	}

	#ifdef CONVERT_VPU_INSTANCE_NUM_FROM_VRM
	for (i = 0; i < VRM_DEC_MAX; i++) {
		gs_stDecUsed[i].used = 0;
		gs_stDecUsed[i].idx = 0;
		gs_stDecUsed[i].vrm_idx = 0;
	}

	for (i = 0; i < VRM_ENC_MAX; i++) {
		gs_stEncUsed[i].used = 0;
		gs_stEncUsed[i].idx = 0;
		gs_stEncUsed[i].vrm_idx = 0;
	}
	#endif

	/*
	 * Add reg property for each device tree node if there are only
	 * size property with dynamic allocation.
	 */
	for_each_compatible_node(np, NULL, "telechips,pmap") {

		struct reserved_mem *rmem;

		const char *name;
		s32 ret;

		u64 base;
		u64 size;
		//phys_addr_t end;

		const __be32 *prop;

		//s32 len;
		u64 string_max_length;
		u64 string_copy_length;

		u64 groups = 0;
		u32 flags = 0;
		s32 sort;
		s32 ivideo_sw;
		u64 vpu_sw_access_region_size;

		ivideo_sw = (s32)video_sw;
		vpu_sw_access_region_size = (u64)VPU_SW_ACCESS_REGION_SIZE;

		ret = of_property_read_string(np, "telechips,pmap-name", &name);
		if (ret != 0) {
			continue;
		}

		rmem = of_reserved_mem_lookup(np);
		if (rmem == NULL) {
			V_DBG(VPU_DBG_MEM_SEQ, "%s failed to read!", name);
			continue;
		}

		base = rmem->base;
		size = rmem->size;

		//if ((rmem->size > 0U) && ((ULONG_MAX - rmem->base) >= rmem->size)) {
		//	end = rmem->base + rmem->size - 1U;
		//} else {
		//	end = rmem->base;
		//}

#ifdef CONFIG_PMAP_SECURED
		/*
		 * When CONFIG_PMAP_SECURED is not set, simply ignore "telechips,
		 * pmap-secured" property.
		 */
		prop = of_get_property(np, "telechips,pmap-secured", &len);
		if (prop != NULL) {
			groups = of_read_number(prop, len/MAX_VRM_GROUPS);
			if ((groups >= 1U) && (groups <= MAX_VRM_GROUPS)) {
				flags |= VRM_FLAG_SECURED;
			}
		}
#endif
		prop = of_get_property(np, "telechips,pmap-shared", NULL);
		if (prop != NULL) {
			flags |= VRM_FLAG_SHARED;
		}

		sort = vrm_find_vidx_by_name(name);
		if ((sort >= 0) && (sort < MAX_VRM_VMEM)) {
			int ivideo, ivideo_ext;

			ivideo = video;
			ivideo_ext = video_ext;
			flags |= VRM_FLAG_VSTORED;

			if ((sort == ivideo) || (sort == ivideo_ext)) {
				flags |= VRM_FLAG_VSHARED;
			} else if (sort == ivideo_sw) {
				flags |= VRM_FLAG_VSWED;
			} else {
				VPU_DONOTHING(sort);
			}
		}

		if ((sort == ivideo_sw) && (size < vpu_sw_access_region_size)) {
			V_DBG(
				VPU_DBG_ERROR,
				"video_sw (%d) doesn't have enough memory (size=0x%lx, VPU_SW_ACCESS_REGION_SIZE=0x%lx, Dec-instance=%d, Enc-instance=%d.",
				sort,
				size,
				vpu_sw_access_region_size, //VPU_SW_ACCESS_REGION_SIZE,
				VPU_INST_MAX,
				VPU_ENC_MAX_CNT
			);
			return -EINVAL;
		}

		/* Initialize pmap and register it into pmap list */
		if (num_vrms_left == 0) {
			return -ENOMEM;
		}

		entry = &vrm_tb[(s32)MAX_VRMS - num_vrms_left];
		--num_vrms_left;

		string_max_length = sizeof(entry->info.name) - 1;
		string_copy_length = strlen(name) < string_max_length ? strlen(name) : string_max_length;

		if (string_copy_length > 0) {
			if (strncpy(entry->info.name, name, string_copy_length) == NULL) {
				V_DBG(VPU_DBG_ERROR,
					"VPU failed to get %s", entry->info.name);
				return -EFAULT;
			}
			entry->info.name[string_copy_length] = '\0';
		}

		entry->info.name[VRM_NAME_LEN-1] = '\0';
		entry->info.base = base;
		entry->info.size = size;
		entry->info.groups = (u32) groups;
		entry->info.rc = 0;
		entry->info.flags = flags;

		list_add_tail(&entry->list, &vpmap_list_head);

		/*
		 * Register secure area as a parent for each secured pmap and
		 * calculate base address of each secure area.
		 */
		if (vrm_is_secured(&entry->info)) {
			s32 group_idx = (s32)entry->info.groups - 1;

			entry->parent = &secure_area_tb[group_idx];
			entry->parent->info.flags |= entry->info.flags;

			if (entry->info.base < entry->parent->info.base) {
				entry->parent->info.base = entry->info.base;
			}

			V_DBG(
				VPU_DBG_MEM_SEQ,
				"[DEBUG][VRM] name=%s, size=%lu, base=0x%lx, gbase=0x%lx, groups=%lu, ggroups=%lu, group_idx=%d, flags=%u, gflags=%u.",
				entry->info.name,
				entry->info.size,
				entry->info.base,
				entry->parent->info.base,
				entry->info.groups,
				entry->parent->info.groups,
				group_idx,
				entry->info.flags,
				entry->parent->info.flags
			);
		}

		/*
		 * Register secure area as a parent for each secured pmap and
		 * calculate base address of each secure area.
		 */
		if (vrm_is_vstored(&entry->info)) {
			ventry = &vmem_tb[sort];

			(void)strncpy(ventry->info.name, name, VRM_NAME_LEN-1);
			entry->info.name[VRM_NAME_LEN-1] = '\0';
			ventry->info.base = entry->info.base;
			ventry->info.size = entry->info.size;
			ventry->info.pa = ventry->info.base;
			ventry->info.groups = entry->info.groups;
			ventry->info.flags |= entry->info.flags;

			if (vrm_is_vshared(&entry->info)) {
				ventry->parent = &vmem_tb[sort + 1];

				ventry->info.sidx[sort + 1] = 1;
				ventry->parent->info.sidx[sort] = 1;

				(void)strncpy(ventry->parent->info.name,
					vname[sort + 1], VRM_NAME_LEN);
				ventry->parent->info.name[VRM_NAME_LEN-1] = '\0';
				ventry->parent->info.base =
					(ventry->info.size > __UINT64_MAX__ - (ventry->info.base)) ? __UINT64_MAX__ : (u64)(ventry->info.base + ventry->info.size) ;
				ventry->parent->info.size = ventry->info.size;
				ventry->parent->info.pa = ventry->parent->info.base;
				ventry->parent->info.groups = ventry->info.groups;
				ventry->parent->info.flags |= ventry->info.flags;
				ventry->parent->info.flags |= VRM_FLAG_VREAREND;

				V_DBG(
					VPU_DBG_MEM_SEQ,
					"[DEBUG][VRM] sort_idx=%d, name=%s, size=%lu, used=%lu, base=0x%lx, pa=0x%lx, va=%p, groups=%u, rc=%u, flags=%u.",
					sort,
					ventry->parent->info.name,
					ventry->parent->info.size,
					ventry->parent->info.used,
					ventry->parent->info.base,
					ventry->parent->info.pa,
					ventry->parent->info.va,
					ventry->parent->info.groups,
					ventry->parent->info.rc,
					ventry->parent->info.flags
				);
			}

			V_DBG(
				VPU_DBG_MEM_SEQ,
				"[DEBUG][VRM] name=%s, vname=%s, size=%lu, vsize=%lu, vused=%lu, base=0x%lx, vbase=0x%lx, vpa=0x%lx, vva=%p, groups=%u, vgroups=%u, sort_idx=%d, rc=%u, vrc=%u, flags=%u, vflags=%u.",
				entry->info.name,
				ventry->info.name,
				entry->info.size,
				ventry->info.size,
				ventry->info.used,
				entry->info.base,
				ventry->info.base,
				ventry->info.pa,
				ventry->info.va,
				entry->info.groups,
				ventry->info.groups,
				sort,
				entry->info.rc,
				ventry->info.rc,
				entry->info.flags,
				ventry->info.flags
			);
		}

		if ((U64_MAX - vrm_total_size) >= entry->info.size) {
			vrm_total_size += entry->info.size;
		} else {
			/* XXX: Should not happen */
		}

		/* Update info with pmap related properties */
		ret = vrm_update_info(np, &entry->info, &ventry->info);

		if (ret != 0) {
			V_DBG(
				VPU_DBG_ERROR,
				"[DEBUG][VRM] name=%s, size=%lu, base=0x%lx, groups=%u, flags=%u.",
				entry->info.name,
				entry->info.size,
				entry->info.base,
				entry->info.groups,
				entry->info.flags
			);
			return -ENOMEM;
		}
	}

	/*
	 * Register secure areas with size > 0 into pmap list to print
	 * out later when user requests.
	 */
	for (i = 0; i < MAX_VRM_GROUPS; i++) {
		entry = &secure_area_tb[i];
		if (entry->info.size != 0U) {
			list_add_tail(&entry->list, &vpmap_list_head);
		}
	}

	V_DBG(
		VPU_DBG_MEM_SEQ,
		"[DEBUG][VRM] Done - total reserved %llu bytes (%lluK, %lluM).",
		vrm_total_size,
		vrm_total_size >> 10U,
		vrm_total_size >> 20U
	);

	if (misc_register(&vrm_misc_device) > 0) {
		dev_err(&pdev->dev, "VPU RM: failed to register misc device.\n");
		return -ENOMEM;
	} else {

		return 0;
	}
}
EXPORT_SYMBOL(vrm_probe);

VREMOVE_RET_TYPE vrm_remove(struct platform_device *pdev)
{
	V_DBG(VPU_DBG_INFO, "[DEBUG][VRM] vrm_mutex destroy pt:%p", pdev);
	(void)mutex_destroy(&vrm_mutex);
	misc_deregister(&vrm_misc_device);

	VREMOVE_RETURN();
}
EXPORT_SYMBOL(vrm_remove);

//
/*
 * Specify an external function to interface with user-space.
 * For reference, consider compatibility with the existing vpu_buffer.c
 * vmem_proc_alloc_memory, vmem_proc_free_memory,
 * vmem_get_free_memory
 */
int vmem_proc_alloc_memory(
			int codec_type,
			MEM_ALLOC_INFO_t *alloc_info,
			vputype type
		)
{
	s32 ret = 0;
	(void)mutex_lock(&vrm_mutex);
	ret = vrm_alloc_procmem(codec_type, alloc_info, type);
	if (ret < 0) {
		ret = -ENOMEM;
	}

	(void)mutex_unlock(&vrm_mutex);
	return ret;
}
EXPORT_SYMBOL(vmem_proc_alloc_memory);

int vmem_proc_free_memory(vputype type)
{
	s32 ret = 0;
	(void)mutex_lock(&vrm_mutex);
	ret = vrm_free_procmem(type);
	if (ret < 0) {
		ret = -ENOMEM;
	}
	(void)mutex_unlock(&vrm_mutex);
	return ret;
}
EXPORT_SYMBOL(vmem_proc_free_memory);

unsigned int vmem_get_free_memory(vputype type)
{
	unsigned int ret = 0U;
	s32 idx;
	s32 freed = 0;

	if (type < VPU_MAX) {
		idx = (s32)type;

		(void)mutex_lock(&vrm_mutex);
		freed = vrm_get_freemem(idx);
		if (freed < 0) {
			ret = (unsigned int) -ENOMEM; //FIXME : unsigned int return ???
		} else {
			ret = (unsigned int) freed;
		}
		(void)mutex_unlock(&vrm_mutex);

	} else {
		ret = (unsigned int) -ENOMEM; //FIXME : unsigned int return ???
	}
	return ret;
}
EXPORT_SYMBOL(vmem_get_free_memory);

pgprot_t vmem_get_pgprot(pgprot_t ulOldProt, unsigned long ulPageOffset)
{
	pgprot_t newProt;

	(void)mutex_lock(&vrm_mutex);
	newProt = pgprot_writecombine(ulOldProt);
	V_DBG(VPU_DBG_MEM_SEQ, "[DEBUG][VRM] (write-combine).%ul", ulPageOffset);
	(void)mutex_unlock(&vrm_mutex);
	return newProt;
}
EXPORT_SYMBOL(vmem_get_pgprot);

int vmem_alloc_count(int type)
{
	s32 alloced;
	(void)mutex_lock(&vrm_mutex);
	alloced = vrm_alloc_count(type);
	if (alloced < 0) {
		alloced = -EFAULT;
	}
	(void)mutex_unlock(&vrm_mutex);

	return (int)alloced;
}

/**
 * @brief Get the VPU device instance index.
 *
 * This function retrieves the instance index of the VPU device. It uses the
 * `vrm_get_instance` function to obtain the index. If the
 * `CONVERT_VPU_INSTANCE_NUM_FROM_VRM` macro is defined, it also converts the
 * obtained index based on an internal array, allowing for instance
 * numbering conversion.
 *
 * @param[in,out] nIdx A pointer to the desired instance index. Upon return,
 *                    this pointer will be updated with the converted instance index.
 */
void vdec_get_instance(int *nIdx)
{
	s32 idx = *nIdx;
	(void)mutex_lock(&vrm_mutex);
	*nIdx = vrm_get_instance(idx);

#ifdef CONVERT_VPU_INSTANCE_NUM_FROM_VRM
	if (*nIdx >= 0) {
		s32 i = 0;
		while (i < VRM_DEC_MAX) {
			if (gs_stDecUsed[i].used == 0) {
				gs_stDecUsed[i].used = 1;
				gs_stDecUsed[i].idx = i;
				gs_stDecUsed[i].vrm_idx = *nIdx;
				break;
			}
			i++;
		}

		if (i < VRM_DEC_MAX) {
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG " [%s][get instance] vrm:0x%X(%d) ==> inst:0x%X(%d)",
				vpu_vputype_to_string(gs_stDecUsed[i].idx),
				gs_stDecUsed[i].vrm_idx, gs_stDecUsed[i].vrm_idx,
				gs_stDecUsed[i].idx, gs_stDecUsed[i].idx);

			*nIdx = gs_stDecUsed[i].idx;
		}
	}
#endif
	(void)mutex_unlock(&vrm_mutex);
}
EXPORT_SYMBOL(vdec_get_instance);

void vdec_check_instance_available(unsigned int *szfreed)
{
	u32 freed = 0;

	(void)mutex_lock(&vrm_mutex);
	freed = vrm_check_instance_available();
	*szfreed = freed;
	(void)mutex_unlock(&vrm_mutex);
}
EXPORT_SYMBOL(vdec_check_instance_available);

/**
 * @brief Clear the VPU device instance index.
 *
 * This function clears the instance index of the VPU device. It uses the
 * `vrm_clear_instance` function to release the index. If the
 * `CONVERT_VPU_INSTANCE_NUM_FROM_VRM` macro is defined, it also handles the
 * internal array to clear the instance usage.
 *
 * @param[in] nIdx The instance index to be cleared.
 */
void vdec_clear_instance(int nIdx)
{
	(void)mutex_lock(&vrm_mutex);
#ifdef CONVERT_VPU_INSTANCE_NUM_FROM_VRM
	if ((nIdx >= 0) && (nIdx < VRM_DEC_MAX)) {
		int found = 0;
		int inst = 0;
		s32 i = 0;

		while (i < VRM_DEC_MAX) {
			if ((gs_stDecUsed[i].used == 1) && (gs_stDecUsed[i].idx == nIdx)) {
				nIdx = gs_stDecUsed[i].vrm_idx;
				inst = gs_stDecUsed[i].idx;
				found = 1;

				// Reset the internal array entry
				gs_stDecUsed[i].used = 0;
				gs_stDecUsed[i].vrm_idx = 0;
				gs_stDecUsed[i].idx = 0;

				break;
			}
			i++;
		}

		if (found == 0) {
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG " [%s][clear instance] inst:0x%X(%d) is not found",
				  vpu_vputype_to_string(nIdx), nIdx, nIdx);
		} else {
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG " [%s][clear instance] inst:0x%X(%d) ==> vrm:0x%X(%d)",
				  vpu_vputype_to_string(inst), inst, inst, nIdx, nIdx);
		}
	}
#endif
	(void)vrm_clear_instance(nIdx);
	(void)mutex_unlock(&vrm_mutex);
}
EXPORT_SYMBOL(vdec_clear_instance);

#ifdef CONVERT_VPU_INSTANCE_NUM_FROM_VRM
vputype vrm_get_dec_inst_to_vtype(int inst)
{
	int i = 0;
	while (i < VRM_DEC_MAX) {
		if (gs_stDecUsed[i].idx == inst) {
			return i;
		}
		i++;
	}
	return VPU_MAX;
}
EXPORT_SYMBOL(vrm_get_dec_inst_to_vtype);
vputype vrm_get_enc_inst_to_vtype(int inst)
{
	int i = 0;
	while (i < VRM_ENC_MAX) {
		if (gs_stEncUsed[i].idx == inst) {
			return i;
		}
		i++;
	}
	return VPU_MAX;
}
EXPORT_SYMBOL(vrm_get_enc_inst_to_vtype);
#endif

void venc_get_instance(int *nIdx)
{
	if (*nIdx < __UINT32_MAX__ - enc_main) {
		s32 idx = *nIdx + enc_main;
		(void)mutex_lock(&vrm_mutex);
		*nIdx = vrm_get_instance(idx);

#ifdef CONVERT_VPU_INSTANCE_NUM_FROM_VRM
		if (*nIdx >= 0) {
			s32 i = 0;
			while (i < VRM_ENC_MAX) {
				if (gs_stEncUsed[i].used == 0) {
					gs_stEncUsed[i].used = 1;
					gs_stEncUsed[i].idx = i;
					gs_stEncUsed[i].vrm_idx = *nIdx;
					break;
				}
				i++;
			}

			if (i < VRM_ENC_MAX) {
				V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG " [%s][get instance] vrm:0x%X(%d) ==> inst:0x%X(%d)]",
					vpu_vputype_to_string(gs_stEncUsed[i].idx),
					gs_stEncUsed[i].vrm_idx, gs_stEncUsed[i].vrm_idx,
					gs_stEncUsed[i].idx, gs_stEncUsed[i].idx);

				*nIdx = gs_stEncUsed[i].idx;
			}
		}
#endif

		(void)mutex_unlock(&vrm_mutex);
	} else {
		V_DBG(VPU_DBG_ERROR, VLOG_TAG " [%s][get instance] nIdx is overflowed. *nIdx:%d", *nIdx);
	}
}
EXPORT_SYMBOL(venc_get_instance);

void venc_check_instance_available(unsigned int *szfreed)
{
	unsigned int freed = 0;
	(void)mutex_lock(&vrm_mutex);
	freed = vrm_check_instance_available();
	*szfreed = freed;
	(void)mutex_unlock(&vrm_mutex);
}
EXPORT_SYMBOL(venc_check_instance_available);

void venc_clear_instance(int nIdx)
{
	(void)mutex_lock(&vrm_mutex);
#ifdef CONVERT_VPU_INSTANCE_NUM_FROM_VRM
	if ((nIdx >= 0) && (nIdx < VRM_ENC_MAX)) {
		int found = 0;
		int inst = 0;
		s32 i = 0;

		while (i < VRM_ENC_MAX) {
			if ((gs_stEncUsed[i].used == 1) && (gs_stEncUsed[i].idx == nIdx)) {
				nIdx = gs_stEncUsed[i].vrm_idx;
				inst = gs_stEncUsed[i].idx;
				found = 1;

				// Reset the internal array entry
				gs_stEncUsed[i].used = 0;
				gs_stEncUsed[i].vrm_idx = 0;
				gs_stEncUsed[i].idx = 0;

				break;
			}
			i++;
		}

		if (found == 0) {
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG " [%s][clear instance] inst:0x%X(%d) is not found",
				  vpu_vputype_to_string(nIdx), nIdx, nIdx);
		} else {
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG " [%s][clear instance] inst:0x%X(%d) ==> vrm:0x%X(%d)]",
				  vpu_vputype_to_string(inst), inst, inst, nIdx, nIdx);
		}
	}
#endif

	vrm_clear_instance(nIdx);
	mutex_unlock(&vrm_mutex);
}
EXPORT_SYMBOL(venc_clear_instance);

void vmem_set_only_decode_mode(int bDec_only)
{
	LOG_COVERITY("%d", bDec_only);
	return;
}
#if 0
int vmem_is_cma_allocated_virt_region(const void *start_virtaddr,
					unsigned int length)
{
	return 0;
}
#else
int vmem_is_cma_within_a_region(void *start_addr,
					void *end_addr,
					void *cmp_addr,
					unsigned int cmp_length)
{
#if 0
	if (cmp_addr != 0)
		if ((start_addr >= cmp_addr)
			&& (end_addr <= (cmp_addr + cmp_length - 1)))
			return 1;
#endif
	LOG_COVERITY("%p%p%p%d", start_addr, end_addr, cmp_addr, cmp_length);
	return 0;
}

int vmem_is_cma_within_a_region_cv(const void *start_addr,
					const void *end_addr,
					void *cmp_addr,
					unsigned int cmp_length)
{
#if 0
	if (cmp_addr != 0)
		if ((start_addr >= cmp_addr)
			&& (end_addr <= (cmp_addr + cmp_length - 1)))
			return 1;
#endif
	LOG_COVERITY("%p%p%p%d", start_addr, end_addr, cmp_addr, cmp_length);
	return 0;
}

int vmem_is_cma_allocated_virt_region(void *start_virtaddr,
					unsigned int length)
{
#if 0
	int i, type;
	void *end_virtaddr = (void *)start_virtaddr + length - 1;

	for (type = 0; type < VPU_MAX; type++) {
		if (vmem_allocated_count[type] > 0) {
			for (i = vmem_allocated_count[type]; i > 0; i--) {
				if (vmem_is_cma_within_a_region(
					start_virtaddr,
					end_virtaddr,
					vmem_alloc_info[type][i-1]
						.kernel_remap_addr,
					vmem_alloc_info[type][i-1]
						.request_size)) {
					if (vmem_is_cma_allocated_phy_region(
						vmem_alloc_info[type][i-1]
							.phy_addr,
						vmem_alloc_info[type][i-1]
							.request_size))
						return 1;
					else
						return 0;
				}
			}
		}
	}
#endif
	LOG_COVERITY("%p%d", start_virtaddr, length);
	return 0;
}

int vmem_is_cma_allocated_cv_virt_region(const void *start_virtaddr,
					unsigned int length)
{
#if 0
	int i, type;
	const void *end_virtaddr = start_virtaddr + length - 1;

	for (type = 0; type < VPU_MAX; type++) {
		if (vmem_allocated_count[type] > 0) {
			for (i = vmem_allocated_count[type]; i > 0; i--) {
				if (vmem_is_cma_within_a_region_cv(
					start_virtaddr,
					end_virtaddr,
					vmem_alloc_info[type][i-1]
						.kernel_remap_addr,
					vmem_alloc_info[type][i-1]
						.request_size)) {
					if (vmem_is_cma_allocated_phy_region(
						vmem_alloc_info[type][i-1]
							.phy_addr,
						vmem_alloc_info[type][i-1]
							.request_size))
						return 1;
					else
						return 0;
				}
			}
		}
	}
#endif
	LOG_COVERITY("%p%d", start_virtaddr, length);
	return 0;
}

int vmem_is_cma_allocated_phy_region(unsigned int start_phyaddr,
					  unsigned int length)
{
#if 0
	unsigned int end_phyaddr;
	unsigned long lsize = start_phyaddr + length;

	if (lsize > 0)
		lsize -= 1;

	if (lsize < UINT_MAX) {
		end_phyaddr = lsize;
	} else {
		V_DBG(VPU_DBG_ERROR,
			"end_phyaddr_range failed : start_addr=%u, length=%u", start_phyaddr, length);
		return 0;
	}

	// pmap_video
	if ((start_phyaddr >= pmap_video.base)
		&& (end_phyaddr <= (pmap_video.base + pmap_video.size - 1)))
		return pmap_is_cma_alloc(&pmap_video);

	// pmap_video_sw
	if ((start_phyaddr >= pmap_video_sw.base)
		&& (end_phyaddr <= (pmap_video_sw.base + pmap_video_sw.size - 1)))
		return pmap_is_cma_alloc(&pmap_video_sw);

#if DEFINED_CONFIG_VENC_CNT_1to16
	// pmap_enc
	if ((start_phyaddr >= pmap_enc.base)
		&& (end_phyaddr <= (pmap_enc.base + pmap_enc.size - 1)))
		return pmap_is_cma_alloc(&pmap_enc);
#endif

#if DEFINED_CONFIG_VDEC_CNT_345
	// pmap_video_ext
	if ((start_phyaddr >= pmap_video_ext.base)
		&& (end_phyaddr <= (pmap_video_ext.base + pmap_video_ext.size - 1)))
		return pmap_is_cma_alloc(&pmap_video_ext);
#endif

#if DEFINED_CONFIG_VDEC_CNT_5
	// pmap_video_ext2
	if ((start_phyaddr >= pmap_video_ext2.base)
		&& (end_phyaddr <=
		(pmap_video_ext2.base + pmap_video_ext2.size - 1)))
		return pmap_is_cma_alloc(&pmap_video_ext2);
#endif

#if DEFINED_CONFIG_VENC_CNT_2to16
	// pmap_enc_ext[0]
	if ((start_phyaddr >= pmap_enc_ext[0].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[0].base + pmap_enc_ext[0].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[0]);
#endif

#if DEFINED_CONFIG_VENC_CNT_3to16
	//pmap_enc_ext[1]
	if ((start_phyaddr >= pmap_enc_ext[1].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[1].base + pmap_enc_ext[1].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[1]);
#endif

#if DEFINED_CONFIG_VENC_CNT_4to16
	// pmap_enc_ext[2]
	if ((start_phyaddr >= pmap_enc_ext[2].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[2].base + pmap_enc_ext[2].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[2]);
#endif

#if DEFINED_CONFIG_VENC_CNT_5to16
	// pmap_enc_ext[3]
	if ((start_phyaddr >= pmap_enc_ext[3].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[3].base + pmap_enc_ext[3].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[3]);
#endif

#if DEFINED_CONFIG_VENC_CNT_6to16
	// pmap_enc_ext[4]
	if ((start_phyaddr >= pmap_enc_ext[4].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[4].base + pmap_enc_ext[4].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[4]);
#endif

#if DEFINED_CONFIG_VENC_CNT_7to16
	// pmap_enc_ext[5]
	if ((start_phyaddr >= pmap_enc_ext[5].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[5].base + pmap_enc_ext[5].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[5]);
#endif

#if DEFINED_CONFIG_VENC_CNT_8to16
	// pmap_enc_ext[6]
	if ((start_phyaddr >= pmap_enc_ext[6].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[6].base + pmap_enc_ext[6].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[6]);
#endif

#if DEFINED_CONFIG_VENC_CNT_9to16
	// pmap_enc_ext[7]
	if ((start_phyaddr >= pmap_enc_ext[7].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[7].base + pmap_enc_ext[7].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[7]);
#endif

#if DEFINED_CONFIG_VENC_CNT_10to16
	// pmap_enc_ext[8]
	if ((start_phyaddr >= pmap_enc_ext[8].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[8].base + pmap_enc_ext[8].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[8]);
#endif

#if DEFINED_CONFIG_VENC_CNT_11to16
	// pmap_enc_ext[9]
	if ((start_phyaddr >= pmap_enc_ext[9].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[9].base + pmap_enc_ext[9].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[9]);
#endif

#if DEFINED_CONFIG_VENC_CNT_12to16
	// pmap_enc_ext[10]
	if ((start_phyaddr >= pmap_enc_ext[10].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[10].base + pmap_enc_ext[10].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[10]);
#endif

#if DEFINED_CONFIG_VENC_CNT_13to16
	// pmap_enc_ext[11]
	if ((start_phyaddr >= pmap_enc_ext[11].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[11].base + pmap_enc_ext[11].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[11]);
#endif

#if DEFINED_CONFIG_VENC_CNT_14to16
	// pmap_enc_ext[12]
	if ((start_phyaddr >= pmap_enc_ext[12].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[12].base + pmap_enc_ext[12].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[12]);
#endif

#if DEFINED_CONFIG_VENC_CNT_15to16
	// pmap_enc_ext[13]
	if ((start_phyaddr >= pmap_enc_ext[13].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[13].base + pmap_enc_ext[13].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[13]);
#endif

#if DEFINED_CONFIG_VENC_CNT_16
	// pmap_enc_ext[14]
	if ((start_phyaddr >= pmap_enc_ext[14].base)
		&& (end_phyaddr <=
		(pmap_enc_ext[14].base + pmap_enc_ext[14].size - 1)))
		return pmap_is_cma_alloc(&pmap_enc_ext[14]);
#endif
#endif
	LOG_COVERITY("%d%d", start_phyaddr, length);
	return 0;
}
#endif
int vmem_init(void)
{
	LOG_COVERITY("%d", gMemConfigDone);
	return 0;
}

int vmem_config(void)
{
	if (gMemConfigDone == (char)0U) {
		V_DBG(VPU_DBG_INFO, "[DEBUG][VRM] vrm_mutex init");
		mutex_init(&vrm_mutex);
		gMemConfigDone = (char)1U;
	}
	return 0;
}

void vmem_deinit(void)
{
	LOG_COVERITY("%d", gMemConfigDone);
	return;
}

unsigned int vmem_get_freemem_size(vputype type)
{
	return vmem_get_free_memory(type);
}
//*/
