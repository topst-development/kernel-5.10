// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) Telechips Inc.
 */

#include "vpu_comm.h"

#ifdef CONFIG_SUPPORT_TCC_VPU

#include <linux/version.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/kthread.h>
#include <linux/uaccess.h>
#include <linux/time.h>
#include <linux/compat.h>

#include <linux/init.h>
#include <linux/fs.h>
#include <linux/vfs.h>
#include <linux/uaccess.h>
#include <linux/debugfs.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 1, 0)
#	include <soc/telechips/pmap.h>
#endif

#include "vpu_buffer.h"
#include "vpu_devices.h"
#include "vpu_mgr_sys.h"
#include "vpu_mgr.h"
#include "vpu_mgr_flexio.h"
#include "vpu_rm.h"

#define dprintk_vpu(msg...)  V_DBG(VPU_DBG_INFO,  "TCC_VPU_MGR: " msg)
#define detailk_vpu(msg...)  V_DBG(VPU_DBG_INFO,  "TCC_VPU_MGR: " msg)
#define cmdk_vpu(msg...)     V_DBG(VPU_DBG_INFO,  "TCC_VPU_MGR [Cmd]: " msg)
#define err_vpu(msg...)      V_DBG(VPU_DBG_ERROR, "TCC_VPU_MGR [Err]: " msg)

#undef VLOG_TAG
#define VLOG_TAG "[vmgr]"

//#define VPU_REGISTER_DUMP
// for interlaced format with fileplay-mode or all format w/ ring-mode

//For test purpose!!
//#define FORCED_ERROR
#ifdef FORCED_ERROR
#define FORCED_ERR_CNT 300
static int forced_error_count = FORCED_ERR_CNT;
#endif

/////////////////////////////////////////////////////////////////////////////
// Control only once!!

#define IS_VALID_VPU_TYPE(vpu_type) \
	(((vpu_type) >= VPU_DEC) && ((vpu_type) < (int)VPU_MAX))
struct vmgr_t {
	int vpu_vtype; //vpu type
	int vpu_inst_num; //vpu instance number
};

struct VpuList vmgr_vlist;

static struct mgr_data_t vmgr_data;
static struct task_struct *kidle_task;

static char fname_file[] = "file";

#if defined(USE_ACCESS_POINT)
// SHARE_POINT_ORDER_XXX :
//    VPU = 0, JPU = 1, HEVC = 2,
//    4KD2 = 3, HEVC_ENC = 4, HEVC_ENC_2 = 5
#   define SHARE_POINT_ORDER_VPU 0U

//Decoder
typedef int (*tccfp_vpu_dec_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_vpu_dec_t tcc_vpu_dec;
typedef int (*tccfp_vpu_dec_esc_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_vpu_dec_esc_t tcc_vpu_dec_esc;
typedef int (*tccfp_vpu_dec_ext_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
static tccfp_vpu_dec_ext_t tcc_vpu_dec_ext;

//Encoder
#	if DEFINED_CONFIG_VENC_CNT_1to16
	typedef int (*tccfp_vpu_enc_t)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	tccfp_vpu_enc_t tcc_vpu_enc;
#	endif

typedef struct st_vpu_func_t {
	unsigned int check_code1;
	int (*tccfp_vpu_dec)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code2;
	int (*tccfp_vpu_enc)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code3;
	int (*tccfp_vpu_dec_esc)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	int (*tccfp_vpu_dec_ext)(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2);
	unsigned int check_code4;
} st_vpu_func;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0)
static st_vpu_func stVPUFuncBase = {0, NULL, 0, NULL, 0, NULL, NULL, 0};
#endif

static st_vpu_func *stVPUFunc = INITIAL_NULL;

static int check_access_addr_valid(void)
{
	int ret = -1;

	if (((CHECK_CODE_01 | stVPUFunc->check_code1) == CHECK_CODE_01) &&
			((CHECK_CODE_02 | stVPUFunc->check_code2) == CHECK_CODE_02) &&
			((CHECK_CODE_03 | stVPUFunc->check_code3) == CHECK_CODE_03) &&
			((CHECK_CODE_04 | stVPUFunc->check_code4) == CHECK_CODE_04)) {
		ret = 0;
	} else {
		V_DBG(VPU_DBG_ERROR, "VPU CheckCode %c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c",
				GET_FOURCC_1(stVPUFunc->check_code1),
				GET_FOURCC_2(stVPUFunc->check_code1),
				GET_FOURCC_3(stVPUFunc->check_code1),
				GET_FOURCC_4(stVPUFunc->check_code1),
				GET_FOURCC_1(stVPUFunc->check_code2),
				GET_FOURCC_2(stVPUFunc->check_code2),
				GET_FOURCC_3(stVPUFunc->check_code2),
				GET_FOURCC_4(stVPUFunc->check_code2),
				GET_FOURCC_1(stVPUFunc->check_code3),
				GET_FOURCC_2(stVPUFunc->check_code3),
				GET_FOURCC_3(stVPUFunc->check_code3),
				GET_FOURCC_4(stVPUFunc->check_code3),
				GET_FOURCC_1(stVPUFunc->check_code4),
				GET_FOURCC_2(stVPUFunc->check_code4),
				GET_FOURCC_3(stVPUFunc->check_code4),
				GET_FOURCC_4(stVPUFunc->check_code4)
			  );
	}
	return ret;
}

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
static int get_access_addr_file(void)
{
	int ret = 0;
	struct file *filp = NULL;
	mm_segment_t oldfs;
	void *tTmpPtr = NULL;

#	if LINUX_VERSION_CODE < KERNEL_VERSION(5, 0, 0)
	oldfs = get_fs();
	set_fs(get_ds());
#	elif LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	oldfs = get_fs();
	set_fs(KERNEL_DS);
#	else
	oldfs = force_uaccess_begin();
#	endif

	filp = filp_open("/proc/vpu", O_RDONLY, 420); // 0644 to 420
	VPU_CAST_PT(tTmpPtr, filp);
	if (IS_ERR(tTmpPtr)) {
		V_DBG(VPU_DBG_ERROR, "/proc/vpu file open fail!!");
		ret = -1;
	} else {
		char tmpdata[20];
		unsigned long long res = 0;
		u32 idx = 0;

		idx = (u32)((u32)sizeof(void *) * 2U) + 2U;

#	if LINUX_VERSION_CODE < KERNEL_VERSION(4, 14, 0)
		ret = vfs_read(filp, tmpdata, sizeof(tmpdata), &filp->f_pos);
#	else
		ret = filp->f_op->read(filp, tmpdata, sizeof(tmpdata), &filp->f_pos);
#	endif

		tmpdata[idx] = '\0';

		ret = kstrtoull(tmpdata, 16, &res);

		(void)memmove((void *)&stVPUFunc, (void *)&res, sizeof(unsigned long));

		(void)filp_close(filp, NULL);
	}

#	if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	set_fs(oldfs);
#	else
	force_uaccess_end(oldfs);
#	endif
	return ret;
}

#else

static int get_access_addr_mem(void)
{
	int ret = 0;
	void *va = NULL;

	va = vetc_ioremap(SHARE_POINT_ADDR + (SHARD_POINT_GAP * SHARE_POINT_ORDER_VPU), SHARD_POINT_GAP);

	if (va == NULL) {
		V_DBG(VPU_DBG_ERROR, "ioremap failed");
		ret = -ENOMEM;
	} else {
		memcpy(&stVPUFuncBase, va, sizeof(st_vpu_func));
		stVPUFunc = &stVPUFuncBase;

		V_DBG(VPU_DBG_INFO, "remap (PA : 0x%08x / VA : 0x%p) Dec ADDR : 0x%p",
				SHARE_POINT_ADDR, va,
				stVPUFunc->tccfp_vpu_dec);

		iounmap(va);
	}
	return ret;
}
#endif

static int get_access_addr(void)
{
	int ret = 0;

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 10, 0)
	ret = get_access_addr_file();
#else
	ret = get_access_addr_mem();
#endif

	if (ret == 0) {
		ret = check_access_addr_valid();
		if (ret == 0) {
			tcc_vpu_dec = (tccfp_vpu_dec_t)stVPUFunc->tccfp_vpu_dec;
			tcc_vpu_dec_esc = (tccfp_vpu_dec_esc_t)stVPUFunc->tccfp_vpu_dec_esc;
			tcc_vpu_dec_ext = (tccfp_vpu_dec_ext_t)stVPUFunc->tccfp_vpu_dec_ext;
			#if DEFINED_CONFIG_VENC_CNT_1to16
			tcc_vpu_enc = (tccfp_vpu_enc_t)stVPUFunc->tccfp_vpu_enc;
			{
				unsigned int chip_name = vetc_get_chip_name();
				if ((chip_name == 0x8035U) || (chip_name == 0x8036U)) {
					V_DBG(VPU_DBG_ERROR, "This IP (0x%04x) is not support VPU ENCODER IP.", chip_name);
					tcc_vpu_enc = NULL;
				}
			}
			#endif
		} else {
			ret = -1;
		}
	}

	return ret;
}

#else // defined(USE_ACCESS_POINT)

//Decoder
extern int tcc_vpu_dec(int Op, vcodec_handle_t *pHandle, void *pParam1,
		void *pParam2);
extern int tcc_vpu_dec_esc(int Op, vcodec_handle_t *pHandle,
					void *pParam1, void *pParam2);
extern int tcc_vpu_dec_ext(int Op, vcodec_handle_t *pHandle,
					void *pParam1, void *pParam2);

//Encoder
# 	if DEFINED_CONFIG_VENC_CNT_1to16
extern int tcc_vpu_enc(int Op, codec_handle_t *pHandle, void *pParam1,
		void *pParam2);
# 	endif
#endif // defined(USE_ACCESS_POINT)


static int tcc_vpu_dec_l(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	return tcc_vpu_dec(Op, pHandle, pParam1, pParam2);
}

//internal
static void vmgr_set_close_and_free_mem(vputype type);

/**
 * @brief Asserts and deasserts system-level resources for the Video Sub-system.
 *
 * This function is responsible for managing system-level resources such as clocks, interrupts,
 * and reset controls for the Video Sub-system. It can be used to assert and deassert these resources
 * based on whether the Video Sub-system is opened or closed.
 *
 * @param opened Flag indicating if the Video Sub-system is currently opened (1) or closed (0).
 *               When opened is 0, system resources will be reset and initialized.
 */
static void vmgr_sys_assert(int opened)
{
	// Enable clocks for the Video Sub-system
	vmgr_enable_clock(0, 0);

	if (opened == 0) {
		// Reset the Video Sub-system, enable its interrupt
		vmgr_hw_reset();
	}
}

/**
 * @brief Deasserts system-level resources for the Video Sub-system.
 *
 * This function is responsible for deasserting system-level resources such as interrupts,
 * reset controls, and clocks for the Video Sub-system. It can be used to release these resources
 * when the Video Sub-system is being closed or no longer in use.
 *
 * @param opened Flag indicating if the Video Sub-system is currently opened (1) or closed (0).
 *               When opened is 0 or negative, system resources will be deasserted and disabled.
 */
static void vmgr_sys_deassert(int opened)
{
	if (opened <= 0) {
		// Disable the Video Sub-system's interrupt, reset it, and then reassert resources
		vmgr_hw_assert();
	}

	// Disable clocks for the Video Sub-system
	vmgr_disable_clock(0, 0);
}

/**
 * @brief Perform cleanup operations upon receiving a codec_exit signal from the VPU.
 *
 * This function is responsible for handling cleanup operations when a codec_exit signal is received from the VPU.
 * It sets the appropriate flags and frees memory associated with the specified VPU type.
 *
 * @param type The VPU type for which the codec_exit signal is received.
 */
static void vmgr_codec_exit(vputype type)
{
	// Perform cleanup and memory deallocation for the specified VPU type
	vmgr_set_close_and_free_mem(type);

	// If a codec_exit signal was received due to a timeout, update the timeout-related information
	if (vmgr_data.timeout_exit == 1) {
		V_DBG(VPU_DBG_SEQUENCE,
			  "codec_exit prev [%s] => curr [%s]",
			  vpu_vputype_to_string((vputype)(vmgr_data.timeout_vpu_type)), vpu_vputype_to_string(type));
	}
	vmgr_data.timeout_vpu_type = (int)type;
	vmgr_data.timeout_exit = 1;
}

/**
 * @brief Find the first element in the list with a matching VPU type.
 *
 * This function iterates through each element in the list and checks if the VPU type matches the provided type.
 * It returns the first element with a matching VPU type, or NULL if no matching element is found.
 *
 * @param list The pointer to the list head.
 * @param type The VPU type to match.
 * @param cmd_type The VPU cmd_type to match.
 * @return Pointer to the first element with a matching VPU type, or NULL if no matching element is found or if the list is invalid.
 */
#if 0
static struct VpuList *vmgr_find_element_with_vpu_type(struct list_head *list, int type, int cmd_type)
{
	struct VpuList *entry;

	list_for_each_entry(entry, list, list) {
		if (entry->type == type) {
			if (cmd_type == -1) {
				return entry;
			} else if (cmd_type == VPU_DEC_DECODE) {
				if ((entry->cmd_type == VPU_DEC_DECODE) || (entry->cmd_type == V2D_IP_DEC_FRMDATA)) {
					return entry;
				}
			} else {
				if (entry->cmd_type == cmd_type) {
					return entry;
				}
			}
		}
	}

	return NULL;
}
#endif

static struct VpuList *vmgr_find_dec_element_with_vpu_type(struct list_head *list, int type)
{
	struct VpuList *entry;

	list_for_each_entry(entry, list, list) {
		if (entry->type == type) {
			if ((entry->cmd_type == VPU_DEC_DECODE) ||
			    (entry->cmd_type == V2D_IP_DEC_FRMDATA)) {
				return entry;
			} else {
				return NULL;
			}
		}
	}

	return NULL;
}

static struct VpuList *vmgr_v2_find_urgent_element_with_vpu_type(struct list_head *list, int type)
{
	struct VpuList *entry;

	list_for_each_entry(entry, list, list) {
		if (entry->type == type) {
			if (entry->cmd_type == V2D_IP_FRM_FLUSH) {
				(void)pr_info("%s: V2D_IP_FRM_FLUSH", __func__);
				return entry;
			} else if (entry->cmd_type == V2D_IP_DRV_RST) {
				(void)pr_info("%s: V2D_IP_DRV_RST", __func__);
				return entry;
			} else {
				return NULL;
			}
		}
	}

	return NULL;
}

#if 0 //not used
/**
 * @brief Remove entries with a specific type from the list.
 *
 * This function removes entries with a specific type from the specified list.
 *
 * @param list Pointer to the list head.
 * @param type The type of entries to be removed.
 * @param cmd_type The cmd_type of entries to be removed.
 */
static void vmgr_remove_entries_with_type(struct list_head *list, int type, int cmd_type)
{
	struct list_head *pos, *tmp;
	struct VpuList *entry;

	if (list != NULL) {
		list_for_each_safe(pos, tmp, list) {
			entry = list_entry(pos, struct VpuList, list);
			if ((entry->type == type) && (entry->cmd_type == cmd_type)) {
				list_del(pos);
			}
		}
	}
}
#endif

#if 0
/**
 * @brief Remove all entries with a specific type from the list.
 *
 * This function removes all entries with a specific type from the specified list.
 *
 * @param list Pointer to the list head.
 * @param type The type of entries to be removed.
 *
 * @note This function removes all entries that match the specified type.
 *       It does not consider the cmd_type of entries.
 *       For removing entries based on both type and cmd_type, use vmgr_remove_entries_with_type().
 */
static void vmgr_remove_all_entries_with_type(struct list_head *list, int type)
{
	struct list_head *pos, *tmp;
	struct VpuList *entry;

	if (list != NULL) {
		/**
		 * @note Using list_for_each_safe to safely traverse and manipulate the list.
		 */
		list_for_each_safe(pos, tmp, list) {
			entry = list_entry(pos, struct VpuList, list);
			if (entry->type == type) {
				/**
				 * @note Removing the entry from the list.
				 */
				list_del(pos);
			}
		}
	}
}


/**
 * @brief Get the first element in the list with a matching VPU type.
 *
 * This function retrieves the first element in the list with a matching VPU type.
 *
 * @param list The pointer to the list head.
 * @param type The VPU type to match.
 * @return Pointer to the first element with a matching VPU type, or NULL if no matching element is found or if the list is invalid.
 */
static struct VpuList *vmgr_get_first_element_with_vpu_type(struct list_head *list, int type)
{
	if (list == NULL) {
		return NULL;
	}

	return vmgr_find_element_with_vpu_type(list, type, -1);
}

static struct VpuList *vmgr_get_next_element_with_vpu_type(struct list_head *list, int type, int cmd_type)
{
	if (list == NULL) {
		return NULL;
	}

	return vmgr_find_element_with_vpu_type(list, type, cmd_type);
}
#endif

/**
 * @brief Remove all entries from the list.
 *
 * This function removes all entries from the specified list by iterating over the list and deleting each entry.
 *
 * @param list Pointer to the list head.
 */
static void vmgr_remove_all_entries(struct list_head *list)
{
	struct list_head *pos, *tmp;

	list_for_each_safe(pos, tmp, list) {
		list_del(pos);
	}
}

/**
 * @brief Count the number of allocated nodes in the list with a matching VPU type.
 *
 * This function counts the number of allocated nodes in the specified list and returns the count.
 *
 * @param list Pointer to the list head.
 * @param type The VPU type to match.
 * @return The number of allocated nodes in the list.
 */
static int vmgr_count_allocated_nodes_with_vpu_type(struct list_head *list, int type)
{
	int count = 0;
	struct VpuList *entry;

	list_for_each_entry(entry, list, list) {
		if (entry->type == type) {
			if (count < (int)INT_MAX) {
				count++;
			}
		}
	}

	return count;
}

/**
 * @brief Count the number of allocated nodes in the list with a matching VPU type.
 *
 * This function counts the number of allocated nodes in the specified list and returns the count.
 *
 * @param list Pointer to the list head.
 * @param type The VPU type to match.
 * @return The number of allocated nodes in the list.
 */
#if 0
static int vmgr_cmd_decode_count_allocated_nodes_with_vpu_type(struct list_head *list, int type)
{
	int count = 0;
	struct VpuList *entry;

	list_for_each_entry(entry, list, list) {
		if (entry->type == type) {
			if ((entry->cmd_type == VPU_DEC_DECODE) || (entry->cmd_type == V2D_IP_DEC_FRMDATA)) {
				count++;
			}
		}
	}

	return count;
}
#endif
/**
 * @brief Count the number of allocated nodes in the list.
 *
 * This function counts the number of allocated nodes in the specified list and returns the count.
 *
 * @param list Pointer to the list head.
 * @return The number of allocated nodes in the list.
 */
static int vmgr_count_allocated_nodes(struct list_head *list)
{
	int count = 0;
	struct VpuList *entry;

	list_for_each_entry(entry, list, list) {
		if (count < (int)INT_MAX) {
			count++;
		}
	}

	return count;
}

/**
 * @brief Function to check if there is a duplicate element in the list.
 *
 * @param list The list to check for duplicates.
 * @param element The element to compare for duplicates.
 * @return true if a duplicate element is found, false otherwise.
 */
static bool vmgr_is_duplicate_element(struct list_head *list, struct VpuList *element)
{
	struct VpuList *entry;

	// Iterate through each element in the list
	list_for_each_entry(entry, list, list) {
		// Check if the current element is the same as the given element
		if (entry == element) {
			return true; // The duplicate element already exists in the list
		}
	}

	return false; // No duplicate element found
}

/**
 * @brief Function to add an element to the list.
 *
 * @param element The element to add to the list.
 * @param list The list to add the element to.
 * @return true if the element is added successfully, false if a duplicate element is found.
 */
static bool vmgr_add_element_to_list(struct VpuList *element, struct list_head *list)
{
	if (vmgr_is_duplicate_element(list, element)) {
		V_DBG(VPU_DBG_SEQUENCE, "Duplicate element detected, cannot add to the list");
		return false; // Return false to handle the error if a duplicate element is found
	}

	list_add_tail(&element->list, list);
	return true; // Element added successfully
}

/**
 * @brief Function to remove an element from the list.
 *
 * @param element The element to remove from the list.
 * @param list The list to remove the element from.
 * @return true if the element is removed successfully, false if the element does not exist in the list.
 */
static bool vmgr_remove_element_from_list(struct VpuList *element, struct list_head *list)
{
	if (!vmgr_is_duplicate_element(list, element)) {
		V_DBG(VPU_DBG_SEQUENCE, "Element does not exist in the list");
		return false; // Return false to indicate that the element does not exist in the list (for error handling)
	}

	list_del(&element->list);
	return true; // Return true to indicate successful removal of the element
}

/**
 * @brief Check if the list is empty.
 *
 * This function checks whether the provided list is empty.
 *
 * @param list Pointer to the list head.
 * @return Returns a pointer to the special 'vmgr_vlist' element if the list is empty, otherwise returns NULL.
 */
static struct VpuList *vmgr_is_list_empty(struct list_head *list)
{
	if (list_empty(list) != 0) {
		return &vmgr_vlist;
	}
	return NULL;
}

/**
 * @brief Get the first entry in the provided list.
 *
 * This function returns a pointer to the first entry in the provided list.
 *
 * @param list Pointer to the list head.
 * @return Pointer to the first entry in the provided list.
 */
static struct VpuList *vmgr_get_first_list_entry(struct list_head *list)
{
	return (struct VpuList *)list_first_entry(list, struct VpuList, list);
}

/**
 * @brief Get the number of alive 'vmgr'
 *
 * This function returns the number of alive 'vmgr'
 *
 * @return The number of alive 'vmgr'.
 */
int vmgr_get_alive(void)
{
	return atomic_read(&vmgr_data.opened);
}

/**
 * @brief Get the number of active VPU instances.
 *
 * This function counts the number of active VPU instances by iterating through the closed array.
 *
 * @return The number of active VPU instances.
 */
static int count_active_instances_in_vpu(void)
{
	int active = 0;
	int vpu_type = 0;

	for (vpu_type = 0; vpu_type < VPU_ENC; vpu_type++) {
		if ((vmgr_data.closed[vpu_type] != VPU_CLOSED) && (active < __INT32_MAX__)) {
			active++;
		}
	}

	return active;
}


/**
 * @brief Get the interrupt timeout value in milliseconds.
 *
 * This function calculates the interrupt timeout value in milliseconds based on the default timeout value
 * and the number of alive devices in the 'vmgr'. If there are alive devices, the timeout value
 * is multiplied by the number of alive devices.
 *
 * @param def_timeout_msec The default interrupt timeout value in milliseconds.
 * @return The calculated interrupt timeout value in milliseconds.
 */
#define DEFAULT_TIMEOUT_MSEC 200
static int gs_timeout_msec = DEFAULT_TIMEOUT_MSEC;
static int vmgr_get_interrupt_timeout_msec(int def_timeout_msec)
{
	int timeout_msec = def_timeout_msec;

	if (module_param_vdbg_drv > 0U) {//for debugging
		unsigned int mul = (module_param_vdbg_drv >> 16) & 0xFFFF;
		if ((mul > 0) && (mul < 100)) {
			if (mul != 0 && timeout_msec > UINT_MAX / mul) {
				(void)pr_err("[%s:%d] overflow has occurred in timeout_msec. timeout_msec initialized to (int) 0.", __func__, __LINE__);
				timeout_msec = (int)0;
			} else {
				timeout_msec = timeout_msec * mul;
			}
		}

		if (gs_timeout_msec != timeout_msec) {
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				"default timeout(%d) to (mul:%d => %d)",
				gs_timeout_msec, mul, timeout_msec);

			gs_timeout_msec = timeout_msec;
			module_param_vdbg_drv &= 0x0000FFFF;
		}
	}
	return timeout_msec;
}

/**
 * @brief Set the close flag and free memory for specific vpu_type
 *
 * @param type The VPU type to set the close flag and free memory for.
 */
static void vmgr_set_close_and_free_mem(vputype type)
{
	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
		"[%s] set close and free memory!",
		vpu_vputype_to_string(type));

	vmgr_data.closed[type] = VPU_CLOSED;
	vmgr_data.handle[type] = 0x00;
	if (vmgr_data.vpu_ctrl.interlaced[type] == 1) {
		vmgr_data.vpu_ctrl.interlaced[type] = 0;
		vmgr_data.vpu_ctrl.interlaced_video_total--;
	}

	vmgr_data.vpu_ctrl.pending_count[type] = 0;

	(void)vmem_proc_free_memory(type);
}

#if DEFINED_CONFIG_VENC_CNT_1to16
static int tcc_vpu_enc_l(int Op, vcodec_handle_t *pHandle, void *pParam1, void *pParam2) VPU_NO_SANITIZE_CFI
{
	int ret = RETCODE_FAILURE;
	if (tcc_vpu_enc == NULL) {
		ret = RETCODE_INVALID_HANDLE;
	} else {
		ret = tcc_vpu_enc(Op, pHandle, pParam1, pParam2);
	}
	return ret;
}
#endif

int vmgr_opened(void)
{
	return (vmgr_get_alive() == 0) ? 0 : 1;
}

int vmgr_get_close(vputype type)
{
	return vmgr_data.closed[type];
}

int vmgr_set_close(vputype type, int value, int bfreemem)
{
	int ret = 0;
	if (vmgr_data.closed[type] == value) {
		if (value == VPU_CLOSED) {
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				"[%s] was already closed. active(vpu:%d|vmgr:%d)",
				vpu_vputype_to_string(type),
				count_active_instances_in_vpu(), vmgr_get_alive());
		} else {
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				" [%s] was already set into %d. , active(vpu:%d|vmgr:%d)",
				vpu_vputype_to_string(type), value,
				count_active_instances_in_vpu(), vmgr_get_alive());
		}
		ret = -1;
	}

	if (ret == 0) {
		vmgr_data.closed[type] = value;
		if (value == VPU_CLOSED) {
			vmgr_data.handle[type] = 0x00;

			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				" [%s] closed, active(vpu:%d|vmgr:%d)",
				vpu_vputype_to_string(type),
				count_active_instances_in_vpu(), vmgr_get_alive());

			if (bfreemem == 1) {
				V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
					" [%s] free memory",
					vpu_vputype_to_string(type));
				(void)vmem_proc_free_memory(type);
			}
		}
	}

	return ret;
}

static void vmgr_close_all(int bfreemem)
{
	(void)vmgr_set_close(VPU_DEC, 1, bfreemem);
	(void)vmgr_set_close(VPU_DEC_EXT, 1, bfreemem);
	(void)vmgr_set_close(VPU_DEC_EXT2, 1, bfreemem);
	(void)vmgr_set_close(VPU_DEC_EXT3, 1, bfreemem);
	(void)vmgr_set_close(VPU_DEC_EXT4, 1, bfreemem);

#if DEFINED_CONFIG_VENC_CNT_1to16
	(void)vmgr_set_close(VPU_ENC, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_2to16
	(void)vmgr_set_close(VPU_ENC_EXT, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_3to16
	(void)vmgr_set_close(VPU_ENC_EXT2, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_4to16
	(void)vmgr_set_close(VPU_ENC_EXT3, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_5to16
	(void)vmgr_set_close(VPU_ENC_EXT4, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_6to16
	(void)vmgr_set_close(VPU_ENC_EXT5, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_7to16
	(void)vmgr_set_close(VPU_ENC_EXT6, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_8to16
	(void)vmgr_set_close(VPU_ENC_EXT7, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_9to16
	(void)vmgr_set_close(VPU_ENC_EXT8, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_10to16
	(void)vmgr_set_close(VPU_ENC_EXT9, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_11to16
	(void)vmgr_set_close(VPU_ENC_EXT10, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_12to16
	(void)vmgr_set_close(VPU_ENC_EXT11, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_13to16
	(void)vmgr_set_close(VPU_ENC_EXT12, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_14to16
	(void)vmgr_set_close(VPU_ENC_EXT13, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_15to16
	(void)vmgr_set_close(VPU_ENC_EXT14, 1, bfreemem);
#endif
#if DEFINED_CONFIG_VENC_CNT_16
	(void)vmgr_set_close(VPU_ENC_EXT15, 1, bfreemem);
#endif
}

int vmgr_process_ex(struct VpuList *cmd_list, vputype type, int Op, int *result)
{
	int res = 0;

	if (type >= VPU_MAX) {
		res = -1;
	}

	if (vmgr_get_alive() == 0) {
		res = -1;
	}

	if (res == 0) {

		if (vmgr_data.closed[type] != VPU_CLOSED) {
			cmd_list->type = (unsigned int)type;
			cmd_list->cmd_type = Op;
			cmd_list->handle = vmgr_data.handle[type];
			cmd_list->args = NULL;
			cmd_list->comm_data = NULL;
			cmd_list->vpu_result = result;

			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
					"[%s][%x] force add cmd:LIST_ADD, Op = 0x%x, active(vpu:%d|vmgr:%d)",
					vpu_vputype_to_string(type), cmd_list->handle, Op,
					count_active_instances_in_vpu(), vmgr_get_alive());

			(void)vmgr_list_manager(cmd_list, (unsigned int)LIST_ADD);
		}
	}

	return res;
}

static int vmgr_internal_handler(void)
{
	int ret, ret_code = RETCODE_INTR_DETECTION_NOT_ENABLED;
	unsigned int timeout = gs_timeout_msec;
	unsigned long jtimeout;

	if (vmgr_data.check_interrupt_detection > 0) {
		if (atomic_read(&vmgr_data.oper_intr) > 0) {
			V_DBG(VPU_DBG_INTERRUPT, "Success 1: vpu operation!!");
			ret_code = RETCODE_SUCCESS;
		} else {
			timeout = vmgr_get_interrupt_timeout_msec(timeout);
			jtimeout = msecs_to_jiffies(timeout);
			if (jtimeout >= ULONG_MAX) {
				jtimeout = ULONG_MAX;
			}

			ret = wait_event_interruptible_timeout(
						vmgr_data.oper_wq,
						atomic_read
						(&vmgr_data.oper_intr) > 0,
						(long)jtimeout);

			if (atomic_read(&vmgr_data.oper_intr) > 0) {
				V_DBG(VPU_DBG_INTERRUPT,
					"Success 2: vpu operation!!");
#if defined(FORCED_ERROR)
				if (forced_error_count-- <= 0) {
					ret_code = RETCODE_CODEC_EXIT;
					forced_error_count = FORCED_ERR_CNT;
					vetc_dump_reg_all(
						(char *)vmgr_data.base_addr,
					"force-timed_out in vmgr internal handler");
				} else {
				}
#endif
				ret_code = RETCODE_SUCCESS;
			} else {
				static unsigned char fname[] = "timed_out in vmgr_internal_handler";

				err_vpu(
					"[CMD 0x%x][%d]: vpu timed_out(ref %d msec) => oper_intr[%d]!! [%d]th frame len %d",
					vmgr_data.current_cmd, ret, timeout,
					atomic_read(&vmgr_data.oper_intr),
					vmgr_data.nDecode_Cmd,
					vmgr_data.szFrame_Len);

				vetc_dump_reg_all((char *)vmgr_data.base_addr, fname);
				ret_code = RETCODE_CODEC_EXIT;
			}
		}
		atomic_set(&vmgr_data.oper_intr, 0);
		vmgr_status_clear((unsigned int *)vmgr_data.base_addr);
	}

	V_DBG(VPU_DBG_INTERRUPT, "out (Interrupt detection=%d, ret_code=%d)",
		vmgr_data.check_interrupt_detection,
		ret_code);

	return ret_code;
}

static int vmgr_process(vputype type, int cmd, long pHandle, void *args)
{
	int ret = 0;
#ifdef CONFIG_VPU_TIME_MEASUREMENT
	long long startTime, endTime;
	long long time_gap_us = 0LL;
	startTime = vetc_GetKtime();
#endif

	vmgr_data.check_interrupt_detection = 0;
	vmgr_data.current_cmd = cmd;

	if (type < VPU_ENC) {
		if ((cmd != VPU_DEC_INIT) &&
			(cmd != VPU_DEC_INIT_KERNEL) &&
			(cmd != V2D_IP_DRV_INI)) {
			if (vmgr_get_close(type)
					|| (vmgr_data.handle[type] == 0x00)) {
				return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
			}
		}

		if ((cmd != VPU_DEC_BUF_FLAG_CLEAR) &&
			(cmd != VPU_DEC_BUF_FLAG_CLEAR_KERNEL) &&
			(cmd != VPU_DEC_DECODE) &&
			(cmd != VPU_DEC_DECODE_KERNEL)) {
			V_DBG(VPU_DBG_CMD,
				"[%s], command: 0x%x", vpu_vputype_to_string(type), cmd);
		}

		switch (cmd) {
		case VPU_DEC_INIT:
		case VPU_DEC_INIT_KERNEL:
		case V2D_IP_DRV_INI:
		{
			VDEC_INIT_t *arg;
			union_codec_handle_t codec_handle;
			unsigned int vpu_lib_dbg = get_vpu_lib_dbg_param();
			bool isFlexible = (cmd == V2D_IP_DRV_INI);

			if (isFlexible) {
				arg = (VDEC_INIT_t *) v2fhdmgr_unmarshal_ip_inidata(args);
				cmd = VPU_DEC_INIT;
				vmgr_data.vpu_ctrl.isFlexible[type] = true;
			} else {
				arg = (VDEC_INIT_t *) args;
				vmgr_data.vpu_ctrl.isFlexible[type] = false;
			}

			if (arg == NULL) {
				V_DBG(VPU_DBG_SEQUENCE,
					"[%s] CMD(0x%x), active(vpu:%d|vmgr:%d)",
					vpu_vputype_to_string(type), cmd,
					count_active_instances_in_vpu(), vmgr_get_alive());
				return RETCODE_FAILURE;
			}

			vmgr_data.handle[type] = 0x00;

			arg->gsVpuDecInit.m_RegBaseVirtualAddr =
				(codec_addr_t) vmgr_data.base_addr;
			arg->gsVpuDecInit.m_Memcpy =
				(void*(*)(void *dest, const void *src,
					unsigned int count,
					unsigned int type)) vetc_memcpy;
			arg->gsVpuDecInit.m_Memset =
				(void (*)(void *ptr, int value, unsigned int num,
					unsigned int type)) vetc_memset;
			arg->gsVpuDecInit.m_Interrupt =
				(int (*)(void))vmgr_internal_handler;
			arg->gsVpuDecInit.m_Ioremap =
				(void *(*)(phys_addr_t phy_addr,
				unsigned int size)) vetc_ioremap;
			arg->gsVpuDecInit.m_Iounmap =
				(void (*)(void *virt_addr))vetc_iounmap;
			arg->gsVpuDecInit.m_reg_read =
				(unsigned int (*)(void *base_addr,
				unsigned int offset)) vetc_reg_read;
			arg->gsVpuDecInit.m_reg_write =
				(void (*)(void *base_addr, unsigned int offset,
				unsigned int data)) vetc_reg_write;
			arg->gsVpuDecInit.m_Usleep =
				(void (*)(unsigned int uimin,
				unsigned int uimax)) vetc_usleep;

			vmgr_data.bDiminishInputCopy =
			(arg->gsVpuDecInit.m_uiDecOptFlags &
				(unsigned int)(0x4000000)) ? (bool)true : (bool)false; //(1U << 26U)

			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				"[%s] Init In => workbuff %#x/%#x, Reg: %#x, format : %d, "
				"Stream(%#x/%#x, %d), Dec-%d: "
				"Init In => optFlag %#x, avcBuff: %#x- %d, Userdata(%d), "
				"Inter: %d, PlayEn: %d, MaxRes: %d, disminished Copy(%d)",
				vpu_vputype_to_string(type),
				arg->gsVpuDecInit.m_BitWorkAddr[PA],
				arg->gsVpuDecInit.m_BitWorkAddr[VA],
				arg->gsVpuDecInit.m_RegBaseVirtualAddr,
				arg->gsVpuDecInit.m_iBitstreamFormat,
				arg->gsVpuDecInit.m_BitstreamBufAddr[PA],
				arg->gsVpuDecInit.m_BitstreamBufAddr[VA],
				arg->gsVpuDecInit.m_iBitstreamBufSize,
				type,
				arg->gsVpuDecInit.m_uiDecOptFlags,
				arg->gsVpuDecInit.m_pSpsPpsSaveBuffer,
				arg->gsVpuDecInit.m_iSpsPpsSaveBufferSize,
				arg->gsVpuDecInit.m_bEnableUserData,
				arg->gsVpuDecInit.m_bCbCrInterleaveMode,
				arg->gsVpuDecInit.m_iFilePlayEnable,
				arg->gsVpuDecInit.m_iMaxResolution,
				vmgr_data.bDiminishInputCopy);

#if defined(USE_ACCESS_POINT)
			if (check_access_addr_valid() != 0) {
				err_vpu(
					"[%s] Access address envalid!!(%d)",
					vpu_vputype_to_string(type), check_access_addr_valid());

				return RETCODE_FAILURE;
			}
#endif

			if (vmem_alloc_count((int)type) <= 0) {
				err_vpu("[%s] No Buffer allocation", vpu_vputype_to_string(type));
				return RETCODE_FAILURE;
			}

			codec_handle.pcodec_handle = &arg->gsVpuDecHandle;

			if ((vpu_lib_dbg & VPU_DBG_LIB_USE_CB_PRINTK) == VPU_DBG_LIB_USE_CB_PRINTK) {
				unsigned int codec_ip;
				vpu_dec_ctrl_log_status_t dec_log;

				// Extract codec_ip (A part), please refer to enum vpu_ip_type
				// VPU_IP_C7 = 1, VPU_IP_4KD2 = 2, VPU_IP_HEVC_ENC = 3, VPU_IP_HEVC_ENC2 = 4, VPU_IP_JPU_C6 = 5, VPU_IP_HEVC_DEC = 6
				codec_ip = (vpu_lib_dbg & 0x00F000U) >> 12;
				V_DBG(VPU_DBG_ERROR, "[VPU] codec_ip: %d", codec_ip);

				// Check if codec_ip matches desired value
				if (codec_ip == 1) {
					// Check if codec_ip matches desired value
					// Extract log_mask (BBB part)
					unsigned int log_mask = (vpu_lib_dbg & 0x000FFFU);

					V_DBG(VPU_DBG_ERROR, "[VPU] log_mask: %d (%x)", log_mask, log_mask);
					dec_log.pfLogPrintCb = (void (*)(const char *, ...))vpu_printk;
					dec_log.stLogLevel.bVerbose = (log_mask & 1U) ? 1 : 0;
					dec_log.stLogLevel.bDebug   = (log_mask & 2U) ? 1 : 0;
					dec_log.stLogLevel.bInfo	  = (log_mask & 4U) ? 1 : 0;
					dec_log.stLogLevel.bWarn	  = (log_mask & 8U) ? 1 : 0;
					dec_log.stLogLevel.bError   = (log_mask & 16U) ? 1 : 0; // 0x10
					dec_log.stLogLevel.bAssert  = (log_mask & 32U) ? 1 : 0; // 0x20
					dec_log.stLogLevel.bFunc	  = (log_mask & 64U) ? 1 : 0; // 0x40
					dec_log.stLogLevel.bTrace   = (log_mask & 128U) ? 1 : 0; // 0x80
					ret = tcc_vpu_dec_l(VPU_CTRL_LOG_STATUS, NULL, (void *)(&dec_log), (void *)NULL);
				}
			}

			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)(codec_handle.pvcodec_handle),
				(void *)&arg->gsVpuDecInit,
				(void *)NULL);

			if (ret != RETCODE_SUCCESS) {
				err_vpu("[%s][VPU_DEC_INIT] error => ret(0x%x, %d)", vpu_vputype_to_string(type), ret, ret);
				if (ret != RETCODE_CODEC_EXIT) {
					static unsigned char fname[] = "Init error";

					vetc_dump_reg_all((char *)vmgr_data.base_addr, fname);
				}
			} else {
				if (isFlexible) {
					v2fhdmgr_marshal_op_inidata(args);
				}
			}

			if ((ret != RETCODE_CODEC_EXIT) && (arg->gsVpuDecHandle != 0)) {
				vmgr_data.handle[type] = arg->gsVpuDecHandle;
				vmgr_data.vpu_ctrl.interlaced[type] = 0;
				vmgr_data.vpu_ctrl.pending_count[type] = 0;

				(void)vmgr_set_close(type, 0, 0);

				V_DBG(VPU_DBG_CMD,
					"Dec-%d :: handle = 0x%x",
					type, arg->gsVpuDecHandle);

				V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
					"[%s] Init Done Handle(%#x), ret(%d)",
					vpu_vputype_to_string(type), arg->gsVpuDecHandle, ret);
			} else {
				V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
					"[%s][VPU_DEC_INIT] RETCODE_CODEC_EXIT returned ...(%d), handle(%x)",
					vpu_vputype_to_string(type), vmgr_get_alive(), arg->gsVpuDecHandle);

				//vmgr_codec_exit(type);
				vmgr_set_close_and_free_mem(type);
			}

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			vmgr_data.iTime[type].print_out_index = 0;
			vmgr_data.iTime[type].proc_base_cnt = 0;
			vmgr_data.iTime[type].accumulated_proc_time = 0;
			vmgr_data.iTime[type].accumulated_frame_cnt = 0;
			vmgr_data.iTime[type].proc_time_30frames = 0;
#endif
		}
		break;

		case VPU_DEC_SEQ_HEADER:
		case VPU_DEC_SEQ_HEADER_KERNEL:
		{
			void *arg = args;
			unsigned int iSize, iBitstreamDataSize;
			union {
				unsigned int i_data;
				int *pi_data;	//NULL
				void *pv_data;
			} udata;

			dec_initial_info_t *gsVpuDecInitialInfo;

			udata.pi_data = NULL;

			gsVpuDecInitialInfo = vmgr_data.bDiminishInputCopy ?
			&((VDEC_DECODE_t *)arg)->gsVpuDecInitialInfo :
			&((VDEC_SEQ_HEADER_t *)
				arg)->gsVpuDecInitialInfo;

			if (((VDEC_DECODE_t *)arg)->gsVpuDecInput
				.m_iBitstreamDataSize > 0) {
				iBitstreamDataSize =
					(unsigned int)((VDEC_DECODE_t *)arg)->
					   gsVpuDecInput.m_iBitstreamDataSize;
			} else {
				iBitstreamDataSize = 0;
			}

			if (vmgr_data.bDiminishInputCopy) {
				iSize = iBitstreamDataSize;
			} else {
				iSize = ((VDEC_SEQ_HEADER_t *)arg)->stream_size;
			}
			vmgr_data.szFrame_Len = iSize;
			udata.i_data = iSize;
			vmgr_data.check_interrupt_detection = 1;
			vmgr_data.nDecode_Cmd = 0;

			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				"[%s] VPU_DEC_SEQ_HEADER in :: size(%d)",
				vpu_vputype_to_string(type), iSize);

			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(vmgr_data.bDiminishInputCopy ?
					(void *)(&((VDEC_DECODE_t *)
						arg)->gsVpuDecInput) :
					(void *)udata.pv_data),	//(long)iSize
				(void *)gsVpuDecInitialInfo);

			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				"[%s] VPU_DEC_SEQ_HEADER(%#x,%d) res info "
				"%4dx%-4d (L/R/T/B:%d/%d/%d/%d)",
				vpu_vputype_to_string(type), ret, ret,
				gsVpuDecInitialInfo->m_iPicWidth,
				gsVpuDecInitialInfo->m_iPicHeight,
				gsVpuDecInitialInfo->m_iAvcPicCrop.m_iCropLeft,
				gsVpuDecInitialInfo->m_iAvcPicCrop.m_iCropRight,
				gsVpuDecInitialInfo->m_iAvcPicCrop.m_iCropTop,
				gsVpuDecInitialInfo->m_iAvcPicCrop.m_iCropBottom);
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				"[%s] min_buff_count(%d), min_buff_size(%d), "
				"buf_delay(%d), profile(%d), level(%d), interlace(%d) "
				"AspectRatio(%d), ErrorReason(%d)",
				vpu_vputype_to_string(type),
				gsVpuDecInitialInfo->m_iMinFrameBufferCount,
				gsVpuDecInitialInfo->m_iMinFrameBufferSize,
				gsVpuDecInitialInfo->m_iFrameBufDelay,
				gsVpuDecInitialInfo->m_iProfile,
				gsVpuDecInitialInfo->m_iLevel,
				gsVpuDecInitialInfo->m_iInterlace,
				gsVpuDecInitialInfo->m_iAspectRateInfo,
				gsVpuDecInitialInfo->m_iReportErrorReason);
		}
		break;

		case V2D_IP_DEC_SEQDATA:
		{
			VDEC_SEQ_HEADER_t *arg =
				(VDEC_SEQ_HEADER_t *)v2fhdmgr_unmarshal_ip_seqdata(args);

			if (arg != NULL) {
				dec_initial_info_t *initial_info = &arg->gsVpuDecInitialInfo;
				unsigned long iSize = arg->stream_size;

				vmgr_data.szFrame_Len = arg->stream_size;
				vmgr_data.check_interrupt_detection = 1;
				vmgr_data.nDecode_Cmd = 0;

				if (!vdbg_mode()) {
					(void)pr_info("[%s][In] V2D_IP_DEC_SEQDATA: size %lu", __func__, iSize);
				}

				ret = tcc_vpu_dec_l(VPU_DEC_SEQ_HEADER,
						(vcodec_handle_t *)&pHandle,
						(void *) iSize,
						(void *)initial_info);

				if (vdbg_mode()) {
					V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
						"[%s] VPU_DEC_SEQ_HEADER size(%d),(%#x,%d) res info "
						"%4dx%-4d (L/R/T/B:%d/%d/%d/%d)",
						vpu_vputype_to_string(type), iSize, ret, ret,
						initial_info->m_iPicWidth,
						initial_info->m_iPicHeight,
						initial_info->m_iAvcPicCrop.m_iCropLeft,
						initial_info->m_iAvcPicCrop.m_iCropRight,
						initial_info->m_iAvcPicCrop.m_iCropTop,
						initial_info->m_iAvcPicCrop.m_iCropBottom);
					V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
						"[%s] min_buff_count(%d), min_buff_size(%d), "
						"buf_delay(%d), profile(%d), level(%d), interlace(%d) "
						"AspectRatio(%d), ErrorReason(%d)",
						vpu_vputype_to_string(type),
						initial_info->m_iMinFrameBufferCount,
						initial_info->m_iMinFrameBufferSize,
						initial_info->m_iFrameBufDelay,
						initial_info->m_iProfile,
						initial_info->m_iLevel,
						initial_info->m_iInterlace,
						initial_info->m_iAspectRateInfo,
						initial_info->m_iReportErrorReason);
				} else {
					(void)pr_info("[%s] VPU_DEC_SEQ_HEADER(%#x,%d) res info "
						"%4dx%-4d (L/R/T/B:%d/%d/%d/%d)",
						vpu_vputype_to_string(type), ret, ret,
						initial_info->m_iPicWidth,
						initial_info->m_iPicHeight,
						initial_info->m_iAvcPicCrop.m_iCropLeft,
						initial_info->m_iAvcPicCrop.m_iCropRight,
						initial_info->m_iAvcPicCrop.m_iCropTop,
						initial_info->m_iAvcPicCrop.m_iCropBottom);
				}

				v2fhdmgr_marshal_op_seqdata(args);
				if (ret == RETCODE_SUCCESS) {

					ret = v2fhdmgr_register_hwbuf(args, pHandle, tcc_vpu_dec_l);
				}
			}
		}
		break;

		case VPU_DEC_REG_FRAME_BUFFER:
		case VPU_DEC_REG_FRAME_BUFFER_KERNEL:
		{
			VDEC_SET_BUFFER_t *arg =
				(VDEC_SET_BUFFER_t *) args;
			V_DBG(VPU_DBG_INFO,
				"Dec-%d: VPU_DEC_REG_FRAME_BUFFER in :: 0x%x/0x%x ",
				type,
				arg->gsVpuDecBuffer.m_FrameBufferStartAddr[0],
				arg->gsVpuDecBuffer.m_FrameBufferStartAddr[1]);

			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsVpuDecBuffer),
				(void *)NULL);

			V_DBG(VPU_DBG_SEQUENCE,
				"Dec-%d: VPU_DEC_REG_FRAME_BUFFER out",
				type);
		}
		break;

		case VPU_DEC_REG_FRAME_BUFFER3:
		case VPU_DEC_REG_FRAME_BUFFER3_KERNEL:
		{
			VDEC_SET_BUFFER3_t *arg = (VDEC_SET_BUFFER3_t *) args;
			{
				int ii;
				dec_buffer3_t *pDecBuffer3 = &arg->gsVpuDecBuffer3;

				dprintk_vpu("Dec-%d: vpu_proc_reg_framebuffer3 :: cnt = %d", type, pDecBuffer3->m_ulFrameBufferCount);
				for (ii = 0; ii < pDecBuffer3->m_ulFrameBufferCount; ii++) {
					dprintk_vpu("[%d] addrY:0x%x, addrCb:0x%x, addrCr:0x%x, mvcol:0x%x",
						ii, pDecBuffer3->m_addrFrameBuffer[0][ii][0], pDecBuffer3->m_addrFrameBuffer[0][ii][1], pDecBuffer3->m_addrFrameBuffer[0][ii][2], pDecBuffer3->m_addrFrameBuffer[0][ii][3]);
				}

				dprintk_vpu("avcSliceSave:0x%x, vp8mbdata:0x%x", pDecBuffer3->m_AvcSliceSaveBufferAddr, pDecBuffer3->m_Vp8MbDataSaveBufferAddr);
			}

			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsVpuDecBuffer3),
				(void *)NULL);

			V_DBG(VPU_DBG_SEQUENCE,
				"Dec-%d: VPU_DEC_REG_FRAME_BUFFER3 out",
				type);
		}
		break;

		case VPU_DEC_DECODE:
		case VPU_DEC_DECODE_KERNEL:
		case V2D_IP_DEC_FRMDATA:
		{
			VDEC_DECODE_t *arg = NULL;
			bool isFlexible = (cmd == V2D_IP_DEC_FRMDATA);
			int stream_size = 0;

			bool delayed_ringmode;
			bool detecting_field = false;
			bool isPending = false;
			uint32_t i = 0;

			for (i = 0; i < (uint32_t)VPU_MAX; i++) {
				if ((i != type) && (vmgr_data.vpu_ctrl.pending_count[i] > 0)) {
					(void)pr_err("[%s] vpu fw is in a pending state by instance(%s)"
							" (pending count: %d)",
							vpu_vputype_to_string(type), vpu_vputype_to_string(i),
							vmgr_data.vpu_ctrl.pending_count[i]);
					isPending = true;
					break;
				}
			}

			if (isFlexible) {
				arg = (VDEC_DECODE_t *) v2fhdmgr_unmarshal_ip_frmdata(
						args, (vcodec_handle_t *)&pHandle, tcc_vpu_dec_l);
				cmd = VPU_DEC_DECODE;
				delayed_ringmode = v2fhdmgr_ip_check_delayed_ring_mode(args);
			} else {
				arg = (VDEC_DECODE_t *) args;
				delayed_ringmode = false;
			}

			if (arg == NULL) {
				ret = RETCODE_FAILURE;
			} else {
				stream_size = arg->gsVpuDecInput.m_iBitstreamDataSize;
				if ((isFlexible == false) && (vmgr_data.vpu_ctrl.avoid_pending == 1)) {
					V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
						"[%s] handle:0x%x, args:0x%x, arg:0x%x, "
						"Avoid pending! Decode with size (%d ==> 0)",
						vpu_vputype_to_string(type), pHandle, args, arg,
						arg->gsVpuDecInput.m_iBitstreamDataSize);
					arg->gsVpuDecInput.m_iBitstreamDataSize = 0;
				}

				vmgr_data.szFrame_Len = (arg->gsVpuDecInput.m_iBitstreamDataSize > 0)
					? (unsigned int)arg->gsVpuDecInput.m_iBitstreamDataSize : 0U;

				V_DBG(VPU_DBG_THREAD,
					"[%s] In => %#x - %#x, %d, %#x - %#x, %d, flag: %d / %d / %d",
					vpu_vputype_to_string(type),
					arg->gsVpuDecInput.m_BitstreamDataAddr[PA],
					arg->gsVpuDecInput.m_BitstreamDataAddr[VA],
					arg->gsVpuDecInput.m_iBitstreamDataSize,
					arg->gsVpuDecInput.m_UserDataAddr[PA],
					arg->gsVpuDecInput.m_UserDataAddr[VA],
					arg->gsVpuDecInput.m_iUserDataBufferSize,
					arg->gsVpuDecInput.m_iFrameSearchEnable,
					arg->gsVpuDecInput.m_iSkipFrameMode,
					arg->gsVpuDecInput.m_iSkipFrameNum);

				vmgr_data.check_interrupt_detection = 1;

				if (isPending == false) {
					ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
						(vcodec_handle_t *)&pHandle,
						(void *)(&arg->gsVpuDecInput),
						(void *)(&arg->gsVpuDecOutput));
					isPending = (ret == RETCODE_INSTANCE_MISMATCH_ON_PENDING_STATUS);
				}

				if (isPending) {
					ret = RETCODE_SUCCESS;
					arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodedIdx = -1;
					arg->gsVpuDecOutput.m_DecOutInfo.m_iDispOutIdx = -1;
					if (isFlexible) {
						arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodingStatus = VPU_DEC_FW_PENDING;
					} else {
						arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodingStatus = VPU_DEC_BUF_FULL;
					}
					arg->gsVpuDecOutput.m_DecOutInfo.m_iOutputStatus = 0;
				} else {
					switch (ret) {
					case RETCODE_INSUFFICIENT_BITSTREAM:
						vmgr_data.vpu_ctrl.pending_count[type]++;
						break;

					case RETCODE_SUCCESS:
						if (vmgr_data.vpu_ctrl.pending_count[type] != 0) {
							vmgr_data.vpu_ctrl.pending_count[type] = 0;
						}
						break;

					default:
						break;
					}
				}

				{
					int dbg_cmd = VPU_DBG_THREAD;
					if (vmgr_data.vpu_ctrl.avoid_pending == 1) {
						dbg_cmd = VPU_DBG_SEQUENCE;
						vmgr_data.vpu_ctrl.avoid_pending = 0; // reset
						arg->gsVpuDecInput.m_iBitstreamDataSize = stream_size;
					}

					V_DBG(dbg_cmd,
						"[%s] Out => W/H(%d/%d), L/R/T/B(%d/%d/%d/%d)",
						vpu_vputype_to_string(type),
						arg->gsVpuDecOutput.m_DecOutInfo.m_iWidth,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iHeight,
						arg->gsVpuDecOutput.m_DecOutInfo.m_CropInfo.m_iCropLeft,
						arg->gsVpuDecOutput.m_DecOutInfo.m_CropInfo.m_iCropRight,
						arg->gsVpuDecOutput.m_DecOutInfo.m_CropInfo.m_iCropTop,
						arg->gsVpuDecOutput.m_DecOutInfo.m_CropInfo.m_iCropBottom);

					V_DBG(dbg_cmd,
						  "[%s] Out => stream(%d), ret(%d), PicType(%d), "
						  "Idx_Dec/Out(%d/%d), Status_Dec/Out(%d/%d)",
						vpu_vputype_to_string(type), stream_size, ret,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iPicType,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodedIdx,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iDispOutIdx,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodingStatus,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iOutputStatus);

					V_DBG(dbg_cmd,
						"[%s] Out => OutPA(%#x %#x %#x)",
						vpu_vputype_to_string(type),
						arg->gsVpuDecOutput.m_pDispOut[PA][0],
						arg->gsVpuDecOutput.m_pDispOut[PA][1],
						arg->gsVpuDecOutput.m_pDispOut[PA][2]);
				}

				if (arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodingStatus == VPU_DEC_BUF_FULL) {
					err_vpu("[%s] Buffer full", vpu_vputype_to_string(type));
				}

				vmgr_data.nDecode_Cmd++;

				if (isFlexible) {
					v2fhdmgr_marshal_op_frmdata(args, (ret == RETCODE_SUCCESS));
				}

				detecting_field = delayed_ringmode
							? (ret == RETCODE_INSUFFICIENT_BITSTREAM)
							: ((ret == 0) &&
							(arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodingStatus
							== VPU_DEC_SUCCESS_FIELD_PICTURE));

				if (detecting_field) {
					int dbg_cmd = VPU_DBG_ILV_INFO;
					if (vmgr_data.vpu_ctrl.interlaced[type] == 0) {
						vmgr_data.vpu_ctrl.interlaced[type] = 1;
						vmgr_data.vpu_ctrl.interlaced_video_total++;
						dbg_cmd = VPU_DBG_SEQUENCE;
					}

					V_DBG(dbg_cmd, VLOG_TAG
						"[%s][0x%x] Interlace video:FIELD_PICTURE, size=%d, "
						"Out => ret[%d] !! PicType[%d], Idx[%d/%d], Status[%d/%d]",
						vpu_vputype_to_string(type), pHandle,
						arg->gsVpuDecInput.m_iBitstreamDataSize, ret,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iPicType,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodedIdx,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iDispOutIdx,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iDecodingStatus,
						arg->gsVpuDecOutput.m_DecOutInfo.m_iOutputStatus);

					if (isFlexible) {
						struct v2hw_flex_io *fli;
						fli = (struct v2hw_flex_io *)args;
						if (fli != NULL) {
							struct vpu_decoder_data *vdata;
							union { void *ptr; uint64_t off; } uniaddr;

							uniaddr.off = fli->v1_strt;
							vdata = (struct vpu_decoder_data *)uniaddr.ptr;

							if (vdata->flx_io_delay != NULL) {
								if (vmgr_data.vpu_ctrl.pending_vtype == type) {
									(void)pr_err("detect consecutive field frame (type %d)", type);
								}

								vmgr_data.vpu_ctrl.pending_vtype = type;
								vmgr_data.vpu_ctrl.vdata[type] = vdata;

								/* Update the timestamp when a field decoding is fetched */
								vmgr_data.vpu_ctrl.last_timestamp = jiffies;
							} else {
								(void)pr_err("[%s] flex dio is not ready !!", vpu_vputype_to_string(type));
							}
						} else {
							(void)pr_err("[%s] fli is nullified !!", vpu_vputype_to_string(type));
						}
					} else {
						vmgr_data.vpu_ctrl.pending_vtype = type;
					}
				}
			}
		}
		break;

		case VPU_DEC_BUF_FLAG_CLEAR:
		case VPU_DEC_BUF_FLAG_CLEAR_KERNEL:
		{
			int *arg = (int *)args;

			V_DBG(VPU_DBG_FB_CLR_STATE,
				"Dec-%d :: DispIdx Clear %d",
				type, *arg);
			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)(arg), (void *)NULL);
		}
		break;

		case V2D_IP_FRM_CLEAR:
		{
			int slot = -1;
			struct v2hw_flex_io *fli = (struct v2hw_flex_io *)args;
			V2_FLEXIP_GET(fli, CLEAR_FB_IDX, slot);

			ret = tcc_vpu_dec_l(VPU_DEC_BUF_FLAG_CLEAR,
							  (vcodec_handle_t *)&pHandle,
							  (void *)&slot,
							  (void *)NULL);
			if (ret != RETCODE_SUCCESS) {
				(void)pr_err("failed to clear slot %d (ret: %d)", slot, ret);
			}
		}
		break;

		case V2D_IP_FRM_FLUSH:
		{
			int slot = 0, max_count = 0;
			struct v2hw_flex_io *fli = (struct v2hw_flex_io *)args;
			V2_FLEXIP_GET(fli, FLUSH_FB_MAX, max_count);

			if (fli != NULL) {
				struct vpu_decoder_data *vdata;
				union { void *ptr; uint64_t off; } uniaddr;

				for (; slot < max_count; slot++) {
					uint64_t op_key = (1LLU << (unsigned)slot);
					if ((fli->op_keys & op_key) != 0U) {
						ret = tcc_vpu_dec_l(VPU_DEC_BUF_FLAG_CLEAR,
										  (vcodec_handle_t *)&pHandle,
										  (void *)&slot,
										  (void *)NULL);
						if (ret == RETCODE_SUCCESS) {
							fli->op_keys &= ~op_key;
						} else {
							(void)pr_err(
								"failed to clear slot %d (ret: %d)", slot, ret);
						}
					}
				}

				uniaddr.off = fli->v1_strt;
				vdata = (struct vpu_decoder_data *)uniaddr.ptr;

				if (vdata != NULL) {
					struct v2hw_io_delay *dio;
					dio = vdata->flx_io_delay;
					if (dio != NULL) {
						if (vmgr_data.vpu_ctrl.pending_vtype == -1) {
							/* reset dio input state: delay -> empty */
							size_t i;
							for (i = 0u; i < 2u; i++) {
								if (dio->ip_state[i] == V2_DIO_DELAY) {
									(void)pr_info("flush: reset delayed cq2 index %zu", i);
									dio->ip_state[i] = V2_DIO_EMPTY;
								}
							}

							/* reset dio output state: delay -> empty */
							dio->op_state = V2_DIO_EMPTY;
						}
						(void)pr_info("flush: pending type %d, time %ld, op state %d, ret %d",
								vmgr_data.vpu_ctrl.pending_vtype,
								vmgr_data.vpu_ctrl.last_timestamp, dio->op_state, ret);
					}
				}
			}
		}
		break;

		case VPU_DEC_FLUSH_OUTPUT:
		case VPU_DEC_FLUSH_OUTPUT_KERNEL:
		case V2D_IP_FRM_DRAIN:
		{
			VDEC_DECODE_t *arg;

			bool isFlexible = (cmd == V2D_IP_FRM_DRAIN);
			if (isFlexible) {
				arg = (VDEC_DECODE_t *) v2fhdmgr_unmarshal_ip_drndata(args);
				cmd = VPU_DEC_FLUSH_OUTPUT;
			} else {
				arg = (VDEC_DECODE_t *) args;
			}

			V_DBG(VPU_DBG_FB_CLR_STATE,
				"Dec-%d :: VPU_DEC_FLUSH_OUTPUT !!", type);

			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsVpuDecInput),
				(void *)(&arg->gsVpuDecOutput));

			if (isFlexible) {
				v2fhdmgr_marshal_op_frmdata(args, (ret == RETCODE_SUCCESS));
			}
		}
		break;

		case VPU_DEC_CLOSE:
		case VPU_DEC_CLOSE_KERNEL:
		case V2D_IP_DRV_RST:
		{
			bool isFlexible = (cmd == V2D_IP_DRV_RST);
			if (isFlexible) {
				cmd = VPU_DEC_CLOSE;
			}

			vmgr_data.check_interrupt_detection = 1;

			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)NULL, (void *)NULL
				/*(&arg->gsVpuDecOutput) */);
			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				"[%s][cmd:0x%x(%d)] handle(%x), VPU_DEC_CLOSED(%d) !!",
				vpu_vputype_to_string(type), pHandle, cmd, cmd, isFlexible);

			vmgr_set_close_and_free_mem(type);

			if (isFlexible && (vmgr_data.vpu_ctrl.pending_vtype == type)) {
				(void)pr_err("%s: V2D_IP_DRV_RST - reset pending vpu type", vpu_vputype_to_string(type));
				vmgr_data.vpu_ctrl.pending_vtype = -1;
				vmgr_data.vpu_ctrl.avoid_pending = -1;
			}
		}
		break;

		case GET_RING_BUFFER_STATUS:
		case GET_RING_BUFFER_STATUS_KERNEL:
		case V2D_IP_RNG_GETPOS:
		{
			VDEC_RINGBUF_GETINFO_t *arg;

			bool isFlexible = (cmd == V2D_IP_RNG_GETPOS);
			if (isFlexible) {
				arg = (VDEC_RINGBUF_GETINFO_t *)v2fhdmgr_unmarshal_ip_getpos(args);
				cmd = GET_RING_BUFFER_STATUS;
			} else {
				arg = (VDEC_RINGBUF_GETINFO_t *)args;
			}

			vmgr_data.check_interrupt_detection = 1;

			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)NULL,
				(void *)(&arg->gsVpuDecRingStatus));

			if (isFlexible && (ret == RETCODE_SUCCESS)) {
				v2fhdmgr_marshal_op_getpos(args);
			}
		}
		break;

		case FILL_RING_BUFFER_AUTO:
		case FILL_RING_BUFFER_AUTO_KERNEL:
		{
			VDEC_RINGBUF_SETBUF_t *arg =
				(VDEC_RINGBUF_SETBUF_t *) args;

			uint32_t read_ptr = vetc_reg_read(vmgr_data.base_addr, 288U);
			uint32_t write_ptr = vetc_reg_read(vmgr_data.base_addr, 292U);

			vmgr_data.check_interrupt_detection = 1;
			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsVpuDecInit),
				(void *)(&arg->gsVpuDecRingFeed));
			V_DBG(VPU_DBG_SEQUENCE,
				"Dec-%d :: ReadPTR : 0x%08x, WritePTR : 0x%08x",
				type, read_ptr, write_ptr);
		}
		break;

		case VPU_UPDATE_WRITE_BUFFER_PTR:
		case VPU_UPDATE_WRITE_BUFFER_PTR_KERNEL:
		case V2D_IP_RNG_SETPOS:
		{
			VDEC_RINGBUF_SETBUF_PTRONLY_t *arg = NULL;
			union {
				int i_data;
				int *pi_data;	//NULL
				void *pv_data;
			} ucopysize, flushbuf;


			bool isFlexible = (cmd == V2D_IP_RNG_SETPOS);
			if (isFlexible) {
				arg = (VDEC_RINGBUF_SETBUF_PTRONLY_t *)v2fhdmgr_unmarshal_ip_setpos(args);
				cmd = VPU_UPDATE_WRITE_BUFFER_PTR;
			} else {
				arg = (VDEC_RINGBUF_SETBUF_PTRONLY_t *) args;
			}

			if (arg == NULL) {
				ret = RETCODE_FAILURE;
			} else {
				ucopysize.pi_data = NULL;
				ucopysize.i_data = arg->iCopiedSize;
				flushbuf.pi_data = NULL;
				flushbuf.i_data = arg->iFlushBuf;

				vmgr_data.check_interrupt_detection = 1;
				ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
					(vcodec_handle_t *)&pHandle,
					ucopysize.pv_data,
					flushbuf.pv_data);
			}
		}
		break;

		case GET_INITIAL_INFO_FOR_STREAMING_MODE_ONLY:
		case GET_INITIAL_INFO_KERNEL_FOR_STREAMING_MODE_ONLY:
		{
			VDEC_SEQ_HEADER_t *arg =
				(VDEC_SEQ_HEADER_t *) args;
			vmgr_data.check_interrupt_detection = 1;
			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsVpuDecInitialInfo),
				NULL);
		}
		break;

		case VPU_CODEC_GET_VERSION:
		case VPU_CODEC_GET_VERSION_KERNEL:
		{
			const VDEC_GET_VERSION_t *arg =
				(VDEC_GET_VERSION_t *) args;
			vmgr_data.check_interrupt_detection = 1;
			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				arg->pszVersion,
				arg->pszBuildData);
			dprintk_vpu("Dec-%d :: version : %s, build : %s",
				type, arg->pszVersion,
				arg->pszBuildData);
		}
		break;

		case VPU_DEC_SWRESET:
		case VPU_DEC_SWRESET_KERNEL:
		{
			ret = tcc_vpu_dec_l(cmd & ~VPU_BASE_OP_KERNEL,
				(vcodec_handle_t *)&pHandle,
				NULL, NULL);
		}
		break;

		default:
		{
			err_vpu("Dec-%d :: not supported command(0x%x)",
				type, cmd);
			ret = 0x999;
		}
		break;

		}
	}
#if !defined(VPU_D6)
#if DEFINED_CONFIG_VENC_CNT_1to16
	else {
		if (cmd != VPU_ENC_INIT) {
			if (vmgr_get_close(type)
				|| vmgr_data.handle[type] == 0x00) {
				return RETCODE_MULTI_CODEC_EXIT_TIMEOUT;
			}
		}

		if (tcc_vpu_enc == NULL) {
			return RETCODE_INVALID_HANDLE;
		}

		if (cmd != VPU_ENC_ENCODE) {
			V_DBG(VPU_DBG_CMD, "Encoder(%d), command: %#x", type, cmd);
		}

		switch (cmd) {
		case VPU_ENC_INIT:
		{
			VENC_INIT_t *arg = (VENC_INIT_t *) args;
			union_codec_handle_t codec_handle;

			int width, height;
			unsigned int vpu_lib_dbg = get_vpu_lib_dbg_param();

			vmgr_data.handle[type] = 0x00;

			width = arg->gsVpuEncInit.m_iPicWidth;
			if ((width < 16) || (width > VPU_LIMIT_PICWIDTH)) {
				err_vpu("## Enc :: not supported pic. width(%d)", width);
				return RETCODE_INVALID_STRIDE;
			}

			height = arg->gsVpuEncInit.m_iPicHeight;
			if ((height < 16) || (height > VPU_LIMIT_PICHEIGHT)) {
				err_vpu("## Enc :: not supported pic. height(%d)", height);
				return RETCODE_INVALID_STRIDE;
			}

			vmgr_data.szFrame_Len = (((unsigned)width *
									  (unsigned)height * 3) / 2);

			arg->gsVpuEncInit.m_RegBaseVirtualAddr =
				(codec_addr_t) vmgr_data.base_addr;
			arg->gsVpuEncInit.m_Memcpy =
				(void *(*)(void *, const void *, unsigned int,
					unsigned int))vetc_memcpy;
			arg->gsVpuEncInit.m_Memset = (void (*)
					(void *, int,
					unsigned int,
					unsigned int)) vetc_memset;
			arg->gsVpuEncInit.m_Interrupt =
				(int (*)(void))vmgr_internal_handler;
			arg->gsVpuEncInit.m_Ioremap =
				(void *(*)(phys_addr_t,
					unsigned int)) vetc_ioremap;
			arg->gsVpuEncInit.m_Iounmap =
				(void (*)(void *))vetc_iounmap;
				arg->gsVpuEncInit.m_reg_read =
				(unsigned int (*)(void *,
					unsigned int)) vetc_reg_read;
			arg->gsVpuEncInit.m_reg_write =
				(void (*)(void *,
					unsigned int,
					unsigned int)) vetc_reg_write;

			codec_handle.pcodec_handle = &arg->gsVpuEncHandle;

			if ((vpu_lib_dbg & VPU_DBG_LIB_USE_CB_PRINTK) == VPU_DBG_LIB_USE_CB_PRINTK) {
				unsigned int codec_ip;
				vpu_dec_ctrl_log_status_t enc_log;

				// Extract codec_ip (A part), please refer to enum vpu_ip_type
				// VPU_IP_C7 = 1, VPU_IP_4KD2 = 2, VPU_IP_HEVC_ENC = 3, VPU_IP_HEVC_ENC2 = 4, VPU_IP_JPU_C6 = 5, VPU_IP_HEVC_DEC
				codec_ip = (vpu_lib_dbg & 0x00F000U) >> 12;
				V_DBG(VPU_DBG_ERROR, "[VPU] codec_ip: %d", codec_ip);

				// Check if codec_ip matches desired value
				if (codec_ip == 1) {
					// Check if codec_ip matches desired value
					// Extract log_mask (BBB part)
					unsigned int log_mask = (vpu_lib_dbg & 0x000FFFU);

					V_DBG(VPU_DBG_ERROR, "[VPU] log_mask: %d (%x)", log_mask, log_mask);
					enc_log.pfLogPrintCb = (void (*)(const char *, ...))vpu_printk;
					enc_log.stLogLevel.bVerbose = (log_mask & 1U) ? 1 : 0;
					enc_log.stLogLevel.bDebug	= (log_mask & 2U) ? 1 : 0;
					enc_log.stLogLevel.bInfo	  = (log_mask & 4U) ? 1 : 0;
					enc_log.stLogLevel.bWarn	  = (log_mask & 8U) ? 1 : 0;
					enc_log.stLogLevel.bError	= (log_mask & 16U) ? 1 : 0; // 0x10
					enc_log.stLogLevel.bAssert	= (log_mask & 32U) ? 1 : 0; // 0x20
					enc_log.stLogLevel.bFunc	  = (log_mask & 64U) ? 1 : 0; // 0x40
					enc_log.stLogLevel.bTrace	= (log_mask & 128U) ? 1 : 0; // 0x80
					ret = tcc_vpu_enc_l(VPU_CTRL_LOG_STATUS, NULL, (void *)(&enc_log), (void *)NULL);
				}
			}

			ret = tcc_vpu_enc_l(cmd,
				(vcodec_handle_t *)(codec_handle.pvcodec_handle),
				(void *)(&arg->gsVpuEncInit),
				(void *)(&arg->gsVpuEncInitialInfo));

			if (ret != RETCODE_SUCCESS) {
				V_DBG(VPU_DBG_ERROR,
					"## Enc :: Init Done with ret(0x%x)",
					ret);
				if (ret != RETCODE_CODEC_EXIT) {
					static unsigned char fname[] = "Init error";

					vetc_dump_reg_all(
						(char *)vmgr_data.base_addr, fname);
				}
			}

			if (ret != RETCODE_CODEC_EXIT
				&& arg->gsVpuEncHandle != 0) {
				vmgr_data.handle[type] =
					arg->gsVpuEncHandle;
				(void)vmgr_set_close(type, 0, 0);
				V_DBG(VPU_DBG_CMD,
					"Enc vmgr_data.handle = 0x%x",
					arg->gsVpuEncHandle);
			} else {
				//To free memory!!
				(void)vmgr_set_close(type, 0, 0);
				(void)vmgr_set_close(type, 1, 1);
			}
			V_DBG(VPU_DBG_SEQUENCE,
				"Enc :: Init Done Handle(0x%x)",
				arg->gsVpuEncHandle);
			vmgr_data.nDecode_Cmd = 0;

#ifdef CONFIG_VPU_TIME_MEASUREMENT
			vmgr_data.iTime[type].print_out_index = 0;
			vmgr_data.iTime[type].proc_base_cnt = 0;
			vmgr_data.iTime[type].accumulated_proc_time = 0;
			vmgr_data.iTime[type].accumulated_frame_cnt = 0;
			vmgr_data.iTime[type].proc_time_30frames = 0;
#endif
		}
		break;

		case VPU_ENC_REG_FRAME_BUFFER:
		{
			VENC_SET_BUFFER_t *arg =
				(VENC_SET_BUFFER_t *) args;
			ret = tcc_vpu_enc_l(cmd,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsVpuEncBuffer),
				(void *)NULL);
		}
		break;

		case VPU_ENC_PUT_HEADER:
		{
			VENC_PUT_HEADER_t *arg =
				(VENC_PUT_HEADER_t *) args;

#if !defined(VPU_C5)
			vmgr_data.check_interrupt_detection = 1;
#endif

			ret = tcc_vpu_enc_l(cmd,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsVpuEncHeader),
				(void *)NULL);
		}
		break;

		case VPU_ENC_ENCODE:
		{
			VENC_ENCODE_t *arg = (VENC_ENCODE_t *) args;

			V_DBG(VPU_DBG_INFO,
				"Enc In !! Handle = %#x, %#x-%#x-%#x, %d-%d-%d, "
				"%d-%d-%d, %d, %#x-%d",
				pHandle,
				arg->gsVpuEncInput.m_PicYAddr,
				arg->gsVpuEncInput.m_PicCbAddr,
				arg->gsVpuEncInput.m_PicCrAddr,
				arg->gsVpuEncInput.m_iForceIPicture,
				arg->gsVpuEncInput.m_iSkipPicture,
				arg->gsVpuEncInput.m_iQuantParam,
				arg->gsVpuEncInput.m_iChangeRcParamFlag,
				arg->gsVpuEncInput.m_iChangeTargetKbps,
				arg->gsVpuEncInput.m_iChangeFrameRate,
				arg->gsVpuEncInput.m_iChangeKeyInterval,
				arg->gsVpuEncInput.m_BitstreamBufferAddr,
				arg->gsVpuEncInput.m_iBitstreamBufferSize);

			vmgr_data.check_interrupt_detection = 1;
			ret = tcc_vpu_enc_l(cmd,
				(vcodec_handle_t *)&pHandle,
				(void *)(&arg->gsVpuEncInput),
				(void *)(&arg->gsVpuEncOutput));

#ifdef VPU_REGISTER_DUMP
			if (arg->gsVpuEncInput.m_iForceIPicture != 0) {
				V_DBG(VPU_DBG_REG_DUMP,
					"ForceIPicture = %d, 0x%x - 0x%x",
					arg->gsVpuEncInput.m_iForceIPicture,
					_vmgr_reg_read(0x194),
					_vmgr_reg_read(0x1C4));
			}
#else
#if 0
			if (arg->gsVpuEncInput.m_iChangeRcParamFlag != 0
				&& (arg->gsVpuEncInput.m_iChangeTargetKbps != 0
				|| arg->gsVpuEncInput.m_iChangeFrameRate !=
				0)) {
				V_DBG(VPU_DBG_REG_DUMP,
					"Flag(%d) :: %d Kbps, %d fps => %d kbps, %d bit, %d Qp",
					arg->gsVpuEncInput.m_iChangeRcParamFlag,
					arg->gsVpuEncInput.m_iChangeTargetKbps,
					arg->gsVpuEncInput.m_iChangeFrameRate,
					_vmgr_reg_read(0x128),
					_vmgr_reg_read(0x12C),
					_vmgr_reg_read(0x1D4));
			} else {
				if (arg->gsVpuEncInput.m_iChangeTargetKbps !=
					_vmgr_reg_read(0x128)) {
					V_DBG(VPU_DBG_REG_DUMP,
						"%d Kbps => %d kbps, %d bit, %d Qp",
						arg->gsVpuEncInput
							.m_iChangeTargetKbps,
						_vmgr_reg_read(0x128),
						_vmgr_reg_read(0x12C),
						_vmgr_reg_read(0x1D4));
				}
			}
#endif
#endif

			vmgr_data.nDecode_Cmd++;

			V_DBG(VPU_DBG_SEQUENCE,
				"Enc Out[%d] !! PicType[%d], Encoded_size[%d]",
				ret,
				arg->gsVpuEncOutput.m_iPicType,
				arg->gsVpuEncOutput
					.m_iBitstreamOutSize);
		}
		break;

		case VPU_ENC_CLOSE:
		{
			vmgr_data.check_interrupt_detection = 1;
			ret =
				tcc_vpu_enc_l(cmd,
					(vcodec_handle_t *)&pHandle,
					(void *)NULL, (void *)NULL);
			V_DBG(VPU_DBG_CLOSE,
				"Enc VPU_ENC_CLOSED !!");
			(void)vmgr_set_close(type, 1, 1);
		}
		break;

		default:
			err_vpu("## Enc :: not supported command(0x%x)", cmd);
			ret = 0x999;
		break;
		}
	}
#endif
#endif

#ifdef CONFIG_VPU_TIME_MEASUREMENT
	endTime = vetc_GetKtime();
	time_gap_us = vetc_GetTimediff_us(endTime, startTime);

	if (cmd == VPU_DEC_INIT || cmd == VPU_ENC_INIT) {
		V_DBG(VPU_DBG_PERF, "Elapsed time for V%s_INIT[dev-%u] is %d us",
			cmd == VPU_DEC_INIT ? "DEC" : "ENC",
			type,
			time_gap_us);
	} else if (cmd == VPU_DEC_DECODE || cmd == VPU_ENC_ENCODE) {
		printMeasurementTime((void *)&vmgr_data, type, ((cmd == VPU_DEC_DECODE) ? 1 : 0),  time_gap_us);
	}
#endif

	return ret;
}

static int vmgr_proc_exit_by_external(struct VpuList *list, int *result,
					   vputype type)
{
	int ret = 0;
	if ((vmgr_get_close(type) == 0) &&
		(vmgr_data.handle[(unsigned int)type] != 0x00)) {
		list->type = (unsigned int) type;
		if (type >= VPU_ENC) {
			list->cmd_type = VPU_ENC_CLOSE;
		} else {
			list->cmd_type = VPU_DEC_CLOSE;
		}
		list->handle = vmgr_data.handle[(unsigned int)type];
		list->args = NULL;
		list->comm_data = NULL;
		list->vpu_result = result;

		V_DBG(VPU_DBG_SEQUENCE,
			"vmgr process exit by external for %d!!", (unsigned int)type);
		(void)vmgr_list_manager(list, (unsigned int)LIST_ADD);

		ret = 1;
	}

	return ret;
}

#if 0 // Keep the code for future use
static void vmgr_wait_process(int wait_ms)
{
	int max_count = wait_ms / 20;

	//wait!! in case exceptional processing. ex). sdcard out!!
	while (vmgr_data.cmd_processing) {
		max_count--;
		msleep(20);

		if (max_count <= 0) {
			err_vpu("cmd_processing(cmd %d) didn't finish!!",
				vmgr_data.current_cmd);
			break;
		}
	}
}
#endif

static int vmgr_external_all_close(int wait_ms)
{
	unsigned int type;
	int max_count;
	int ret;

	for (type = 0U; type < (unsigned int)VPU_MAX; type++) {
		if (vmgr_proc_exit_by_external(
			&vmgr_data.vList[type], &ret, (vputype) type) > 0) {
			max_count = wait_ms / 10;

			while (vmgr_get_close((vputype) type) == 0) {
				if (max_count == 0) {
					break;
				} else {
					max_count--;
				}
				usleep_range(10000, 11000); //msleep(10);
			}
		}
	}

	return 0;
}

static int vmgr_cmd_open(char *str, vputype vpu_type)
{
	int ret = 0;
	int vmgr_count = vmgr_get_alive();

	if (!IS_VALID_VPU_TYPE(vpu_type)) {
		V_DBG(VPU_DBG_ERROR, VLOG_TAG
			"[%s] invalid vpu_type(%d) for release",
			vpu_vputype_to_string(vpu_type), vpu_type);
		return -EINVAL;
	}

	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
		"[%s] %s Begin (vmgr:%d)",
		vpu_vputype_to_string(vpu_type), str, vmgr_count);

	vmgr_sys_assert(vmgr_count);

	vmgr_data.vpu_ctrl.last_vlist[vpu_type] = NULL;
	vmgr_data.vpu_ctrl.interlaced[vpu_type] = 0;
	vmgr_data.vpu_ctrl.last_timestamp = 0;

	if (vmgr_count == 0) {
		atomic_set(&vmgr_data.oper_intr, 0);
		vmgr_remove_all_entries(&vmgr_data.comm_data.main_list);
		vmgr_data.cmd_processing = 0;

		vmgr_data.timeout_exit = 0;
		vmgr_data.timeout_vpu_type = -1;
		vmgr_data.vpu_ctrl.pending_vtype = -1;
		vmgr_data.vpu_ctrl.avoid_pending = -1;

		#ifdef FORCED_ERROR
		forced_error_count = FORCED_ERR_CNT;
		#endif
	}

	atomic_inc(&vmgr_data.opened);

	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
		"[%s] %s End.. (vmgr:%d)",
		vpu_vputype_to_string(vpu_type), str, vmgr_get_alive());

	return ret;
}

static int vmgr_cmd_release(char *str, vputype vpu_type)
{
	int vmgr_count = vmgr_get_alive();

	if (!IS_VALID_VPU_TYPE(vpu_type)) {
		V_DBG(VPU_DBG_ERROR, VLOG_TAG
			"[%s] invalid vpu_type(%d) for release",
			vpu_vputype_to_string(vpu_type), vpu_type);
		return -EINVAL;
	}

	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
		"[%s] %s Begin (vmgr:%d)",
		vpu_vputype_to_string(vpu_type), str, vmgr_count);

	atomic_dec(&vmgr_data.opened);

	vmgr_count = vmgr_get_alive();

	vmgr_sys_deassert(vmgr_count);

	if (vmgr_data.vpu_ctrl.last_vlist[vpu_type] != NULL) {
		if (vmgr_data.vpu_ctrl.last_vlist[vpu_type]->args != NULL) {
			if (vmgr_data.vpu_ctrl.last_vlist[vpu_type]->cmd_type == VPU_DEC_DECODE) {
				kfree(vmgr_data.vpu_ctrl.last_vlist[vpu_type]->args);
			}
		}
		kfree(vmgr_data.vpu_ctrl.last_vlist[vpu_type]);
		vmgr_data.vpu_ctrl.last_vlist[vpu_type] = NULL;
	}

	if (vmgr_data.vpu_ctrl.interlaced[vpu_type] == 1) {
		vmgr_data.vpu_ctrl.interlaced[vpu_type] = 0;
		vmgr_data.vpu_ctrl.interlaced_video_total--;
	}

	if (vmgr_data.nOpened_Count >= UINT_MAX) {
		vmgr_data.nOpened_Count = 0;
	} else {
		vmgr_data.nOpened_Count++;
	}

	V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
		"[%s] %s End.. (vmgr:%d), (total:%d), cmd_queued(%d)",
		vpu_vputype_to_string(vpu_type), str, vmgr_count, vmgr_data.nOpened_Count, vmgr_data.cmd_queued);
	return 0;
}

static unsigned int hangup_rel_count;
static long vmgr_ioctl_l(struct file *filp, unsigned int cmd, unsigned long arg)
{
	int ret = 0;
	int command = 0;
	struct vmgr_t *p_vmgr = filp->private_data;
	CONTENTS_INFO info;
	OPENED_sINFO open_info;
	union {
		unsigned long ul_data;
		unsigned int *pui_data;
		int *pi_data;
		void *pv_data;
		const void *pcv_data;	//NULL
		CONTENTS_INFO *pci_data;
		OPENED_sINFO *posi_data;
	} uarg;

	VPU_UNUSED_PARAMETER(filp);

	uarg.pcv_data = NULL;
	uarg.ul_data = arg;

	command = (cmd > INT_MAX_U) ? INT_MAX_S : ((int)cmd);

	switch (command) {
	case VPU_SET_CLK:
	case VPU_SET_CLK_KERNEL:
		if (command == VPU_SET_CLK_KERNEL) {
			(void)memcpy(&info, (CONTENTS_INFO *)uarg.pci_data,
				sizeof(info));

			if ((info.type >= VPU_ENC) && (info.isSWCodec > 0U)) {
				vmgr_data.clk_limitation = 0;
				V_DBG(VPU_DBG_RSTCLK,
					"The clock limitation for VPU is released.");
			}
		} else {
			if (copy_from_user
				(&info, (CONTENTS_INFO *)uarg.pci_data,
				sizeof(info)) > 0U) {
				ret = -EFAULT;
			} else {
				if ((info.type >= VPU_ENC)
					&& (info.isSWCodec > 0U)) {
					vmgr_data.clk_limitation = 0;
					V_DBG(VPU_DBG_RSTCLK,
						"The clock limitation for VPU is released.");
				}
			}
		}
		break;

	case VPU_GET_FREEMEM_SIZE:
	case VPU_GET_FREEMEM_SIZE_KERNEL:
		{
			vputype type = VPU_DEC;
			unsigned int freemem_sz;

			if (command == VPU_GET_FREEMEM_SIZE_KERNEL) {
				(void)memcpy(&type, (unsigned int *)uarg.pui_data,
					sizeof(unsigned int));

				if (type > VPU_MAX) {
					type = VPU_DEC;
				}

				freemem_sz = vmem_get_freemem_size(type);

				(void)memcpy((unsigned int *)uarg.pui_data, &freemem_sz,
					sizeof(unsigned int));
			} else {
				if (copy_from_user
					(&type, (unsigned int *)uarg.pui_data,
						sizeof(unsigned int)) > 0U) {
					ret = -EFAULT;
				} else {
					if (type > VPU_MAX) {
						type = VPU_DEC;
					}

					freemem_sz =
						vmem_get_freemem_size(type);

					if (copy_to_user(
						(unsigned int *)uarg.pui_data,
						&freemem_sz,
						sizeof(unsigned int)) != 0U) {
						ret = -EFAULT;
					}
				}
			}
		}
		break;

	case VPU_HW_RESET:
		vmgr_hw_reset();
		break;

	case VPU_SET_MEM_ALLOC_MODE:
	case VPU_SET_MEM_ALLOC_MODE_KERNEL:
		if (command == VPU_SET_MEM_ALLOC_MODE_KERNEL) {
			(void)memcpy(&open_info, (OPENED_sINFO *)uarg.posi_data,
					sizeof(OPENED_sINFO));

			if (open_info.opened_cnt != 0U) {
				vmem_set_only_decode_mode
					((int)open_info.type);
			}
			ret = 0;
		} else {
			if (copy_from_user
				(&open_info, (OPENED_sINFO *)uarg.posi_data,
					sizeof(OPENED_sINFO)) > 0U) {
				ret = -EFAULT;
			} else {
				if (open_info.opened_cnt != 0U) {
					vmem_set_only_decode_mode
						((int)open_info.type);
				}
				ret = 0;
			}
		}
		break;

	case VPU_CHECK_CODEC_STATUS:
	case VPU_CHECK_CODEC_STATUS_KERNEL:
		if (command == VPU_CHECK_CODEC_STATUS_KERNEL) {
			(void)memcpy((int *)uarg.pi_data, vmgr_data.closed,
					sizeof(vmgr_data.closed));
		} else {
			if (copy_to_user
				((int *)uarg.pi_data, vmgr_data.closed,
					sizeof(vmgr_data.closed)) != 0U) {
				ret = -EFAULT;
			} else {
				ret = 0;
			}
		}
		break;

	case VPU_CHECK_INSTANCE_AVAILABLE:
	case VPU_CHECK_INSTANCE_AVAILABLE_KERNEL:
		{
			unsigned int nAvailable_Instance = 0;
			unsigned int type = (unsigned int) VPU_DEC;

			ret = 0;

			if (command == VPU_CHECK_INSTANCE_AVAILABLE_KERNEL) {
				if (NULL ==
					memcpy(&type, (int *)uarg.pi_data,
						sizeof(unsigned int))) {
					ret = -EFAULT;
				} else {
					if (copy_from_user
						(&type, (int *)uarg.pi_data,
							sizeof(unsigned int)) > 0U) {
						ret = -EFAULT;
					}
				}
			}

			if (ret == 0) {
				if (type < (unsigned int) VPU_ENC) {
					vdec_check_instance_available
						(&nAvailable_Instance);
				} else {
					venc_check_instance_available
						(&nAvailable_Instance);
				}

				if (command ==
					VPU_CHECK_INSTANCE_AVAILABLE_KERNEL) {
					(void)memcpy((unsigned int *)uarg.pui_data,
						&nAvailable_Instance,
						sizeof(unsigned int));
				} else {
					if (copy_to_user
						((unsigned int *)uarg.pui_data,
							&nAvailable_Instance,
							sizeof(unsigned int)) != 0U) {
						ret = -EFAULT;
					}
				}
			}
		}
		break;

	case VPU_GET_INSTANCE_IDX:
	case VPU_GET_INSTANCE_IDX_KERNEL:
		{
			INSTANCE_INFO iInst;
			vputype vpu_type;

			if (command == VPU_GET_INSTANCE_IDX_KERNEL) {
				(void)memcpy(&iInst, (int *)uarg.pi_data, sizeof(INSTANCE_INFO));
			} else {
				if (copy_from_user(&iInst, (int *)uarg.pi_data, sizeof(INSTANCE_INFO)) > 0U) {
					ret = -EFAULT;
				}
			}

			if (ret == 0) {
				if (iInst.type == (int)VPU_ENC) {
					venc_get_instance(&iInst.nInstance);
					if ((iInst.nInstance < (int)VPU_ENC) && (iInst.nInstance >= (int)VPU_MAX)) {
						V_DBG(VPU_DBG_ERROR, "invalid inst index: %d", iInst.nInstance);
						ret = -EINVAL;
					}
					vpu_type = vrm_get_enc_inst_to_vtype(iInst.nInstance);
				} else {
					vdec_get_instance(&iInst.nInstance);
					if ((iInst.nInstance < 0) && (iInst.nInstance >= (int)VPU_ENC)) {
						V_DBG(VPU_DBG_ERROR, "invalid inst index: %d", iInst.nInstance);
						ret = -EINVAL;
					}
					vpu_type = vrm_get_dec_inst_to_vtype(iInst.nInstance);
				}

				if (command == VPU_GET_INSTANCE_IDX_KERNEL) {
					(void)memcpy((int *)uarg.pi_data, &iInst, sizeof(INSTANCE_INFO));
				} else {
					if (copy_to_user((int *)uarg.pi_data, &iInst, sizeof(INSTANCE_INFO)) != 0U) {
						ret = -EFAULT;
					}
				}

				if (ret == 0) {

					if (p_vmgr != NULL) {
						p_vmgr->vpu_vtype = vpu_type;
						p_vmgr->vpu_inst_num = iInst.nInstance;
					}

					ret = vmgr_cmd_open(fname_file, vpu_type);
				}
			}
		}
		break;

	case VPU_CLEAR_INSTANCE_IDX:
	case VPU_CLEAR_INSTANCE_IDX_KERNEL:
		{
			INSTANCE_INFO iInst;
			vputype vpu_type;

			if (command == VPU_CLEAR_INSTANCE_IDX_KERNEL) {
				(void)memcpy(&iInst, (int *)uarg.pi_data,
					sizeof(INSTANCE_INFO));
			} else {
				if (copy_from_user
					(&iInst, (int *)uarg.pi_data,
					sizeof(INSTANCE_INFO)) > 0U) {
					ret = -EFAULT;
				}
			}

			if (ret == 0) {
				if (iInst.type == (int) VPU_ENC) {
					vpu_type = vrm_get_enc_inst_to_vtype(iInst.nInstance);
					venc_clear_instance(iInst.nInstance);
				} else {
					vpu_type = vrm_get_dec_inst_to_vtype(iInst.nInstance);
					vdec_clear_instance(iInst.nInstance);
				}

				(void)vmgr_cmd_release(fname_file, vpu_type);
				if (p_vmgr != NULL) {
					//reset
					p_vmgr->vpu_vtype = -1;
					p_vmgr->vpu_inst_num = -1;
				}
			}
		}
		break;

	case VPU_SET_RENDERED_FRAMEBUFFER:
	case VPU_SET_RENDERED_FRAMEBUFFER_KERNEL:
		if (command == VPU_SET_RENDERED_FRAMEBUFFER_KERNEL) {
			(void)memcpy(&vmgr_data.gsRender_fb_info,
				(void *)uarg.pv_data,
				sizeof(VDEC_RENDERED_BUFFER_t));
		} else {
			if (copy_from_user
				(&vmgr_data.gsRender_fb_info,
				(void *)uarg.pv_data,
				sizeof(VDEC_RENDERED_BUFFER_t)) > 0U) {
				ret = -EFAULT;
			} else {
				V_DBG(VPU_DBG_ERROR,
				"set rendered buffer info: 0x%x ~ 0x%x",
				vmgr_data.gsRender_fb_info.start_addr_phy,
				vmgr_data.gsRender_fb_info.size);
			}
		}
		break;

	case VPU_GET_RENDERED_FRAMEBUFFER:
	case VPU_GET_RENDERED_FRAMEBUFFER_KERNEL:
		if (command == VPU_GET_RENDERED_FRAMEBUFFER_KERNEL) {
			(void)memcpy((void *)uarg.pv_data,
				&vmgr_data.gsRender_fb_info,
				sizeof(VDEC_RENDERED_BUFFER_t));
		} else {
			if (copy_to_user
				((void *)uarg.pv_data,
				&vmgr_data.gsRender_fb_info,
				sizeof(VDEC_RENDERED_BUFFER_t)) != 0U) {
				ret = -EFAULT;
			} else {
				V_DBG(VPU_DBG_ERROR,
				"get rendered buffer info: 0x%x ~ 0x%x",
				vmgr_data.gsRender_fb_info.start_addr_phy,
				vmgr_data.gsRender_fb_info.size);
			}
		}
		break;

	case VPU_TRY_FORCE_CLOSE:
	case VPU_TRY_FORCE_CLOSE_KERNEL:
		if (!vmgr_data.bVpu_already_proc_force_closed) {
			vmgr_data.external_proc = 1;
			(void)vmgr_external_all_close(200);
			vmgr_data.external_proc = 0;
			vmgr_data.bVpu_already_proc_force_closed = (bool) true;
		}
		break;

	case VPU_TRY_CLK_RESTORE:
	case VPU_TRY_CLK_RESTORE_KERNEL:
		vmgr_restore_clock(0,
				vmgr_get_alive());
		break;

	case VPU_TRY_HANGUP_RELEASE:
		if (hangup_rel_count >= UINT_MAX) { //FIXME : need to set valid check number.
			V_DBG(VPU_DBG_ERROR,
				"hangup_rel_count is already MAX count, can't increase.");
		} else {
			hangup_rel_count++;
			V_DBG(VPU_DBG_CLOSE,
				" vpu ===> VPU_TRY_HANGUP_RELEASE %d'th",
				hangup_rel_count);
		}
		break;

	default:
		V_DBG(VPU_DBG_ERROR, "Unsupported ioctl[%d]!!!", cmd);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static long vmgr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	long ret;
	mutex_lock(&vmgr_data.comm_data.io_mutex);
	ret = vmgr_ioctl_l(filp, cmd, arg);
	mutex_unlock(&vmgr_data.comm_data.io_mutex);
	return ret;
}

#ifdef CONFIG_COMPAT
static long vmgr_compat_ioctl(struct file *filep, unsigned int cmd,
				   unsigned long arg)
{
	return vmgr_ioctl(filep, cmd, (unsigned long)compat_ptr(arg));
}
#endif

static irqreturn_t vmgr_isr_handler(int irq, void *dev_id)
{
	VPU_UNUSED_PARAMETER(irq);
	VPU_UNUSED_PARAMETER(dev_id);

	atomic_inc(&vmgr_data.oper_intr);

	wake_up_interruptible(&vmgr_data.oper_wq);

	return (irqreturn_t)IRQ_HANDLED;
}

static int vmgr_open(struct inode *pinode, struct file *filp)
{
	struct vmgr_t *p_vmgr;

	VPU_UNUSED_PARAMETER(pinode);

	// Allocate memory for vmgr_t
	p_vmgr = kmalloc(sizeof(struct vmgr_t), GFP_KERNEL);
	if (p_vmgr == NULL) {
		pr_err("Failed to allocate memory for vmgr_t\n");
		return -ENOMEM;
	}

	// Initialize vmgr members if needed
	p_vmgr->vpu_vtype = -1; // not ready
	p_vmgr->vpu_inst_num = -1;

	// Assign vmgr to private_data
	filp->private_data = p_vmgr;

	V_DBG(VPU_DBG_SEQUENCE, "alloc vmgr %x", p_vmgr);

	return 0;
}

static int vmgr_release(struct inode *pinode, struct file *filp)
{
	struct vmgr_t *p_vmgr = filp->private_data;

	VPU_UNUSED_PARAMETER(pinode);

	// Free the allocated memory
	if (p_vmgr != NULL) {
		if (p_vmgr->vpu_vtype != -1) {

			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
				"[%s][%d] force clear and release ...",
				vpu_vputype_to_string(p_vmgr->vpu_vtype), p_vmgr->vpu_inst_num);
		#if 0
			vrm_clear_instance_by_vputype(p_vmgr->vpu_vtype);
		#else
			if (p_vmgr->vpu_vtype < (int)VPU_ENC) {
				vdec_clear_instance(p_vmgr->vpu_inst_num);
			} else {
				venc_clear_instance(p_vmgr->vpu_inst_num);
			}
		#endif
			mutex_lock(&vmgr_data.comm_data.file_mutex);
			(void)vmgr_cmd_release(fname_file, p_vmgr->vpu_vtype);
			mutex_unlock(&vmgr_data.comm_data.file_mutex);

			//reset
			p_vmgr->vpu_vtype = -1;
			p_vmgr->vpu_inst_num = -1;
		}

		V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG "free vmgr %x", p_vmgr);
		kfree(p_vmgr);
		filp->private_data = NULL;
	}

	return 0;
}

struct VpuList *vmgr_list_manager(struct VpuList *args, unsigned int cmd)
{
	int isError = 0;
	struct VpuList *ret = NULL;
	struct VpuList *oper_data = (struct VpuList *) args;
	bool should_wake_up = false; // Variable indicating whether wake_up_interruptible should be called

	if (oper_data == NULL) {
		if ((cmd == (unsigned int)LIST_ADD) || (cmd == (unsigned int)LIST_DEL)) {
			V_DBG(VPU_DBG_SEQUENCE, "Data is null, cmd=%d", cmd);
			isError = 1;
		}
	}

	if (isError != 1) {
		if (cmd == (unsigned int)LIST_ADD) {
			*oper_data->vpu_result = RET0;
		}

		mutex_lock(&vmgr_data.comm_data.list_mutex);
		{
			switch ((int)cmd) {
			case (int)LIST_ADD:
				if (vmgr_add_element_to_list(oper_data, &vmgr_data.comm_data.main_list)) {
					*oper_data->vpu_result |= RET1;
					if (vmgr_data.cmd_queued > INT_MAX) {
						V_DBG(VPU_DBG_SEQUENCE, "Cmd queued is already FULL");
					} else {
						vmgr_data.cmd_queued++;
					}

					if (vmgr_data.comm_data.thread_intr > INT_MAX) {
						V_DBG(VPU_DBG_SEQUENCE, "Comm data thread interrupt count is already NULL");
					} else {
						vmgr_data.comm_data.thread_intr++;
					}
					// Set should_wake_up to true to indicate that wake_up_interruptible needs to be called
					should_wake_up = true;
				}
				break;
			case (int)LIST_DEL:
				if (vmgr_data.cmd_queued == 0U) {
					V_DBG(VPU_DBG_SEQUENCE, "No commands queued for deletion");
					break;
				}
				if (vmgr_remove_element_from_list(oper_data, &vmgr_data.comm_data.main_list)) {
					vmgr_data.cmd_queued--;
				}
				break;
			case LIST_IS_EMPTY:
				ret = vmgr_is_list_empty(&vmgr_data.comm_data.main_list);
				break;
			case (int)LIST_GET_ENTRY:
				ret = vmgr_get_first_list_entry(&vmgr_data.comm_data.main_list);
				break;
			default:
				/* Nothing to do */
				break;
			}
		}
		mutex_unlock(&vmgr_data.comm_data.list_mutex);

		if (should_wake_up && (cmd == (unsigned int)LIST_ADD)) {
			wake_up_interruptible(&vmgr_data.comm_data.thread_wq);
		}
	}

	return ret;
}

static void vmgr_set_pending_escapable_vpulist(struct VpuList *oper_data)
{
	int dbg_cmd;

	// Found an interlaced pair. Extract relevant information.
	int stream_size = -1;
	if (oper_data->cmd_type == VPU_DEC_DECODE) {
		VDEC_DECODE_t *arg = (VDEC_DECODE_t *)oper_data->args;
		stream_size = arg->gsVpuDecInput.m_iBitstreamDataSize;
	}

	vmgr_data.vpu_ctrl.last_timestamp = jiffies; // Update the timestamp when a list item is fetched

	// Save the last stream for error resilience scheme
	if ((vmgr_data.vpu_ctrl.last_vlist[vmgr_data.vpu_ctrl.pending_vtype] == NULL) && (oper_data->args != NULL)) {
		struct VpuList *p_vpulist = kmalloc(sizeof(struct VpuList), GFP_KERNEL);
		dbg_cmd = VPU_DBG_SEQUENCE;

		if (p_vpulist != NULL) {
			vmgr_data.vpu_ctrl.last_vlist[vmgr_data.vpu_ctrl.pending_vtype] = p_vpulist;
			memcpy(p_vpulist, oper_data, sizeof(struct VpuList));

			p_vpulist->args = (VDEC_DECODE_t *)kmalloc(sizeof(VDEC_DECODE_t), GFP_KERNEL);
			if (p_vpulist->args != NULL) {
				memcpy(p_vpulist->args, oper_data->args, sizeof(VDEC_DECODE_t));

				stream_size = ((VDEC_DECODE_t *)(oper_data->args))->gsVpuDecInput.m_iBitstreamDataSize;
			}

			if (p_vpulist->args == NULL) {
				(void)pr_err("Failed to allocate memory for VDEC_DECODE_t");
			} else {
				V_DBG(dbg_cmd, VLOG_TAG "[%s] Save the last stream for error resilience scheme. size=%d",
						vpu_vputype_to_string(vmgr_data.vpu_ctrl.pending_vtype),
						stream_size);
			}

		} else {
			(void)pr_err("Failed to allocate memory for struct VpuList");
		}
	} else {
		dbg_cmd = VPU_DBG_ILV_INFO;
	}

	V_DBG(dbg_cmd, VLOG_TAG "[%s] found an interlaced pair => handle=%x, vpu_type=%d, oper_data=0x%x, size=%d, time:%lu",
			vpu_vputype_to_string(vmgr_data.vpu_ctrl.pending_vtype), oper_data->handle,
			vmgr_data.vpu_ctrl.pending_vtype, oper_data, stream_size, vmgr_data.vpu_ctrl.last_timestamp);

	// Reset pending status.
	vmgr_data.vpu_ctrl.pending_vtype = -1;
	vmgr_data.vpu_ctrl.avoid_pending = 0;
}

#if 0 // original
static struct VpuList *vmgr_get_pending_escapable_vpulist(void)
{
	// Check if 200ms has elapsed since the last list item was fetched
	unsigned long jtimeout;
	unsigned long timestamp_out;
	unsigned long vmgr_jiffies = jiffies;
	unsigned int vmgr_timeout_msec;
	unsigned int timeout_msec;
	unsigned int time_diff;
	unsigned int time_coefficient = 5;

	struct VpuList *oper_data = NULL;

	vmgr_timeout_msec = vmgr_get_interrupt_timeout_msec(gs_timeout_msec);
	if (vmgr_timeout_msec < (ULONG_MAX / time_coefficient)) {
		if (vmgr_data.vpu_ctrl.last_timestamp > vmgr_jiffies) {
			time_diff = vmgr_jiffies - vmgr_data.vpu_ctrl.last_timestamp;
			timeout_msec = vmgr_timeout_msec * time_coefficient; //200 * 5
			jtimeout = msecs_to_jiffies(timeout_msec);
			if (time_diff > jtimeout) {
				if (vmgr_data.vpu_ctrl.last_timestamp < ULONG_MAX - jtimeout) {
					timestamp_out = vmgr_data.vpu_ctrl.last_timestamp + jtimeout;
					if (timestamp_out >= jiffies) {
						if (time_after(jiffies, (timestamp_out))) {
							// If pending time exceeds a threshold, attempt error resilience scheme.
							// Perform desired action when no new list item has been fetched for more than 200ms

							vmgr_data.vpu_ctrl.last_timestamp = vmgr_jiffies; // Update the timestamp when a list item is fetched

							// Try to decode with zero input as part of the error resilience scheme.
							oper_data = vmgr_data.vpu_ctrl.last_vlist[vmgr_data.vpu_ctrl.pending_vtype];
							if (oper_data->args != NULL) {
								VDEC_DECODE_t *arg = (VDEC_DECODE_t *)oper_data->args;
								int stream_size = arg->gsVpuDecInput.m_iBitstreamDataSize;

								V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
										"[%s][timeout:%lu msec] Error resilience scheme to avoid pending status - decode with zero input, "
										"total=%d, handle=0x%x, args=0x%x, cmd=%d, size=%d",
										vpu_vputype_to_string(vmgr_data.vpu_ctrl.pending_vtype), timeout_msec,
										vmgr_data.vpu_ctrl.interlaced_video_total, oper_data->handle, oper_data->args,
										oper_data->cmd_type, stream_size);

								// Signal
								vmgr_data.vpu_ctrl.avoid_pending = 1;
								// Reset pending status.
								vmgr_data.vpu_ctrl.pending_vtype = -1;
							}
						} else {
							err_vpu("timeout limit is overflowed");
						}
					} else {
						err_vpu("timeout limit is overflowed");
					}
				} else {
					err_vpu("timestamp_out is overflowed");
				}
			} else {
				err_vpu("time_diff is overflowed");
			}
		} else {
			err_vpu("jtimeout is overflowed");
		}
	}

	return oper_data;
}
#else // modified (alternative to reduce excessive indentation)
static struct VpuList *vmgr_get_pending_escapable_vpulist(void)
{
	// Check if 200ms has elapsed since the last list item was fetched
	unsigned long jtimeout = 0ul;
	unsigned long timestamp_out = 0ul;
	unsigned long vmgr_jiffies = jiffies;
	unsigned int vmgr_timeout_msec = 0u;
	unsigned int timeout_msec = 0u;
	unsigned int time_diff = 0u;
	unsigned int time_coefficient = 5u;

	struct VpuList *oper_data = NULL;

	do {
		vmgr_timeout_msec = vmgr_get_interrupt_timeout_msec(gs_timeout_msec);
		if (vmgr_timeout_msec >= (ULONG_MAX / time_coefficient)) {
			err_vpu("vmgr_timeout_msec is overflowed");
			break;
		}

		if (vmgr_data.vpu_ctrl.last_timestamp >= vmgr_jiffies) {
			err_vpu("jtimeout is overflowed");
			break;
		}

		time_diff = vmgr_jiffies - vmgr_data.vpu_ctrl.last_timestamp;
		timeout_msec = vmgr_timeout_msec * time_coefficient; //200 * 5
		jtimeout = msecs_to_jiffies(timeout_msec);

		if (time_diff <= jtimeout) {
			dprintk_vpu("still not timed out (time_diff)");
			break;
		}

		if (vmgr_data.vpu_ctrl.last_timestamp >= (ULONG_MAX - jtimeout)) {
			err_vpu("timestamp_out is overflowed");
			break;
		}

		timestamp_out = vmgr_data.vpu_ctrl.last_timestamp + jtimeout;
		if (timestamp_out < jiffies) {
			dprintk_vpu("still not timed out (timestamp_out)");
			break;
		}

		if (time_after(jiffies, timestamp_out) == 0) {
			dprintk_vpu("still not timed out (time_after)");
			break;
		}

		// If pending time exceeds a threshold, attempt error resilience scheme.
		// Perform desired action when no new list item has been fetched for more than 200ms

		// Update the timestamp when a list item is fetched
		vmgr_data.vpu_ctrl.last_timestamp = vmgr_jiffies;

		// Try to decode with zero input as part of the error resilience scheme.
		oper_data = vmgr_data.vpu_ctrl.last_vlist[vmgr_data.vpu_ctrl.pending_vtype];
		if (oper_data->args != NULL) {
			VDEC_DECODE_t *arg = (VDEC_DECODE_t *)oper_data->args;
			int stream_size = arg->gsVpuDecInput.m_iBitstreamDataSize;

			V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
					"[%s][timeout:%lu msec] Error resilience scheme to "
					"avoid pending status - decode with zero input, "
					"total=%d, handle=0x%x, args=0x%x, cmd=%d, size=%d",
					vpu_vputype_to_string(vmgr_data.vpu_ctrl.pending_vtype),
					timeout_msec, vmgr_data.vpu_ctrl.interlaced_video_total,
					oper_data->handle, oper_data->args,
					oper_data->cmd_type, stream_size);

			// Signal
			vmgr_data.vpu_ctrl.avoid_pending = 1;
			// Reset pending status.
			vmgr_data.vpu_ctrl.pending_vtype = -1;
		}
	} while (0);

	return oper_data;
}
#endif

static void vmgr_v2_set_pending_escapable_vpulist(struct VpuList *oper_data)
{
	/* create last vlist to escape vpu pending status */
	if (vmgr_data.vpu_ctrl.last_vlist[vmgr_data.vpu_ctrl.pending_vtype] == NULL) {
		struct VpuList *p_vpulist = kmalloc(sizeof(struct VpuList), GFP_KERNEL);
		if (p_vpulist != NULL) {
			vmgr_data.vpu_ctrl.last_vlist[vmgr_data.vpu_ctrl.pending_vtype] = p_vpulist;
			memcpy(p_vpulist, oper_data, sizeof(struct VpuList));

			(void)pr_info("[%s] create vpu list: %p",
						vpu_vputype_to_string(vmgr_data.vpu_ctrl.pending_vtype), p_vpulist);
			p_vpulist->args = NULL;
		} else {
			(void)pr_err("[%s] Failed to allocate memory for struct VpuList",
						vpu_vputype_to_string(vmgr_data.vpu_ctrl.pending_vtype));
		}
	}

	/* reset variables to check pending status */
	vmgr_data.vpu_ctrl.pending_vtype = -1;
	vmgr_data.vpu_ctrl.avoid_pending = 0;
	vmgr_data.vpu_ctrl.last_timestamp = 0;
}

static struct VpuList *vmgr_v2_get_pending_escapable_vpulist(void)
{
	struct VpuList *oper_data = NULL;

	unsigned int vmgr_timeout_msec = vmgr_get_interrupt_timeout_msec(gs_timeout_msec);
	unsigned int time_coefficient = 3;
	unsigned int timeout_msec = vmgr_timeout_msec * time_coefficient;

	unsigned int time_diff = ((jiffies < vmgr_data.vpu_ctrl.last_timestamp) ||
			(vmgr_data.vpu_ctrl.last_timestamp == 0)) ? 0u
		: jiffies - vmgr_data.vpu_ctrl.last_timestamp;

	unsigned long jtimeout = msecs_to_jiffies(timeout_msec);
	bool timed_out = (time_diff > jtimeout);
	bool timed_off = (vmgr_v2_find_urgent_element_with_vpu_type(
				&vmgr_data.comm_data.main_list, vmgr_data.vpu_ctrl.pending_vtype) != NULL);

	// If pending time exceeds a 35 ms threshold, attempt error resilience scheme.
	if (timed_out || timed_off) {
		// Update the timestamp when a list item is fetched
		vmgr_data.vpu_ctrl.last_timestamp = 0;

		oper_data = vmgr_data.vpu_ctrl.last_vlist[vmgr_data.vpu_ctrl.pending_vtype];

		if (oper_data != NULL) {
			struct vpu_decoder_data *vdata;
			struct v2hw_io_delay *dio;
			size_t pop_index;
			struct v2hw_flex_io *fli;
			bool found = false;

			vdata = (struct vpu_decoder_data *)
				vmgr_data.vpu_ctrl.vdata[vmgr_data.vpu_ctrl.pending_vtype];
			dio = vdata->flx_io_delay;

			/* update dio input state: delay -> empty */
			for (pop_index = 0u; pop_index < 2u; pop_index++) {
				if (dio->ip_state[pop_index] == V2_DIO_DELAY) {
					dio->ip_state[pop_index] = V2_DIO_EMPTY;
					found = true;
					break;
				}
			}

			if (found) {
				size_t flx_index = V2D_FINDEX(V2D_IP_DEC_FRMDATA + pop_index);
				fli = vdata->flx_list[flx_index];
				fli->v1_strt = (uint64_t)((uintptr_t)vdata);

				oper_data->args = (void *)fli;

				(void)pr_info("[%s] decode pending field (ip:%zu, op:%d, to: %d, fa: %d)",
						vpu_vputype_to_string(vmgr_data.vpu_ctrl.pending_vtype),
						pop_index, dio->op_state, timed_out, timed_off);

				/* update dio output state: empty -> ready */
				dio->op_state = V2_DIO_READY;

				// Signal
				vmgr_data.vpu_ctrl.avoid_pending = 1;
				// Reset pending status.
				vmgr_data.vpu_ctrl.pending_vtype = -1;
			} else {
				(void)pr_err("[%s] not found delayed cq (op state:%d, to: %d, fa: %d)",
						vpu_vputype_to_string(vmgr_data.vpu_ctrl.pending_vtype),
						dio->op_state, timed_out, timed_off);

				vmgr_data.vpu_ctrl.pending_vtype = -1;
				vmgr_data.vpu_ctrl.avoid_pending = 0;
				vmgr_data.vpu_ctrl.last_timestamp = 0;

				oper_data = vmgr_list_manager(NULL, (unsigned int)LIST_GET_ENTRY);
			}
		}
	} else {
		if (jtimeout - time_diff < 5) {
			(void)pr_info("pending elapsed: %ld ms <= %ld ms",
					msecs_to_jiffies(time_diff), msecs_to_jiffies(jtimeout));
		}
	}

	return oper_data;
}

static int vmgr_operation(void)
{
	int oper_finished;
	struct VpuList *oper_data = NULL;
	bool awake_pending = false;

	int debug_arg1, debug_arg2;

	while ((vmgr_list_manager(NULL, (unsigned int)LIST_IS_EMPTY) == NULL)) {

		vmgr_data.cmd_processing = 1;
		oper_finished = 1;
		V_DBG(VPU_DBG_CMD, "not empty cmd queued(%d)", vmgr_data.cmd_queued);

		mutex_lock(&vmgr_data.comm_data.process_mutex);

		if ((vmgr_data.vpu_ctrl.pending_vtype >= 0) && (vmgr_data.vpu_ctrl.pending_vtype < VPU_ENC)) {
			// Check if there's a pending operation with the specified VPU type and decode mode.
			oper_data = vmgr_find_dec_element_with_vpu_type(
					&vmgr_data.comm_data.main_list, vmgr_data.vpu_ctrl.pending_vtype);

			if (oper_data != NULL) {
				if (vmgr_data.vpu_ctrl.isFlexible[vmgr_data.vpu_ctrl.pending_vtype]) {
					vmgr_v2_set_pending_escapable_vpulist(oper_data);
				} else {
					vmgr_set_pending_escapable_vpulist(oper_data);
				}
			} else {
				if (vmgr_data.vpu_ctrl.last_vlist[vmgr_data.vpu_ctrl.pending_vtype] != NULL) {
					if (vmgr_data.vpu_ctrl.isFlexible[vmgr_data.vpu_ctrl.pending_vtype]) {
						oper_data = vmgr_v2_get_pending_escapable_vpulist();
					} else {
						oper_data = vmgr_get_pending_escapable_vpulist();
					}

					if (vmgr_data.vpu_ctrl.avoid_pending == 1) {
						awake_pending = true;
					}
				} else {
					(void)pr_err("%s: vlist for pending field is empty !!",
							vpu_vputype_to_string(vmgr_data.vpu_ctrl.pending_vtype));
				}
			}
		} else {
			oper_data = vmgr_list_manager(NULL, (unsigned int)LIST_GET_ENTRY);
			if (awake_pending == true) {
				(void)pr_err("%s: awake_pending is enabled unexpectedly !!",
							 vpu_vputype_to_string(oper_data->type));
				awake_pending = false;
			}
		}

		if (oper_data == NULL) {
			if ((vmgr_data.vpu_ctrl.pending_vtype >= 0) &&
				(vmgr_data.vpu_ctrl.pending_vtype < VPU_ENC)) {
				V_DBG(VPU_DBG_THREAD, VLOG_TAG
					"PENDING STATUS::: %s",
					vpu_vputype_to_string(vmgr_data.vpu_ctrl.pending_vtype));
			} else {
				V_DBG(VPU_DBG_SEQUENCE, "data is null");
			}

			vmgr_data.cmd_processing = 0;
			mutex_unlock(&vmgr_data.comm_data.process_mutex);

			return 0;
		}

		*oper_data->vpu_result |= RET2;
		V_DBG(VPU_DBG_CMD,
				"[%d] :: cmd = 0x%x, cmd_queued(%d)",
				oper_data->type, oper_data->cmd_type, vmgr_data.cmd_queued);

		if (oper_data->type < (unsigned int)VPU_MAX) {

			*oper_data->vpu_result |= RET3;
			*oper_data->vpu_result =
					vmgr_process((vputype)(oper_data->type), oper_data->cmd_type,
							oper_data->handle, oper_data->args);

			oper_finished = 1;
			if (*oper_data->vpu_result != RETCODE_SUCCESS) {

				if ((*oper_data->vpu_result != RETCODE_INSUFFICIENT_BITSTREAM)
					&& (*oper_data->vpu_result != RETCODE_INSUFFICIENT_BITSTREAM_BUF)) {

					V_DBG(VPU_DBG_SEQUENCE,
						"[%s][CMD:0x%X(%d)] vmgr_out[0x%x] :: type = %d, "
						"handle = 0x%x, cmd = 0x%x, frame_len %d",
						vpu_vputype_to_string(oper_data->type),
						oper_data->cmd_type, oper_data->cmd_type,
						*oper_data->vpu_result, oper_data->type, oper_data->handle,
						oper_data->cmd_type, vmgr_data.szFrame_Len);
				}

				if (*oper_data->vpu_result == RETCODE_CODEC_EXIT) {
					debug_arg1 = vmgr_count_allocated_nodes_with_vpu_type(
							&vmgr_data.comm_data.main_list, oper_data->type);
					debug_arg2 = vmgr_count_allocated_nodes(&vmgr_data.comm_data.main_list);
					V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
						"[%s][CMD:0x%X(%d)] RETCODE_CODEC_EXIT returned => VPU is unresponsive "
						"...active(vpu:%d|vmgr:%d), cmd_queued(%d/%d)",
						vpu_vputype_to_string(oper_data->type),
						oper_data->cmd_type,
						oper_data->cmd_type,
						count_active_instances_in_vpu(),
						vmgr_get_alive(),
						debug_arg1,
						debug_arg2);

					vmgr_codec_exit((vputype)(oper_data->type));
					//vmgr_data.timeout_vpu_type = oper_data->type;
					//vmgr_data.timeout_exit = 1;
				}
			}
		} else {
			V_DBG(VPU_DBG_ERROR,
					"missed info or unknown command => vpu_type = 0x%x, cmd = 0x%x",
					oper_data->type, oper_data->cmd_type);

			*oper_data->vpu_result = RETCODE_FAILURE;
			oper_finished = 0;
		}

		mutex_unlock(&vmgr_data.comm_data.process_mutex);

		if (oper_finished != 0) {

			if (oper_data->comm_data != NULL) {

				if (vmgr_get_alive() != 0) {
					if (vmgr_data.timeout_exit > 0) {
						if ((oper_data->cmd_type == VPU_DEC_CLOSE) ||
							(oper_data->cmd_type == VPU_DEC_CLOSE_KERNEL) ||
							(oper_data->cmd_type == V2D_IP_DRV_RST)) {
								//[Note] The log message is used to verify whether the application restarts
								//	after the completion of the 'VPU_DEC_CLOSE' command.
								//	It helps to ensure that the application follows the correct sequence of operations
								//	and initiates the restart process accordingly.
							debug_arg1 = vmgr_count_allocated_nodes_with_vpu_type(
										&vmgr_data.comm_data.main_list, oper_data->type);
							debug_arg2 = vmgr_count_allocated_nodes(&vmgr_data.comm_data.main_list);
							V_DBG(VPU_DBG_SEQUENCE, VLOG_TAG
								" [%s][CMD:0x%X(%d)] The completion of the 'VPU_DEC_CLOSE' command "
								"...active(vpu:%d|vmgr:%d), cmd_queued(%d/%d)",
								vpu_vputype_to_string(oper_data->type),
								oper_data->cmd_type,
								oper_data->cmd_type,
								count_active_instances_in_vpu(),
								vmgr_get_alive(),
								debug_arg1,
								debug_arg2);
						}
					}

					if (awake_pending == false) {
						oper_data->comm_data->count++;

						if (oper_data->comm_data->count != 1U) {
							V_DBG(VPU_DBG_CMD,
									"poll wakeup count = %d :: type(0x%x) cmd(0x%x)",
									oper_data->comm_data->count, oper_data->type, oper_data->cmd_type);
						}

						wake_up_interruptible(&oper_data->comm_data->wq);
					}

				} else {
					V_DBG(VPU_DBG_SEQUENCE,
						"Error: abnormal exception or external command was processed!! 0x%p - %d",
						oper_data->comm_data, vmgr_get_alive());
				}
			} else { //oper_data->comm_data == NULL
				V_DBG(VPU_DBG_SEQUENCE,
					"VpuList(0x%x)->vpu_dec_data_t(NULL), active(vpu:%d|vmgr:%d)",
					oper_data, count_active_instances_in_vpu(), vmgr_get_alive());
			}

			if (awake_pending) {
				awake_pending = false;
			} else {
				(void)vmgr_list_manager(oper_data, (unsigned int)LIST_DEL);
			}
			vmgr_data.cmd_processing = 0;

		} else { //oper_finished == 0
			V_DBG(VPU_DBG_SEQUENCE,
				"Error: exception 0x%p - %d",
				oper_data->comm_data, vmgr_get_alive());
		}
	}

	return 0;
}

static int vmgr_thread(void *kthread)
{
	unsigned long jtimeout;

	VPU_UNUSED_PARAMETER(kthread);

	V_DBG(VPU_DBG_THREAD, "enter");

	jtimeout = msecs_to_jiffies(50);
	if (jtimeout > LONG_MAX) {
		jtimeout = LONG_MAX;
	}

	do {
		if (vmgr_list_manager(NULL, (unsigned int)LIST_IS_EMPTY) != NULL) {
			vmgr_data.cmd_processing = 0;

			(void) wait_event_interruptible_timeout(
				vmgr_data.comm_data.thread_wq,
				vmgr_data.comm_data.thread_intr > 0,
				(long)jtimeout);

			vmgr_data.comm_data.thread_intr = 0;
		} else {
			if ((vmgr_get_alive() != 0)
				|| (vmgr_data.external_proc != 0U)) {
				(void)vmgr_operation();
			}
			#if 0
			 else {
				struct VpuList *oper_data = NULL;
				V_DBG(VPU_DBG_SEQUENCE, "DEL for empty");
				oper_data = vmgr_list_manager(NULL, (unsigned int)LIST_GET_ENTRY);
				if (oper_data > 0) {
					(void)vmgr_list_manager(oper_data, (unsigned int)LIST_DEL);
				}
			}
			#endif
		}
	} while (!kthread_should_stop());

	V_DBG(VPU_DBG_THREAD, "finish");

	return 0;
}

static int vmgr_mmap(struct file *filp, struct vm_area_struct *vma)
{
	unsigned long current_vm_range = (vma->vm_end >= vma->vm_start) ?
		(vma->vm_end - vma->vm_start) : 0U;

#if defined(CONFIG_TCC_MEM)
	if (range_is_allowed(vma->vm_pgoff, current_vm_range) < 0) {
		V_DBG(VPU_DBG_ERROR, "this address is not allowed");
		return -EAGAIN;
	}
#endif

	VPU_UNUSED_PARAMETER(filp);

	vma->vm_page_prot = vmem_get_pgprot(vma->vm_page_prot, vma->vm_pgoff);
	if (remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, current_vm_range, vma->vm_page_prot) > 0) {
		V_DBG(VPU_DBG_ERROR, "remap_pfn_range failed");
		return -EAGAIN;
	}

	vma->vm_ops = NULL;
	vetc_vm_flags_set(vma, VM_IO | VM_DONTEXPAND | VM_PFNMAP);

	return 0;
}

static const struct file_operations vmgr_fops = {
	.open = vmgr_open,
	.release = vmgr_release,
	.mmap = vmgr_mmap,
	.unlocked_ioctl = vmgr_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = vmgr_compat_ioctl,
#endif
};

static struct miscdevice vmgr_misc_device = {
	MISC_DYNAMIC_MINOR,
	MGR_NAME,
	&vmgr_fops,
};

int vmgr_probe(struct platform_device *pdev)
{
	int ret;
	int type;
	unsigned long int_flags;
	struct resource *st_resource = NULL;
	void *tTmpPtr = NULL;

	if (pdev->dev.of_node == NULL) {
		return -ENODEV;
	}

	V_DBG(VPU_DBG_PROBE, "vmgr initializing!!");
	(void)memset(&vmgr_data, 0, sizeof(struct mgr_data_t));
	for (type = 0; type < (int)VPU_MAX; type++) {
		vmgr_data.closed[type] = VPU_CLOSED;
	}

	vmgr_init_variable();
	atomic_set(&vmgr_data.oper_intr, 0);

	ret = platform_get_irq(pdev, 0);
	if (ret >= 32) {
		vmgr_data.irq = (unsigned int)ret;
	} else {
		V_DBG(VPU_DBG_ERROR, "invalid irq number"); //FIXME : Is the irq vlaue always greater than 31? it is related to line number 2105
		return -1;	//FIXME : need to change to proper value
	}

	vmgr_data.nOpened_Count = 0;
	st_resource = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (st_resource == NULL) {
		dev_err(&pdev->dev, "missing phy memory resource");
		return -1;
	}
	st_resource->end += 1U;

	vmgr_data.base_addr =
		devm_ioremap(&pdev->dev, st_resource->start,
			(st_resource->end - st_resource->start));
	V_DBG(VPU_DBG_PROBE,
		"============> VPU base address [0x%x -> 0x%p], irq num [%d]",
		st_resource->start, vmgr_data.base_addr, (vmgr_data.irq - 32U));

	vmgr_get_clock(pdev->dev.of_node);
	vmgr_get_reset(pdev->dev.of_node);

	init_waitqueue_head(&vmgr_data.comm_data.thread_wq);
	init_waitqueue_head(&vmgr_data.oper_wq);

	mutex_init(&vmgr_data.comm_data.list_mutex);
	mutex_init(&(vmgr_data.comm_data.io_mutex));
	mutex_init(&(vmgr_data.comm_data.file_mutex));
	mutex_init(&(vmgr_data.comm_data.process_mutex));

	INIT_LIST_HEAD(&vmgr_data.comm_data.main_list);

	ret = vmem_config();
	if (ret < 0) {
		V_DBG(VPU_DBG_ERROR, "unable to configure memory for VPU!! %d",
			ret);
		return -ENOMEM;
	}

#if defined(USE_ACCESS_POINT)
	if (stVPUFunc == NULL) {
		ret = get_access_addr();
		if (ret != 0) {
			V_DBG(VPU_DBG_ERROR, "Getting for library access point failed!!");
			return RETCODE_FAILURE;
		}
	}
#endif

	vmgr_init_interrupt();
	int_flags = vmgr_get_int_flags();
	ret = vmgr_request_irq(vmgr_data.irq, vmgr_isr_handler, int_flags,
			MGR_NAME, &vmgr_data);
	if (ret > 0) {
		V_DBG(VPU_DBG_ERROR, "to aquire vpu-dec-irq");
	}

	vmgr_data.irq_reged = 1;

	kidle_task = kthread_run(vmgr_thread, NULL, "vVPU_th");
	VPU_CAST_PT(tTmpPtr, kidle_task);
	if (IS_ERR(tTmpPtr)) {
		V_DBG(VPU_DBG_ERROR, "unable to create thread!!");
		kidle_task = NULL;
		return -1;
	}
	V_DBG(VPU_DBG_PROBE, "success: thread created!!");

	vmgr_close_all(1);

	if (misc_register(&vmgr_misc_device) > 0) {
		V_DBG(VPU_DBG_ERROR, "VPU Manager: Couldn't register device.");
		return -EBUSY;
	}

	return 0;
}
EXPORT_SYMBOL(vmgr_probe);

VREMOVE_RET_TYPE vmgr_remove(struct platform_device *pdev)
{
	misc_deregister(&vmgr_misc_device);

	if (kidle_task != NULL) {
		(void)kthread_stop(kidle_task);
		kidle_task = NULL;
	}

	devm_iounmap(&pdev->dev, (void __iomem *)vmgr_data.base_addr);
	if (vmgr_data.irq_reged > 0U) {
		vmgr_free_irq(vmgr_data.irq, &vmgr_data);
		vmgr_data.irq_reged = 0;
	}

	vmgr_put_clock();
	vmgr_put_reset();
	vmem_deinit();

	//(void)pr_info("success :: thread stopped!!\n");

	VREMOVE_RETURN();
}
EXPORT_SYMBOL(vmgr_remove);

#if defined(CONFIG_PM)
int vmgr_suspend(struct platform_device *pdev, pm_message_t state)
{
	int i, open_count = 0;

	VPU_UNUSED_PARAMETER(state);

	if (vmgr_get_alive() != 0) {
		(void)pr_info(
		"\n vpu(%p): suspend In DEC(%d/%d/%d/%d/%d), ENC(%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d)\n", pdev,
		vmgr_data.closed[VPU_DEC], 		vmgr_data.closed[VPU_DEC_EXT],
		vmgr_data.closed[VPU_DEC_EXT2], vmgr_data.closed[VPU_DEC_EXT3],
		vmgr_data.closed[VPU_DEC_EXT4], vmgr_data.closed[VPU_ENC],
		vmgr_data.closed[VPU_ENC_EXT], vmgr_data.closed[VPU_ENC_EXT2],
		vmgr_data.closed[VPU_ENC_EXT3], vmgr_data.closed[VPU_ENC_EXT4],
		vmgr_data.closed[VPU_ENC_EXT5], vmgr_data.closed[VPU_ENC_EXT6],
		vmgr_data.closed[VPU_ENC_EXT7], vmgr_data.closed[VPU_ENC_EXT8],
		vmgr_data.closed[VPU_ENC_EXT9], vmgr_data.closed[VPU_ENC_EXT10],
		vmgr_data.closed[VPU_ENC_EXT11], vmgr_data.closed[VPU_ENC_EXT2],
		vmgr_data.closed[VPU_ENC_EXT13], vmgr_data.closed[VPU_ENC_EXT4],
		vmgr_data.closed[VPU_ENC_EXT15]);

		(void)vmgr_external_all_close(200);

		open_count = vmgr_get_alive();

		for (i = 0; i < open_count; i++) {
			vmgr_disable_clock(0, 0);
		}

		(void)pr_info(
		"vpu(%p): suspend Out DEC(%d/%d/%d/%d/%d), ENC(%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d/%d)\n\n", pdev,
		vmgr_data.closed[VPU_DEC], vmgr_data.closed[VPU_DEC_EXT],
		vmgr_data.closed[VPU_DEC_EXT2], vmgr_data.closed[VPU_DEC_EXT3],
		vmgr_data.closed[VPU_DEC_EXT4], vmgr_data.closed[VPU_ENC],
		vmgr_data.closed[VPU_ENC_EXT], vmgr_data.closed[VPU_ENC_EXT2],
		vmgr_data.closed[VPU_ENC_EXT3], vmgr_data.closed[VPU_ENC_EXT4],
		vmgr_data.closed[VPU_ENC_EXT5], vmgr_data.closed[VPU_ENC_EXT6],
		vmgr_data.closed[VPU_ENC_EXT7], vmgr_data.closed[VPU_ENC_EXT8],
		vmgr_data.closed[VPU_ENC_EXT9], vmgr_data.closed[VPU_ENC_EXT10],
		vmgr_data.closed[VPU_ENC_EXT11], vmgr_data.closed[VPU_ENC_EXT12],
		vmgr_data.closed[VPU_ENC_EXT13], vmgr_data.closed[VPU_ENC_EXT14],
		vmgr_data.closed[VPU_ENC_EXT15]);
	}

	return 0;
}
EXPORT_SYMBOL(vmgr_suspend);

int vmgr_resume(struct platform_device *pdev)
{
	int i, open_count = 0;

	if (vmgr_get_alive() != 0) {
		open_count = vmgr_get_alive();

		for (i = 0; i < open_count; i++) {
			vmgr_enable_clock(0, 0);
		}

		(void)pr_info("\nvpu(%p): resume\n\n", pdev);
	}

	return 0;
}
EXPORT_SYMBOL(vmgr_resume);
#endif

MODULE_IMPORT_NS(VFS_internal_I_am_really_a_filesystem_and_am_NOT_a_driver);
MODULE_SOFTDEP("pre: vpu_lib jpu_lib hevc_lib vpu_4k_d2_lib vpu_hevc_enc_lib");

MODULE_AUTHOR("Telechips.");
MODULE_DESCRIPTION("TCC VPU Manager");
MODULE_LICENSE("GPL");

#endif //#ifdef CONFIG_SUPPORT_TCC_VPU
