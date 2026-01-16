/* 
* SPDX-License-Identifier: BSD-3-Clause OR GPL-2.0
* Copyright 2025 Telechips Inc. 
* Contact: jayhouse@telechips.com
*/

#ifndef VPU_DLLIST_H
#define VPU_DLLIST_H

#include "vpu_linux_kernel.h"

typedef struct vpu_dllist_node_tag {
    struct vpu_dllist_node_tag *next;
    struct vpu_dllist_node_tag *prev;
} vpu_dllist_node_t;

#define VDLLIST_MAX_NAME	64
typedef struct vpu_dllist_tag {
    char name[VDLLIST_MAX_NAME];
    struct mutex mutex;
    vpu_dllist_node_t *head;
    vpu_dllist_node_t *tail;
    unsigned int count;
} vpu_dllist_t;

//node functions
vpu_dllist_node_t *vpu_dllist_create_node(int size);

void vpu_dllist_destroy_node(vpu_dllist_node_t *node);


//vpu_dllist_t functions
vpu_dllist_t *vpu_dllist_create(char *name);

void vpu_dllist_destroy(vpu_dllist_t *list);


void vpu_dllist_insert_after(vpu_dllist_t *list, vpu_dllist_node_t *insert_node, vpu_dllist_node_t *new_node);

void vpu_dllist_insert_before(vpu_dllist_t *list, vpu_dllist_node_t *insert_node, vpu_dllist_node_t *new_node);

vpu_dllist_node_t *vpu_dllist_remove_node(vpu_dllist_t *list, vpu_dllist_node_t *node);


void vpu_dllist_insert_head(vpu_dllist_t *list, vpu_dllist_node_t *node);

void vpu_dllist_insert_tail(vpu_dllist_t *list, vpu_dllist_node_t *node);

vpu_dllist_node_t *vpu_dllist_remove_head(vpu_dllist_t *list);

vpu_dllist_node_t *vpu_dllist_remove_tail(vpu_dllist_t *list);

vpu_dllist_node_t *vpu_dllist_get_head(vpu_dllist_t *list);

vpu_dllist_node_t *vpu_dllist_get_tail(vpu_dllist_t *list);

vpu_dllist_node_t *vpu_dllist_get_next(vpu_dllist_t *list, vpu_dllist_node_t *node);

int vpu_dllist_get_count(vpu_dllist_t *list);


//inter lock functions
void vpu_dllist_insert_head_sync(vpu_dllist_t *list, vpu_dllist_node_t *node);

void vpu_dllist_insert_tail_sync(vpu_dllist_t *list, vpu_dllist_node_t *node);

vpu_dllist_node_t *vpu_dllist_remove_sync(vpu_dllist_t *list, vpu_dllist_node_t *node);

vpu_dllist_node_t *vpu_dllist_remove_head_sync(vpu_dllist_t *list);

vpu_dllist_node_t *vpu_dllist_remove_tail_sync(vpu_dllist_t *list);

vpu_dllist_node_t *vpu_dllist_get_head_sync(vpu_dllist_t *list);

vpu_dllist_node_t *vpu_dllist_get_tail_sync(vpu_dllist_t *list);

vpu_dllist_node_t *vpu_dllist_get_next_sync(vpu_dllist_t *list, vpu_dllist_node_t *node);

int vpu_dllist_get_count_sync(vpu_dllist_t *list);

//for debugging
char *vpu_dllist_name(vpu_dllist_t *list);

#endif // VPU_DLLIST_H

