/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_LIST_MANAGER_H
#define VPU_LIST_MANAGER_H

#include "vpu_comm.h"
#include "vpu_internal_type.h"

void vmgr_list_flush(vpu_dllist_t *list);

void vmgr_list_flush_sync(vpu_dllist_t *list);

void vmgr_list_init(vmgr_comm_t *commData);

void vmgr_list_deinit(vmgr_comm_t *commData);

//When each driver is terminated, a reset for the corresponding ID is required in the list
void vmgr_list_reset_with_id(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id);

int vmgr_list_create_wait_q(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id);

int vmgr_list_create_result_q(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id);

int vmgr_list_destroy_wait_q(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id);

int vmgr_list_destory_result_q(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id);

vpu_cmd_t *vmgr_list_alloc(vmgr_comm_t *commData);

int vmgr_list_add(vmgr_comm_t *commData, vpu_cmd_t *arg);

bool vmgr_list_is_empty(vmgr_comm_t *commData);

int vmgr_list_get_cmd_count(vmgr_comm_t *commData);

int vmgr_list_get_cmd_count_with_criteria(vmgr_comm_t *commData, unsigned int drv_id, enum vpu_op_type op_type, enum vpu_cmd_type cmd_type);

int vmgr_list_get_dec_cmd_count_with_id(vmgr_comm_t *commData, unsigned int drv_id);

//show command, not delete head
vpu_cmd_t *vmgr_list_show_cmd(vmgr_comm_t *commData);

vpu_cmd_t *vmgr_list_show_cmd_with_criteria(vmgr_comm_t *commData, unsigned int drv_id, enum vpu_op_type op_type, enum vpu_cmd_type cmd_type);

vpu_cmd_t *vmgr_list_show_dec_cmd_with_emergency(vmgr_comm_t *commData, unsigned int drv_id);

//get command, delete head
vpu_cmd_t *vmgr_list_get_cmd(vmgr_comm_t *commData);

vpu_cmd_t *vmgr_list_get_dec_cmd_with_id(vmgr_comm_t *commData, unsigned int drv_id);

//Retrieve the command from the cmd queue using the index
vpu_cmd_t *vmgr_list_get_cmd_with_index(vmgr_comm_t *commData, int index);

vpu_cmd_t *vmgr_list_get_cmd_with_criteria(vmgr_comm_t *commData, unsigned int drv_id, enum vpu_op_type op_type, enum vpu_cmd_type cmd_type);

vpu_cmd_t *vmgr_list_show_dec_cmd_with_id(vmgr_comm_t *commData, unsigned int drv_id);

//return the number of nodes in the command queue that have the same driver ID and operation type
int vmgr_list_cmd_count_with_id(vmgr_comm_t *commData, enum vpu_op_type op_type, int drv_id);

int vmgr_list_get_result_count(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id);

//show result, not delete head
vpu_cmd_t *vmgr_list_show_result(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id);

//get result, delete head
vpu_cmd_t *vmgr_list_get_result(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id);

//get wait, delete head
vpu_cmd_t *vmgr_list_get_wait(vmgr_comm_t *commData, const enum vpu_op_type op_type, const unsigned int drv_id);


int vmgr_list_add_pool(vmgr_comm_t *commData, vpu_cmd_t *cmd);

int vmgr_list_add_result(vmgr_comm_t *commData, vpu_cmd_t *cmd);

void vmgr_list_flush_result(vmgr_comm_t *commData, unsigned int drv_id, enum vpu_op_type op_type);

int vmgr_list_add_resultwait(vmgr_comm_t *commData, vpu_cmd_t *cmd);

void vmgr_list_flush_resultwait(vmgr_comm_t *commData, unsigned int drv_id, enum vpu_op_type op_type);

#endif //VPU_LIST_MANAGER_H