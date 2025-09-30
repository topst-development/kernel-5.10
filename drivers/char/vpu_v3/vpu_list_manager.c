/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_list_manager.h"
#include "vpu_memtrace.h"
#include "vpu_dbg_string.h"
#include "vpu_dllist.h"
#include "vpu_etc.h"

void vmgr_list_flush(vpu_dllist_t *list)
{
	vpu_cmd_t *cmd = NULL;

	while (list->head != NULL) {
		cmd = (vpu_cmd_t *)vpu_dllist_remove_head(list);
		if (cmd != NULL) {
			if (cmd->args != NULL) {
				VPU_free(cmd->args);
				cmd->args = NULL;
			}

			vpu_dllist_destroy_node((vpu_dllist_node_t *)cmd);
		}
	}
}

void vmgr_list_flush_sync(vpu_dllist_t *list)
{
	mutex_lock(&list->mutex);

	vmgr_list_flush(list);

	mutex_unlock(&list->mutex);
}

static void vmgr_list_destroy_queue(vpu_dllist_t *queue)
{
	vmgr_list_flush(queue);
	VPU_free(queue);
}

void vmgr_list_init(vmgr_comm_t *commData)
{
	mutex_init(&commData->list_mutex); // mutex for command list
	init_waitqueue_head(&commData->thread_wq);

	commData->thread_intr = 0U;
	commData->cmd_queued = 0U;

	commData->cmd_pool = vpu_dllist_create("pool");
	commData->cmd_q = vpu_dllist_create("cmd");
}

void vmgr_list_deinit(vmgr_comm_t *commData)
{
	unsigned int ii;
	//int count;

	//count = vpu_dllist_get_count_sync(commData->cmd_pool);
	//V_DBG(VPU_DBG_INFO, "cmd_pool left count:%d", count);

	//count = vpu_dllist_get_count_sync(commData->cmd_q);
	//V_DBG(VPU_DBG_INFO, "cmd_q left count:%d", count);

	vmgr_list_destroy_queue(commData->cmd_pool);
	vmgr_list_destroy_queue(commData->cmd_q);

	for (ii = 0U; ii < VPU_DRV_ID_MAX; ii++) {
		if (commData->wait_q[VPU_OP_TYPE_DEC][ii] != NULL) {
			//count = vpu_dllist_get_count_sync(commData->wait_q[VPU_OP_TYPE_DEC][ii]);
			//V_DBG(VPU_DBG_INFO, "dec wait_q %d left count:%d", ii, count);

			vmgr_list_destroy_queue(commData->wait_q[VPU_OP_TYPE_DEC][ii]);
		}

		if (commData->wait_q[VPU_OP_TYPE_ENC][ii] != NULL) {
			//count = vpu_dllist_get_count_sync(commData->wait_q[VPU_OP_TYPE_ENC][ii]);
			//V_DBG(VPU_DBG_INFO, "enc wait_q %d left count:%d", ii, count);

			vmgr_list_destroy_queue(commData->wait_q[VPU_OP_TYPE_ENC][ii]);
		}
	}

	for (ii = 0U; ii < VPU_DRV_ID_MAX; ii++) {
		if (commData->result_q[VPU_OP_TYPE_DEC][ii] != NULL) {
			//count = vpu_dllist_get_count_sync(commData->result_q[VPU_OP_TYPE_DEC][ii]);
			//V_DBG(VPU_DBG_INFO, "dec result_q %d left count:%d", ii, count);

			vmgr_list_destroy_queue(commData->result_q[VPU_OP_TYPE_DEC][ii]);
		}

		if (commData->result_q[VPU_OP_TYPE_ENC][ii] != NULL) {
			//count = vpu_dllist_get_count_sync(commData->result_q[VPU_OP_TYPE_ENC][ii]);
			//V_DBG(VPU_DBG_INFO, "enc result_q %d left count:%d", ii, count);

			vmgr_list_destroy_queue(commData->result_q[VPU_OP_TYPE_ENC][ii]);
		}
	}
}

void vmgr_list_reset_with_id(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	int count;
	vetc_mutex_lock(&commData->list_mutex);

	V_DBG(VPU_DBG_INFO, "reset list, op_type:%s(%d), drv_id:%d", vmgr_get_optype_name(op_type), op_type, drv_id);

	if (commData->wait_q[op_type][drv_id] != NULL) {
		count = vpu_dllist_get_count_sync(commData->wait_q[op_type][drv_id]);
		V_DBG(VPU_DBG_INFO, "%s wait_q %d left count:%d", vmgr_get_optype_name(op_type), drv_id, count);

		vmgr_list_destroy_queue(commData->wait_q[op_type][drv_id]);
		commData->wait_q[op_type][drv_id] = NULL;
	}

	if (commData->result_q[op_type][drv_id] != NULL) {
		count = vpu_dllist_get_count_sync(commData->result_q[op_type][drv_id]);
		V_DBG(VPU_DBG_INFO, "%s result_q %d left count:%d", vmgr_get_optype_name(op_type), drv_id, count);

		vmgr_list_destroy_queue(commData->result_q[op_type][drv_id]);
		commData->result_q[op_type][drv_id] = NULL;
	}

	vetc_mutex_unlock(&commData->list_mutex);
}

int vmgr_list_create_wait_q(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	int ret = 0;

	vetc_mutex_lock(&commData->list_mutex);

	if (commData->wait_q[op_type][drv_id] == NULL) {
		char queue_name[VDLLIST_MAX_NAME];

		memset(queue_name, 0x00, VDLLIST_MAX_NAME);
		snprintf(queue_name, VDLLIST_MAX_NAME - 1, "wait_%s_id%02d", op_type == VPU_OP_TYPE_DEC ? "dec" : "enc", drv_id);
		commData->wait_q[op_type][drv_id] = vpu_dllist_create(queue_name);
		//V_DBG(VPU_DBG_INFO, "create wait queue!! name:%s, op_type:%s(%d), drv_id:%d", queue_name, vmgr_get_optype_name(op_type), op_type, drv_id);
	} else {
		ret = -1;
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return ret;
}

int vmgr_list_create_result_q(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	int ret = 0;

	vetc_mutex_lock(&commData->list_mutex);

	if (commData->result_q[op_type][drv_id] == NULL) {
		char queue_name[VDLLIST_MAX_NAME];

		memset(queue_name, 0x00, VDLLIST_MAX_NAME);
		snprintf(queue_name, VDLLIST_MAX_NAME - 1, "result_%s_id%02d", op_type == VPU_OP_TYPE_DEC ? "dec" : "enc", drv_id);
		commData->result_q[op_type][drv_id] = vpu_dllist_create(queue_name);
		//V_DBG(VPU_DBG_INFO, "create result queue!! name:%s, op_type:%s(%d), drv_id:%d", queue_name, vmgr_get_optype_name(op_type), op_type, drv_id);
	} else {
		ret = -1;
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return 0;
}

int vmgr_list_destroy_wait_q(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	vetc_mutex_lock(&commData->list_mutex);

	if (commData->wait_q[op_type][drv_id] != NULL) {
		//V_DBG(VPU_DBG_INFO, "destroy result queue!!, op_type:%s(%d), drv_id:%d", vmgr_get_optype_name(op_type), op_type, drv_id);
		vmgr_list_destroy_queue(commData->wait_q[op_type][drv_id]);
		commData->wait_q[op_type][drv_id] = NULL;
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return 0;
}

int vmgr_list_destory_result_q(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	vetc_mutex_lock(&commData->list_mutex);

	if (commData->result_q[op_type][drv_id] != NULL) {
		//V_DBG(VPU_DBG_INFO, "destroy result queue!!, op_type:%s(%d), drv_id:%d", vmgr_get_optype_name(op_type), op_type, drv_id);
		vmgr_list_destroy_queue(commData->result_q[op_type][drv_id]);
		commData->result_q[op_type][drv_id] = NULL;
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return 0;
}

vpu_cmd_t *vmgr_list_alloc(vmgr_comm_t *commData)
{
	vpu_cmd_t *node = NULL;

	vetc_mutex_lock(&commData->list_mutex);

	if (commData->cmd_pool != NULL) {
		node = (vpu_cmd_t *)vpu_dllist_remove_head(commData->cmd_pool);

		if (node == NULL) {
			node = (vpu_cmd_t *)vpu_dllist_create_node(sizeof(vpu_cmd_t));
		}
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}

int vmgr_list_add(vmgr_comm_t *commData, vpu_cmd_t *cmd)
{
	int ret = 0;
	if (cmd != NULL) {
		vetc_mutex_lock(&commData->list_mutex);

		vpu_dllist_insert_tail(commData->cmd_q, (vpu_dllist_node_t *)cmd);
		commData->cmd_queued++;
		commData->thread_intr++;

		vetc_mutex_unlock(&commData->list_mutex);

		wake_up_interruptible(&commData->thread_wq);
	} else {
		V_DBG(VPU_DBG_ERROR, "Data is null");
		ret = -1;
	}

	return ret;
}

bool vmgr_list_is_empty(vmgr_comm_t *commData)
{
	bool isEmpty = 0;

	vetc_mutex_lock(&commData->list_mutex);

	if (vpu_dllist_get_count(commData->cmd_q) != 0) {
		isEmpty = false;
	} else {
		isEmpty = true;
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return isEmpty;
}

int vmgr_list_get_cmd_count(vmgr_comm_t *commData)
{
	int count = 0;

	vetc_mutex_lock(&commData->list_mutex);

	count = vpu_dllist_get_count(commData->cmd_q);

	vetc_mutex_unlock(&commData->list_mutex);

	return count;
}

int vmgr_list_get_cmd_count_with_criteria(vmgr_comm_t *commData, unsigned int drv_id, enum vpu_op_type op_type, enum vpu_cmd_type cmd_type)
{
	int count = 0;

	vpu_cmd_t *node = NULL;

	vetc_mutex_lock(&commData->list_mutex);

	node = (vpu_cmd_t *)vpu_dllist_get_head(commData->cmd_q);
	while (node != NULL) {
		//V_DBG(VPU_DBG_DETAIL, "[%d] op type:%s(%d), cmd:%s(%d), drv_id:%u, op_type:%d, cmd_type:%d",
		//	node->drv_id, vmgr_get_optype_name(node->op_type), node->op_type, vmgr_cmd_name(node->cmd_type), node->cmd_type, drv_id, op_type, cmd_type);

		if ((node->drv_id == drv_id) && (node->op_type == op_type) && (node->cmd_type == cmd_type)) {
			///V_DBG(VPU_DBG_INFO, "found cmd with drv_id:%u, op_type:%d, cmd_type:%d", node->drv_id, node->op_type, node->cmd_type);
			count++;
		}

		node = (vpu_cmd_t *)vpu_dllist_get_next(commData->cmd_q, (vpu_dllist_node_t *)node);
	}

	vetc_mutex_unlock(&commData->list_mutex);


	return count;
}

int vmgr_list_get_dec_cmd_count_with_id(vmgr_comm_t *commData, unsigned int drv_id)
{
	int count = 0;
	enum vpu_op_type op_type = VPU_OP_TYPE_DEC;
	vpu_cmd_t *node = NULL;

	vetc_mutex_lock(&commData->list_mutex);

	node = (vpu_cmd_t *)vpu_dllist_get_head(commData->cmd_q);
	while (node != NULL) {
		//V_DBG(VPU_DBG_DETAIL, "[%d] op type:%s(%d), cmd:%s(%d), drv_id:%u, op_type:%d, cmd_type:%d",
		//	node->drv_id, vmgr_get_optype_name(node->op_type), node->op_type, vmgr_cmd_name(node->cmd_type), node->cmd_type, drv_id, op_type, cmd_type);

		if ((node->drv_id == drv_id) && (node->op_type == op_type)) {
			///V_DBG(VPU_DBG_INFO, "found cmd with drv_id:%u, op_type:%d, cmd_type:%d", node->drv_id, node->op_type, node->cmd_type);
			count++;
		}

		node = (vpu_cmd_t *)vpu_dllist_get_next(commData->cmd_q, (vpu_dllist_node_t *)node);
	}

	vetc_mutex_unlock(&commData->list_mutex);


	return count;
}


vpu_cmd_t *vmgr_list_show_cmd(vmgr_comm_t *commData)
{
	vpu_cmd_t *node = NULL;

	vetc_mutex_lock(&commData->list_mutex);

	node = (vpu_cmd_t *)vpu_dllist_get_head(commData->cmd_q);

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}

vpu_cmd_t *vmgr_list_show_cmd_with_criteria(vmgr_comm_t *commData, unsigned int drv_id, enum vpu_op_type op_type, enum vpu_cmd_type cmd_type)
{
	vpu_cmd_t *node = NULL;

	vetc_mutex_lock(&commData->list_mutex);

	node = (vpu_cmd_t *)vpu_dllist_get_head(commData->cmd_q);
	while (node != NULL) {
		V_DBG(VPU_DBG_DETAIL, "[%d] op type:%s(%d), cmd:%s(%d), drv_id:%u, op_type:%d, cmd_type:%d",
			node->drv_id, vmgr_get_optype_name(node->op_type), node->op_type, vmgr_cmd_name(node->cmd_type), node->cmd_type, drv_id, op_type, cmd_type);
		if ((node->drv_id == drv_id) && (node->op_type == op_type) && (node->cmd_type == cmd_type)) {
			V_DBG(VPU_DBG_INFO, "found cmd with drv_id:%u, op_type:%d, cmd_type:%d", node->drv_id, node->op_type, node->cmd_type);
			break;
		}

		node = (vpu_cmd_t *)vpu_dllist_get_next(commData->cmd_q, (vpu_dllist_node_t *)node);
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}

vpu_cmd_t *vmgr_list_show_dec_cmd_with_id(vmgr_comm_t *commData, unsigned int drv_id)
{
	vpu_cmd_t *node = NULL;
	enum vpu_op_type op_type = VPU_OP_TYPE_DEC;

	vetc_mutex_lock(&commData->list_mutex);

	node = (vpu_cmd_t *)vpu_dllist_get_head(commData->cmd_q);
	while (node != NULL) {
		V_DBG(VPU_DBG_DETAIL, "[%d] op type:%s(%d), cmd:%s(%d), drv_id:%u, op_type:%d",
			node->drv_id, vmgr_get_optype_name(node->op_type), node->op_type, vmgr_cmd_name(node->cmd_type), node->cmd_type, drv_id, op_type);
		if ((node->drv_id == drv_id) && (node->op_type == op_type)) {
			V_DBG(VPU_DBG_INFO, "found cmd with drv_id:%u, op_type:%d, cmd_type:%d", node->drv_id, node->op_type, node->cmd_type);
			break;
		}

		node = (vpu_cmd_t *)vpu_dllist_get_next(commData->cmd_q, (vpu_dllist_node_t *)node);
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}

vpu_cmd_t *vmgr_list_show_dec_cmd_with_emergency(vmgr_comm_t *commData, unsigned int drv_id)
{
	vpu_cmd_t *node = NULL;
	enum vpu_op_type op_type = VPU_OP_TYPE_DEC;

	vetc_mutex_lock(&commData->list_mutex);

	node = (vpu_cmd_t *)vpu_dllist_get_head(commData->cmd_q);
	while (node != NULL) {
		//V_DBG(VPU_DBG_DETAIL, "[%d] op type:%s(%d), cmd:%s(%d), drv_id:%u, op_type:%d, cmd_type:%d",
		//	node->drv_id, vmgr_get_optype_name(node->op_type), node->op_type, vmgr_cmd_name(node->cmd_type), node->cmd_type, drv_id, op_type, cmd_type);
		if ((node->drv_id == drv_id) && (node->op_type == op_type) &&
			((node->cmd_type == VPU_CMD_DEC_FLUSH) || (node->cmd_type == VPU_CMD_DEC_CLOSE))
			) {
			//V_DBG(VPU_DBG_INFO, "found cmd with drv_id:%u, op_type:%d, cmd_type:%d", node->drv_id, node->op_type, node->cmd_type);
			break;
		}

		node = (vpu_cmd_t *)vpu_dllist_get_next(commData->cmd_q, (vpu_dllist_node_t *)node);
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}



//for debugging
#if 0
static void vmgr_list_print_cmd_queue(vmgr_comm_t *commData)
{
	int index = 0;
	vpu_cmd_t *node = NULL;
	int data_size = 0;

	node = (vpu_cmd_t *)vpu_dllist_get_head(commData->cmd_q);
	V_DBG(VPU_DBG_INFO, "---------------------");
	V_DBG(VPU_DBG_INFO, "print cmd queue, count:%d", vpu_dllist_get_count(commData->cmd_q));

	while (node != NULL) {
		if (node->cmd_type == 5) //decoding {
			vdec_v3_decode_t *cmd_decode = (vdec_v3_decode_t *)node->args;
			data_size = cmd_decode->input.bitstream_size;
		}

		V_DBG(VPU_DBG_INFO, "[%d] id:%d, op type:%s(%d), cmd:%s(%d), size:%d",
			index, node->drv_id, vmgr_get_optype_name(node->op_type), node->op_type, vmgr_cmd_name(node->cmd_type), node->cmd_type, data_size);

		node = (vpu_cmd_t *)vpu_dllist_get_next(commData->cmd_q, (vpu_dllist_node_t *)node);
		index++;
	}
	V_DBG(VPU_DBG_INFO, "---------------------");
}
#endif

vpu_cmd_t *vmgr_list_get_cmd(vmgr_comm_t *commData)
{
	vpu_cmd_t *node = NULL;

	vetc_mutex_lock(&commData->list_mutex);

	node = (vpu_cmd_t *)vpu_dllist_remove_head(commData->cmd_q);
	commData->cmd_queued--;

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}

vpu_cmd_t *vmgr_list_get_cmd_with_criteria(vmgr_comm_t *commData, unsigned int drv_id, enum vpu_op_type op_type, enum vpu_cmd_type cmd_type)
{
	vpu_cmd_t *node = NULL;

	vetc_mutex_lock(&commData->list_mutex);

	node = (vpu_cmd_t *)vpu_dllist_get_head(commData->cmd_q);
	while (node != NULL) {
		//V_DBG(VPU_DBG_DETAIL, "[%d] op type:%s(%d), cmd:%s(%d), drv_id:%u, op_type:%d, cmd_type:%d",
		//	node->drv_id, vmgr_get_optype_name(node->op_type), node->op_type, vmgr_cmd_name(node->cmd_type), node->cmd_type, drv_id, op_type, cmd_type);
		if ((node->drv_id == drv_id) && (node->op_type == op_type) && (node->cmd_type == cmd_type)) {
			//V_DBG(VPU_DBG_INFO, "found cmd with drv_id:%u, op_type:%d, cmd_type:%d", node->drv_id, node->op_type, node->cmd_type);
			break;
		}

		node = (vpu_cmd_t *)vpu_dllist_get_next(commData->cmd_q, (vpu_dllist_node_t *)node);
	}

	if (node != NULL) {
		vpu_dllist_remove_node(commData->cmd_q, (vpu_dllist_node_t *)node);

		commData->cmd_queued--;
	} else {
		node = NULL;
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}

vpu_cmd_t *vmgr_list_get_dec_cmd_with_id(vmgr_comm_t *commData, unsigned int drv_id)
{
	vpu_cmd_t *node = NULL;
	enum vpu_op_type op_type = VPU_OP_TYPE_DEC;

	vetc_mutex_lock(&commData->list_mutex);

	node = (vpu_cmd_t *)vpu_dllist_get_head(commData->cmd_q);
	while (node != NULL) {
		//V_DBG(VPU_DBG_DETAIL, "[%d] op type:%s(%d), cmd:%s(%d), drv_id:%u, op_type:%d, cmd_type:%d",
		//	node->drv_id, vmgr_get_optype_name(node->op_type), node->op_type, vmgr_cmd_name(node->cmd_type), node->cmd_type, drv_id, op_type, cmd_type);
		if ((node->drv_id == drv_id) && (node->op_type == op_type)) {
			//V_DBG(VPU_DBG_INFO, "found cmd with drv_id:%u, op_type:%d, cmd_type:%d", node->drv_id, node->op_type, node->cmd_type);
			break;
		}

		node = (vpu_cmd_t *)vpu_dllist_get_next(commData->cmd_q, (vpu_dllist_node_t *)node);
	}

	if (node != NULL) {
		vpu_dllist_remove_node(commData->cmd_q, (vpu_dllist_node_t *)node);

		commData->cmd_queued--;
	} else {
		node = NULL;
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}


vpu_cmd_t *vmgr_list_get_cmd_with_index(vmgr_comm_t *commData, int index)
{
	vpu_cmd_t *node = NULL;
	int repeat = index;

	vetc_mutex_lock(&commData->list_mutex);

	if (repeat >= 0) {
		node = (vpu_cmd_t *)vpu_dllist_get_head(commData->cmd_q);
		while (repeat > 0 && node != NULL) {
			node = (vpu_cmd_t *)vpu_dllist_get_next(commData->cmd_q, (vpu_dllist_node_t *)node);
			repeat--;
		}

		if (node != NULL) {
			vpu_dllist_remove_node(commData->cmd_q, (vpu_dllist_node_t *)node);
		}
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}

int vmgr_list_get_result_count(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	int count = 0;

	vetc_mutex_lock(&commData->list_mutex);

	if (commData->result_q[op_type][drv_id] != NULL) {
		count = vpu_dllist_get_count(commData->result_q[op_type][drv_id]);
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return count;
}

vpu_cmd_t *vmgr_list_show_result(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	vpu_cmd_t *node = NULL;

	vetc_mutex_lock(&commData->list_mutex);

	if (commData->result_q[op_type][drv_id] != NULL) {
		node = (vpu_cmd_t *)vpu_dllist_get_head(commData->result_q[op_type][drv_id]);
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}

vpu_cmd_t *vmgr_list_get_result(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	vpu_cmd_t *node = NULL;

	vetc_mutex_lock(&commData->list_mutex);

	if (commData->result_q[op_type][drv_id] != NULL) {
		node = (vpu_cmd_t *)vpu_dllist_remove_head(commData->result_q[op_type][drv_id]);
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}

vpu_cmd_t *vmgr_list_get_wait(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id)
{
	vpu_cmd_t *node = NULL;

	vetc_mutex_lock(&commData->list_mutex);

	if (commData->wait_q[op_type][drv_id] != NULL) {
		node = (vpu_cmd_t *)vpu_dllist_remove_head(commData->wait_q[op_type][drv_id]);
	}

	vetc_mutex_unlock(&commData->list_mutex);

	return node;
}

int vmgr_list_add_pool(vmgr_comm_t *commData, vpu_cmd_t *cmd)
{
	int ret = 0;

	if (cmd != NULL) {
		vetc_mutex_lock(&commData->list_mutex);

		if (cmd->args != NULL) {
			VPU_free(cmd->args);
			cmd->args = NULL;
		}

		vpu_dllist_insert_tail(commData->cmd_pool, (vpu_dllist_node_t *)cmd);

		vetc_mutex_unlock(&commData->list_mutex);
	} else {
		V_DBG(VPU_DBG_ERROR, "Data is null");
		ret = -1;
	}

	return ret;
}

int vmgr_list_add_result(vmgr_comm_t *commData, vpu_cmd_t *cmd)
{
	int ret = 0;

	if (cmd != NULL) {
		vetc_mutex_lock(&commData->list_mutex);

		//When vmgr_list_add_result() is called, if the driver is closed and vmgr_deregister() is invoked,
		//resulting in result_q being destroyed and becoming NULL, it is necessary to check if it is NULL before inserting.
		if (commData->result_q[cmd->op_type][cmd->drv_id] != NULL) {
			vpu_dllist_insert_tail(commData->result_q[cmd->op_type][cmd->drv_id], (vpu_dllist_node_t *)cmd);
		}

		vetc_mutex_unlock(&commData->list_mutex);
	} else {
		V_DBG(VPU_DBG_ERROR, "Data is null");
		ret = -1;
	}

	return ret;
}

void vmgr_list_flush_result(vmgr_comm_t *commData, unsigned int drv_id, enum vpu_op_type op_type)
{
	vpu_dllist_t *result_q = commData->result_q[op_type][drv_id];

	vetc_mutex_lock(&commData->list_mutex);

	vmgr_list_flush(result_q);

	V_DBG(VPU_DBG_INFO, "drv_id:%d, after flush, result_q count:%d", drv_id, vpu_dllist_get_count(result_q));

	vetc_mutex_unlock(&commData->list_mutex);
}

int vmgr_list_add_resultwait(vmgr_comm_t *commData, vpu_cmd_t *cmd)
{
	int ret = 0;

	if (cmd != NULL) {
		vetc_mutex_lock(&commData->list_mutex);

		vpu_dllist_insert_tail(commData->wait_q[cmd->op_type][cmd->drv_id], (vpu_dllist_node_t *)cmd);

		vetc_mutex_unlock(&commData->list_mutex);
	} else {
		V_DBG(VPU_DBG_ERROR, "Data is null");
		ret = -1;
	}

	return ret;
}

void vmgr_list_flush_resultwait(vmgr_comm_t *commData, unsigned int drv_id, enum vpu_op_type op_type)
{
	vpu_dllist_t *wait_q = commData->wait_q[op_type][drv_id];

	vetc_mutex_lock(&commData->list_mutex);

	vmgr_list_flush(wait_q);

	vetc_mutex_unlock(&commData->list_mutex);
}

