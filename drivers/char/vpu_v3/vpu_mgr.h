/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_MGR_H
#define VPU_MGR_H

#include "vpu_comm.h"
#include "vpu_mgr_sys.h"
#include "vpu_dbg_string.h"
#include "vpu_internal_type.h"

// This header must located after "vpu_comm.h".
#include <dt-bindings/pmap/common/vpu_mem_size.h>

typedef int (*vpu_process)(void *vpu_private, enum vpu_cmd_type cmd, vpu_cmd_t *cmd_info, vpu_drv_info_t *drv_info);
typedef irqreturn_t (*vpu_isr_handler)(int irq, void *vpu_private);
typedef int (*vpu_get_buffer_size)(void *vpu_private, enum vmgr_buffer_type buf_type, vpu_drv_info_t *drv_info);
typedef int (*vmgr_internal_handler)(void);

typedef struct vpu_accesspoint_t {
	unsigned int check_code1;
	tccfp_vpu_proc_t tccfp_vpu_dec;
	unsigned int check_code2;
	tccfp_vpu_proc_t tccfp_vpu_enc;
	unsigned int check_code3;
	tccfp_vpu_proc_t tccfp_vpu_dec_esc;
	tccfp_vpu_proc_t tccfp_vpu_dec_ext;
	unsigned int check_code4;
} vpu_accesspoint_t;

typedef struct cq_func_t {
	unsigned int (*vpu_get_id_with_reason)(void *priv, unsigned int *reason);
	unsigned int (*vpu_get_interrupt_status)(void *priv);
} cq_func_t;

#define MAX_SUPPORT_CODEC	(64)
typedef struct vpu_ip_cap_t {
	enum vpu_codec_id codec_id;
	const char *support_codec;
	const char *support_profile;
	const char *support_level;
	const unsigned int max_width;
	const unsigned int max_height;
	const unsigned int max_fps;
} vpu_ip_cap_t;

typedef struct vpu_ip_module_t {
	/**
	* @brief Enumeration for the VPU IP types.
	*
	* Each IP type
	*/
	enum vpu_ip_type ip_type;

	/**
	 * @brief Enumeration for the VPU command queue types.
	 *
	 * command queue support type Legacy or 2
	 */
	enum vpu_cq_type cq_type;

	/**
	 * @brief Command queue depth.
	 *
	 * Depth of the command queues
	 */
	unsigned int cq_depth;

	/**
	 * @brief Enumeration of VPU buffer operation modes.
	 *        There are two modes: ring buffer and linear buffer.
	 */
	enum vpu_bs_buffer_mode buffer_mode;

	/**
	 * @brief Internal handler timeout in milliseconds.
	 *
	 * Specifies the timeout in milliseconds for internal handler.
	 */
	int internal_timeout_ms;

	/**
	 * @brief enc_param_size: size of the structure for encoder parameters
	 *
	 * This variable represents the size of the structure used for encoder parameters.
	 * When used as a local variable, storing parameters on the stack can lead to stack-related issues,
	 * so this variable is used for allocating memory on the heap to store the parameters.
	 */
	unsigned int enc_param_size;

	/**
	 * @brief dec_param_size: size of the structure for decoder parameters
	 *
	 * This variable represents the size of the structure used for decoder parameters.
	 * When used as a local variable, storing parameters on the stack can lead to stack-related issues,
	 * so this variable is used for allocating memory on the heap to store the parameters.
	 */
	unsigned int dec_param_size;

	/**
	 * @brief Decoder capabilities for each supported codec.
	 */
	vpu_ip_cap_t dec_capa[MAX_SUPPORT_CODEC];

	/**
	 * @brief Encoder capabilities for each supported codec.
	 */
	vpu_ip_cap_t enc_capa[MAX_SUPPORT_CODEC];

	/**
	 * @brief Clock control for each VPU IP.
	 *
	 * Pointer to the clock control structure that manages the clock for each IP.
	 */
	vmgr_clock_t *clock_ctrl;

	/**
	 * @brief Internal handler for each VPU IP.
	 *
	 * Structure representing the internal handler for each VPU IP.
	 */
	vmgr_internal_handler internal_handler;

	/**
	 * @brief Encoder processing for the library of each IP.
	 */
	vpu_process proc_encode;

	/**
	 * @brief Decoder processing for the library of each IP.
	 */
	vpu_process proc_decode;

	/**
	 * @brief vpu_get_buffer_size: Function pointer for obtaining the buffer size for VPU
	 *
	 * This function pointer is used to obtain the buffer size when allocating physical memory for the decoder and encoder purposes.
	 * It is called to determine the required size of the buffer.
	 */
	vpu_get_buffer_size proc_get_buffer_size;

	/**
	 * @brief Interrupt service handler for each VPU IP.
	 *
	 * Structure representing the interrupt service handler for each VPU IP.
	 */
	vpu_isr_handler isr_handler;

	/**
	 * @brief Command queue support functions.
	 *
	 * Pointer to the structure containing command queue support functions.
	 */
	cq_func_t *cq_func;

	/**
	 * @brief Private data for each VPU IP.
	 *
	 * Pointer to each VPU IP's private data, if available.
	 */
	void *ip_private;

	/**
	 * @brief Pointer to the access point path for each VPU library.
	 *
	 * This variable holds the path to the access point for each VPU library.
	 * It contains the information needed to connect to each VPU library.
	 */
	const char *access_point_path;
} vpu_ip_module_t;

typedef struct vpu_mgr_t {
	/**
	 * @brief Mutex for restricting access via API from external entities to 'vmgr'.
	 *
	 * This mutex is used to control access to 'vmgr' when accessed via API to ensure
	 * thread-safe operations.
	 */
	struct mutex vmgr_mutex;

#if defined(ENABLE_CQ2)
	atomic_t cq_remaining; //from wave5_inform() in 4kd2
	atomic_t handler_intr; //from 4kd2

	atomic_t intrCount[VPU_OP_TYPE_MAX][VPU_DRV_ID_MAX]; //interrupt count from vpu_get_interrupt_status()
	int procCount[VPU_OP_TYPE_MAX][VPU_DRV_ID_MAX]; //to check to flush
#endif

	struct platform_device *pdev;

	//IRQ number and IP base
	unsigned int irq;
	void __iomem *base_addr;
	//int check_interrupt_detection; // why ?

	//there are two ways to access driver information in vpu mgr,
	//one is by accessing a static array using the driver's ID,
	//and the other is by using a linked-list to retrieve the driver info.
	vpu_drv_info_t *drv_info[VPU_OP_TYPE_MAX][VPU_DRV_ID_MAX];
	vpu_dllist_t *drv_list; //opened driver's drv_info list to retrive

	vmgr_comm_t comm_data;

	//each ip's internal handler
	atomic_t oper_intr;
	wait_queue_head_t oper_wq;

	//for polling thread
	atomic_t poll_intr;
	wait_queue_head_t poll_wq;

	atomic_t dev_opened;
	unsigned char irq_reged;
	bool external_proc;

	struct task_struct *kidle_task;
	struct task_struct *collect_task;

	vpu_accesspoint_t *access_point;
	vpu_ip_module_t *each_ip;

#if defined(ENABLE_VPU_FW_LOADING)
	codec_addr_t fw_addr;
#endif

	/**
	* @brief Accumulated value of width multiplied by height multiplied by fps.
	*
	* This variable stores the cumulative value obtained by multiplying the width,
	* height, and frames per second (fps) together. It is used to represent the
	* aggregated performance metric in terms of these factors.
	*/
	unsigned long accumulated_pixel_product;

#if defined(ENABLE_INTERLACE_DELAY_PROCESS)
	unsigned int pending_drv_id; //decode driver id or INVALID_DRV_ID
	unsigned int enable_avoid_pending; //enable or disable
	unsigned long field_processed_timestamp; //jiffies, the moment when one field is processed
#endif

} vpu_mgr_t;

int vmgr_accesspoint_check_addr_valid(vpu_accesspoint_t *accesspoint);

//check status
int vmgr_get_alive(vpu_mgr_t *mgr_ctx);

bool vmgr_get_opened(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id);

int vmgr_set_opened(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id, bool isOpened);

long vmgr_get_handle(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id);

int vmgr_set_handle(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id, long handle);


/**
 * @brief Add pixel product to accumulated value in the video manager context.
 *
 * This function adds the given pixel product value to an accumulated value
 * in the video manager context (`vpu_mgr_t`). The accumulated value represents
 * a running total of pixel products and is updated to reflect the addition
 * of the new pixel product.
 *
 * @param[in] mgr_ctx The video manager context.
 * @param[in] pixel_product The pixel product value to be added.
 *
 * @return The updated accumulated pixel product value after adding `pixel_product`.
 */
unsigned long vmgr_add_accumulated_pixelproduct(vpu_mgr_t *mgr_ctx, unsigned long pixel_product);

/**
 * @brief Subtract pixel product from accumulated value in the video manager context.
 *
 * This function subtracts the given pixel product value from an accumulated value
 * in the video manager context (`vpu_mgr_t`). The accumulated value represents
 * a running total of pixel products and is updated to reflect the subtraction
 * of the pixel product.
 *
 * @param[in,out] mgr_ctx The video manager context.
 * @param[in] pixel_product The pixel product value to be subtracted.
 *
 * @return The updated accumulated pixel product value after subtracting `pixel_product`.
 */
unsigned long vmgr_sub_accumulated_pixelproduct(vpu_mgr_t *mgr_ctx, unsigned long pixel_product);


/**
 * @brief Get the accumulated pixel product value from the video manager context.
 *
 * This function retrieves the accumulated pixel product value stored in the
 * video manager context (`vpu_mgr_t`). The accumulated value represents a
 * running total of pixel products that have been added over time.
 *
 * @param[in] mgr_ctx The video manager context.
 *
 * @return The accumulated pixel product value stored in the `mgr_ctx`.
 */
unsigned long vmgr_get_accumulated_pixelproduct(vpu_mgr_t *mgr_ctx);


//command related
vpu_cmd_t *vmgr_list_manager_alloc(vpu_mgr_t *mgr_ctx);

int vmgr_list_manager_add(vpu_mgr_t *mgr_ctx, vpu_cmd_t *args);

vpu_cmd_t *vmgr_list_manager_show_result(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id);

vpu_cmd_t *vmgr_list_manager_get_result(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id);

int vmgr_list_manager_result_remain(vpu_mgr_t *mgr_ctx, const enum vpu_op_type op_type, const unsigned int drv_id);

int vmgr_list_manager_add_pool(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd);

int vmgr_process_ex(vpu_mgr_t *mgr_ctx, vpu_cmd_t *cmd_list, const enum vpu_op_type op_type, const unsigned int drv_id, enum vpu_cmd_type cmd, int *result);

int vmgr_external_all_close(vpu_mgr_t *mgr_ctx, int wait_ms);


int vmgr_alloc_ip_parameter(vpu_drv_info_t *drv_info, unsigned int param_size);

int vmgr_release_ip_parameter(vpu_drv_info_t *drv_info);

void vmgr_decode_pre_flush(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info);

//register/deregister from dec/enc driver
int vmgr_register(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info);

int vmgr_deregister(vpu_mgr_t *mgr_ctx, vpu_drv_info_t *drv_info);


//each IP's module behavior
vpu_mgr_t *vmgr_alloc(vpu_ip_module_t *vpu_mgr);

void vmgr_free(vpu_mgr_t *mgr_ctx);

int vmgr_probe(vpu_mgr_t *mgr_ctx, struct platform_device *pdev, const char *mgr_name);

int vmgr_remove(vpu_mgr_t *mgr_ctx, struct platform_device *pdev);

#if defined(CONFIG_PM)
int vmgr_suspend(vpu_mgr_t *mgr_ctx, struct platform_device *pdev, pm_message_t state);

int vmgr_resume(vpu_mgr_t *mgr_ctx, struct platform_device *pdev);
#endif

#endif /*VPU_MGR_H*/
