/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#include "vpu_dllist.h"
#include "vpu_memtrace.h"

#define DBG(fmt, args...)	do { pr_err("[MGR][%s:%d]" fmt "\n", __FUNCTION__, __LINE__, ## args); } while (0)

vpu_dllist_node_t *vpu_dllist_create_node(int size)
{
	vpu_dllist_node_t *node;

	//NU_ASSERT(size > sizeof(struct vpu_dllist_tag));

	node = (vpu_dllist_node_t *)VPU_alloc(size);

	return node;
}

void vpu_dllist_destroy_node(vpu_dllist_node_t *node)
{
	//DLLIST_ASSERT(node != NULL);

	VPU_free(node);
}

vpu_dllist_t *vpu_dllist_create(char *name)
{
	vpu_dllist_t *list;

	//@@@DLL_ASSERT(((pfMalloc == NULL) && (pfFree == NULL)) || ((pfMalloc != NULL) && (pfFree != NULL)));

	list = (vpu_dllist_t *)VPU_alloc(sizeof(struct vpu_dllist_tag));
	if (list != NULL) {
		strscpy(list->name, name, VDLLIST_MAX_NAME);
		list->head = NULL;
		list->tail = NULL;
		list->count = 0;
	}

	mutex_init(&list->mutex);

	return list;
}

void vpu_dllist_destroy(vpu_dllist_t *list)
{
	vpu_dllist_node_t *node = NULL;

	//NU_ASSERT(this != NULL);

	while (list->head != NULL) {
		node = vpu_dllist_remove_head(list);
		if (node != NULL) {
			vpu_dllist_destroy_node(node);
		}
	}

	VPU_free(list);
}

void vpu_dllist_insert_after(vpu_dllist_t *list, vpu_dllist_node_t *insert_node, vpu_dllist_node_t *new_node)
{
	//DLLIST_ASSERT(list != NULL);
	//DLLIST_ASSERT(insert_node != NULL);
	//DLLIST_ASSERT(new_node != NULL);

	new_node->prev = insert_node;
	new_node->next = insert_node->next;
	list->count++;

	if (new_node->next != NULL) {
		new_node->next->prev = new_node;
	} else {
		list->tail = new_node;
	}

	insert_node->next = new_node;
}

void vpu_dllist_insert_before(vpu_dllist_t *list, vpu_dllist_node_t *insert_node, vpu_dllist_node_t *new_node)
{
	//DLLIST_ASSERT(list != NULL);
	//DLLIST_ASSERT(insert_node != NULL);
	//DLLIST_ASSERT(new_node != NULL);

	new_node->prev = insert_node->prev;
	new_node->next = insert_node;
	list->count++;

	if (new_node->prev != NULL) {
		new_node->prev->next = new_node;
	} else {
		list->head = new_node;
	}

	insert_node->prev = new_node;
}

vpu_dllist_node_t *vpu_dllist_remove_node(vpu_dllist_t *list, vpu_dllist_node_t *node)
{
	//DLLIST_ASSERT(list != NULL);
	//DLLIST_ASSERT(node != NULL);
	//DLLIST_ASSERT(list->count > 0);

	list->count--;

	if (node->prev != NULL) {
		node->prev->next = node->next;
	} else {
		list->head = node->next;
	}

	if (node->next != NULL) {
		node->next->prev = node->prev;
	} else {
		list->tail = node->prev;
	}

	return node;
}

void vpu_dllist_insert_head(vpu_dllist_t *list, vpu_dllist_node_t *node)
{
	//DLLIST_ASSERT(list != NULL);
	//DLLIST_ASSERT(node != NULL);

	if (list->head == NULL) {
		list->head = node;
		list->tail = node;
		node->prev = NULL;
		node->next = NULL;
		list->count++;
	} else {
		vpu_dllist_insert_before(list, list->head, node);
	}

	//if (list->count > 0) printk("@@@ DDLIST : INSERT_HEAD : name=%s, count=%d\n", list->name, list->count);
}

void vpu_dllist_insert_tail(vpu_dllist_t *list, vpu_dllist_node_t *node)
{
	//DLLIST_ASSERT(list != NULL);
	//DLLIST_ASSERT(node != NULL);

	if (list->tail == NULL) {
		list->head = node;
		list->tail = node;
		node->prev = NULL;
		node->next = NULL;
		list->count++;
	} else {
		vpu_dllist_insert_after(list, list->tail, node);
	}

	//if (list->count > 0) printk("@@@ DDLIST : INSERT_TAIL : name=%s, count=%d\n", list->name, list->count);
}

vpu_dllist_node_t *vpu_dllist_remove_head(vpu_dllist_t *list)
{
	vpu_dllist_node_t *node = NULL;

	//DLLIST_ASSERT(list != NULL);

	if (list->head != NULL) {
		node = vpu_dllist_remove_node(list, list->head);
	}

	//if (node) printk("@@@ DDLIST : REMOVE_HEAD : name=%s, count=%d, !null=%d\n", list->name, list->count, (node!=NULL) ? 1 : 0);

	return node;
}

vpu_dllist_node_t *vpu_dllist_remove_tail(vpu_dllist_t *list)
{
	vpu_dllist_node_t *node = NULL;

	//DLLIST_ASSERT(list != NULL);

	if (list->tail != NULL) {
		node = vpu_dllist_remove_node(list, list->tail);
	}

	//if (node) printk("@@@ DDLIST : REMOVE_TAIL : name=%s, count=%d, !null=%d\n", list->name, list->count, (node!=NULL) ? 1 : 0);

	return node;
}

vpu_dllist_node_t *vpu_dllist_get_head(vpu_dllist_t *list)
{
	//DLLIST_ASSERT(list != NULL);

	return (list->head == NULL) ? NULL : list->head;
}

vpu_dllist_node_t *vpu_dllist_get_tail(vpu_dllist_t *list)
{
	//DLLIST_ASSERT(list != NULL);

	return (list->tail == NULL) ? NULL : list->tail;
}

vpu_dllist_node_t *vpu_dllist_get_next(vpu_dllist_t *list, vpu_dllist_node_t *node)
{
	if (node != NULL) {
		node = node->next;
	}

	return node;
}

int vpu_dllist_get_count(vpu_dllist_t *list)
{
	//DLLIST_ASSERT(list != NULL);

	return list->count;
}

void vpu_dllist_insert_head_sync(vpu_dllist_t *list, vpu_dllist_node_t *node)
{
	mutex_lock(&list->mutex);
	{
		vpu_dllist_insert_head(list, node);
	}
	mutex_unlock(&list->mutex);
}

void vpu_dllist_insert_tail_sync(vpu_dllist_t *list, vpu_dllist_node_t *node)
{
	if (list == NULL) {
		DBG("#### list is NULL");
	}

	if (node == NULL) {
		DBG("#### node is NULL");
	}

	mutex_lock(&list->mutex);
	{
		vpu_dllist_insert_tail(list, node);
	}
	mutex_unlock(&list->mutex);
}

vpu_dllist_node_t *vpu_dllist_remove_sync(vpu_dllist_t *list, vpu_dllist_node_t *node)
{
	mutex_lock(&list->mutex);
	{
		node = vpu_dllist_remove_node(list, node);
	}
	mutex_unlock(&list->mutex);

	return node;
}

vpu_dllist_node_t *vpu_dllist_remove_head_sync(vpu_dllist_t *list)
{
	vpu_dllist_node_t *node;

	mutex_lock(&list->mutex);
	{
		node = vpu_dllist_remove_head(list);
	}
	mutex_unlock(&list->mutex);

	return node;
}

vpu_dllist_node_t *vpu_dllist_remove_tail_sync(vpu_dllist_t *list)
{
	vpu_dllist_node_t *node;

	mutex_lock(&list->mutex);
	{
		node = vpu_dllist_remove_tail(list);
	}
	mutex_unlock(&list->mutex);

	return node;
}

vpu_dllist_node_t *vpu_dllist_get_head_sync(vpu_dllist_t *list)
{
	vpu_dllist_node_t *node;

	mutex_lock(&list->mutex);
	{
		node = vpu_dllist_get_head(list);
	}
	mutex_unlock(&list->mutex);

	return node;
}

vpu_dllist_node_t *vpu_dllist_get_tail_sync(vpu_dllist_t *list)
{
	vpu_dllist_node_t *node;

	mutex_lock(&list->mutex);
	{
		node = vpu_dllist_get_tail(list);
	}
	mutex_unlock(&list->mutex);

	return node;
}

vpu_dllist_node_t *vpu_dllist_get_next_sync(vpu_dllist_t *list, vpu_dllist_node_t *node)
{
	mutex_lock(&list->mutex);
	node = node->next;
	mutex_unlock(&list->mutex);

	return node;
}

int vpu_dllist_get_count_sync(vpu_dllist_t *list)
{
	int count;

	mutex_lock(&list->mutex);
	{
		count = vpu_dllist_get_count(list);
	}
	mutex_unlock(&list->mutex);

	return count;
}

char *vpu_dllist_name(vpu_dllist_t *list)
{
	return list->name;
}

