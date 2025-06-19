/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _VPU2_IOPARAM_FLEX_H_
#define _VPU2_IOPARAM_FLEX_H_

#include <linux/types.h>
#include <linux/stddef.h>

#if defined(__cplusplus)
extern "C" {
#endif

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
// Definition of flexible binary structures for vpu ioctl
//
/* -----------------------------------------------------------------------------
 * definition of vpu2 flex args
 * -------------------------------------------------------------------------- */
// [FIXME] signedness: dec/disp index: signed vs. phys_addr_t: unsigned
struct v2hw_flex_io {
    /*
     * ioctl to request (V2D_IP_XXX)
     * - ip_keys  : should set what vpu lib. need to know
     * - op_keys  : should set what u/s want to get
     * - csi_off  : should be set to zero
     * - numfields: num. of fields to be set by ip_keys
     *
     * ioctl to response (V2D_OP_XXX)
     * - ip_keys  : should be set to zero. should set csi keys iff exists
     * - op_keys  : shoudl be set by vpu lib.
     * - csi_off  : should be set by vpu lib.
     * - numfields: num. of fields to be set by op_keys
     */
    uint32_t  version;   // [TBA] how to use it effectively ?
    uint32_t  fx_size;   // total size allocated

    uint64_t  v1_strt;   // [FIXME] designate ptr to v1 vpu struct

    uint64_t  ip_keys;   // keys for  input process
    uint64_t  op_keys;   // keys for output process
    uint32_t  csi_off;   // offset for csi start point

    uint32_t  boolean;   // [ip] bitfield for boolean variables
                         // [op] field composition type (0: flatten index array)
    int32_t   result;    // op(out operation) only
    uint32_t  maxfields; // max num of fields to be set into a flex array
    uint32_t  numfields; // num. of fields in a flex array

#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wzero-length-array"
#endif
#if defined(__cplusplus)
    // c++ doesn't support C99 flex array member: fields[]
    // (and fields[0]? Don't quote me on that.)
    int32_t fields[1]; // fields[], fields[0]: not sized, fields[1]: sized
#else
    int32_t fields[];
#endif
#if defined(__clang__)
#pragma clang diagnostic pop
#endif
};

#ifdef __KERNEL__
/* -----------------------------------------------------------------------------
 * delayed flexio buffering controller (to fit interlace pair)
 * -------------------------------------------------------------------------- */
enum V2ENUM_DIO_STAT {
    V2_DIO_EMPTY = 0,
    V2_DIO_DELAY = 1,
    V2_DIO_READY = 2,
};

struct v2hw_io_delay {
    int32_t ip_state[2];
    int32_t op_state;
};

/* -----------------------------------------------------------------------------
 * delayed flex io handle life-cycle control
 * -------------------------------------------------------------------------- */
#define V2_FLXDIO_RESET(h) do { \
    if (h != NULL) { \
		h->ip_state[0] = V2_DIO_EMPTY; \
		h->ip_state[1] = V2_DIO_EMPTY; \
		h->op_state    = V2_DIO_EMPTY; \
    }} while((bool)0)

#define V2_FLXDIO_DETACH(h) do { \
    if (h != NULL) { \
        kfree(h);    \
        h = NULL;    \
    }} while((bool)0)

#define V2_FLXDIO_CREATE(h) do { \
        size_t fx_sz = sizeof(struct v2hw_io_delay); \
        h = (struct v2hw_io_delay *)(kzalloc(fx_sz, GFP_KERNEL)); \
        V2_FLXDIO_RESET(h); \
    } while((bool)0)

#endif // __KERNEL__

/* -----------------------------------------------------------------------------
 * flex array handle life-cycle control
 * -------------------------------------------------------------------------- */
/*
 * Defines a dynamically allocated v2 flex io
 * and ensures its parameters are valid.
 */
#define V2_FLEX_LEN(num) (sizeof(int) * (num))

#define V2_FLEXIO_INIT(h, num) do { \
        h->v1_strt = 0u;    \
        h->ip_keys = 0u;    \
        h->op_keys = 0u;    \
        h->csi_off = 0u;    \
        h->boolean = 0u;    \
        h->result  = 0;     \
        h->numfields = 0u;  \
        h->maxfields = num; \
    } while((bool)0)

#ifdef __KERNEL__
#define V2_FLEXIO_CREATE(h, num) do { \
    size_t fx_sz = sizeof(struct v2hw_flex_io) + V2_FLEX_LEN(num); \
    h = (struct v2hw_flex_io *)(kzalloc(fx_sz, GFP_KERNEL)); \
    if (h != NULL) { \
        h->version = 10u; \
        h->fx_size = fx_sz;    \
        V2_FLEXIO_INIT(h, num); \
    }} while((bool)0)
#else  /* __KERNEL__ */
#if defined(__cplusplus)
#define V2_FLEXIO_CREATE(h, num) do { \
    size_t fx_sz = sizeof(struct v2hw_flex_io) + V2_FLEX_LEN(num); \
    h = static_cast<struct v2hw_flex_io *>(calloc(1u, fx_sz)); \
    if (h != nullptr) { \
        h->version = 11u; \
        h->fx_size = fx_sz;    \
        V2_FLEXIO_INIT(h, num); \
    }} while((bool)0)
#else
#define V2_FLEXIO_CREATE(h, num) do { \
    size_t fx_sz = sizeof(struct v2hw_flex_io) + V2_FLEX_LEN(num); \
    h = (struct v2hw_flex_io *)(calloc(1u, fx_sz)); \
    if (h != NULL) { \
        h->version = 12u; \
        h->fx_size = fx_sz;    \
        V2_FLEXIO_INIT(h, num); \
    }} while((bool)0)
#endif
#endif

#define V2_FLEXIO_ASSIGN(h, num) do {  \
    if (h == NULL) {                   \
        V2_FLEXIO_CREATE(h, num); \
    } else {                           \
        V2_FLEXIO_INIT(h, num); \
    }} while((bool)0)

#ifdef __KERNEL__
#define V2_FLEXIO_DELETE(h) do { \
    if (h != NULL) { \
        kfree(h);     \
    }} while((bool)0)
#else
#define V2_FLEXIO_DELETE(h) do { \
    if (h != NULL) { \
        free(h);     \
    }} while((bool)0)
#endif

#define V2_FLEXIO_DETACH(h_list, length) do { \
    if (h_list != NULL) { \
        int32_t i; \
        for (i = 0; i < length; i++) { \
            if (h_list[i] != NULL) { \
                V2_FLEXIO_DELETE(h_list[i]); \
                h_list[i] = NULL; \
            } \
    }}} while((bool)0)

#define V2_FLEXIO_RESET(h_list, length) do { \
    if (h_list != NULL) { \
        int32_t i; \
        for (i = 0; i < length; i++) \
            h_list[i] = NULL; \
    }} while((bool)0)

#define V2_FLEXIO_CLONE(h, t) do { \
    if (h && t) { \
        h->version = t->version;   \
        h->ip_keys = t->ip_keys;   \
        h->op_keys = t->op_keys;   \
        h->csi_off = t->csi_off;   \
        h->boolean = t->boolean;   \
        h->result  = t->result;    \
    }} while((bool)0)

/* -----------------------------------------------------------------------------
 * key/value getter/setter for boolean
 * -------------------------------------------------------------------------- */
#define V2_FLEX_B_CLEAR(h, key) do { \
        h->boolean &= ~((uint32_t)key); \
    } while((bool)0)

#define V2_FLEX_B_CLEAR_IF(cond, h, key) do { \
        if (cond) { h->boolean &= ~((uint32_t)key); } \
    } while((bool)0)

#define V2_FLEX_B_SET(h, key) do { \
        h->boolean |= key;         \
    } while((bool)0)

#define V2_FLEX_B_SET_IF(cond, h, key) do { \
        if (cond) { h->boolean |= key; }  \
    } while((bool)0)

#define V2_FLEX_B_SET_INC_IF(cond, h, key, inc) do { \
    if (cond) {  \
        h->boolean |= key;   \
        h->numfields += inc; \
    } } while((bool)0)

/* bvp bool value pair */
#define V2_FLEX_B_GET(h, bvp, val) do { \
        if (h->boolean & V2D_KI_##bvp) { val = 1; }  \
        else val = 0; \
    } while((bool)0)

#define V2_FLEXIP_B_CHECK(h, bvp) (h->boolean & V2D_KI_##bvp)

/* -----------------------------------------------------------------------------
 * key/value getter/setter for flex field
 * -------------------------------------------------------------------------- */
/* hal module side */
#define V2_FLEXIP_SET(h, kvp, val) do { \
    if ((h->ip_keys & (uint64_t)V2D_KI_##kvp) > 0ull) { \
        /* just update this field */ \
        h->fields[V2D_FI_##kvp] = val;  \
    } else { \
        if (h->numfields < h->maxfields) {  \
            h->ip_keys |= (uint64_t)V2D_KI_##kvp;     \
            h->fields[V2D_FI_##kvp] = val;  \
            h->numfields++; \
        } \
    }} while((bool)0)

// kvp: key-value pair
#define V2_FLEXOP_GET(h, kvp, lval) do { \
        lval = h->fields[V2D_FO_##kvp];  \
    } while((bool)0)

#define V2_FLEXOP_CSI_GET(h, kvp, lval) do { \
    if ((h->csi_off + V2D_CSI_FO_##kvp) < h->maxfields) { \
        lval = h->fields[h->csi_off + V2D_CSI_FO_##kvp]; \
    }} while((bool)0)

// conditional get
#define V2_FLEXOP_CND_GET(h, kvp, lval) do { \
    if ((h->op_keys & (uint64_t)V2D_KO_##kvp) > 0ull) {  \
        lval = h->fields[V2D_FO_##kvp];   \
    }} while((bool)0)

#define V2_FLEXOP_CSI_CND_GET(h, kvp, lval) do { \
    if ((h->ip_keys & (uint64_t)V2D_CSI_KO_##kvp) > 0ull) {  \
        lval = h->fields[h->csi_off + V2D_CSI_FO_##kvp]; \
    }} while((bool)0)

/* hw module side */
#define V2_FLEXIP_GET(h, kvp, lval) do { \
    if ((h->ip_keys & (uint64_t)V2D_KI_##kvp) > 0ull) {  \
        lval = h->fields[V2D_FI_##kvp]; \
    }} while((bool)0)

#define V2_FLEXOP_SET(h, kvp, val) do { \
    if ((h->op_keys & (uint64_t)V2D_KO_##kvp) > 0ull) { \
        /* just update this field */ \
        h->fields[V2D_FO_##kvp] = val;  \
    } else { \
        if (h->numfields < h->maxfields) {  \
            h->op_keys |= (uint64_t)V2D_KO_##kvp;     \
            h->fields[V2D_FO_##kvp] = val;  \
            h->numfields++; \
        } \
    }} while((bool)0)

#define V2_FLEXOP_CSI_OFFSET(h, val) do { \
        h->csi_off = val;     \
    } while((bool)0)

#define V2_FLEXOP_CSI_SET(h, kvp, val) do { \
    if (h->numfields < h->maxfields) {  \
        h->ip_keys |= (uint64_t)V2D_CSI_KO_##kvp;     \
        h->fields[h->csi_off + V2D_CSI_FO_##kvp] = val;  \
    } h->numfields++; \
    } while((bool)0)

/* v2 fields for buffer address: address bit twiddling */
#define V2_FLEXIP_GET_ADDR(h, kvp, dest) do { \
        union { int32_t i32v; uint64_t u64v; } high; \
        union { int32_t i32v; uint64_t u64v; } low; \
        high.u64v = 0ull; low.i32v = 0; \
        V2_FLEXIP_GET(h, kvp##_H, high.i32v); \
        V2_FLEXIP_GET(h, kvp##_L, low.i32v); \
        dest = ((high.u64v << 32u) & 0xFFFFFFFF00000000ull) \
             | (low.u64v & 0XFFFFFFFFull); \
    } while((bool)0)

#define V2_FLEXIP_GET_ADDR2PTR(h, kvp, dest) do { \
        uintptr_t temp = 0u; \
        union { int32_t i32v; uint64_t u64v; } high; \
        union { int32_t i32v; uint64_t u64v; } low; \
        high.u64v = 0ull; low.i32v = 0; \
        V2_FLEXIP_GET(h, kvp##_H, high.i32v); \
        V2_FLEXIP_GET(h, kvp##_L, low.i32v); \
        temp = ((high.u64v << 32u) & 0xFFFFFFFF00000000ull) \
             | (low.u64v & 0XFFFFFFFFull); \
        dest = (uint8_t *)temp; \
    } while((bool)0)

#define V2_FLEXIP_SET_ADDR(h, kvp, dest) do { \
        union { int32_t i32v; uint64_t u64v; } high; \
        union { int32_t i32v; uint64_t u64v; } low; \
        high.u64v = ((uint64_t)dest >> 32ull); \
        low.u64v = ((uint64_t)dest & 0xFFFFFFFFull); \
        V2_FLEXIP_SET(h, kvp##_H, high.i32v); \
        V2_FLEXIP_SET(h, kvp##_L, low.i32v); \
    } while((bool)0)

#define V2_FLEXOP_GET_ADDR(h, kvp, dest) do { \
        union { int32_t i32v; uint64_t u64v; } high; \
        union { int32_t i32v; uint64_t u64v; } low; \
        high.u64v = 0ull; low.i32v = 0; \
        V2_FLEXOP_GET(h, kvp##_H, high.i32v); \
        V2_FLEXOP_GET(h, kvp##_L, low.i32v); \
        dest = ((high.u64v << 32u) & 0xFFFFFFFF00000000ull) \
             | (low.u64v & 0XFFFFFFFFull); \
    } while((bool)0)

#define V2_FLEXOP_GET_ADDR2PTR(h, kvp, dest) do { \
        uintptr_t temp = 0u; \
        union { int32_t i32v; uint64_t u64v; } high; \
        union { int32_t i32v; uint64_t u64v; } low; \
        high.u64v = 0ull; low.i32v = 0; \
        V2_FLEXOP_GET(h, kvp##_H, high.i32v); \
        V2_FLEXOP_GET(h, kvp##_L, low.i32v); \
        temp = ((high.u64v << 32u) & 0xFFFFFFFF00000000ull) \
             | (low.u64v & 0XFFFFFFFFull); \
        dest = (uint8_t *)temp; \
    } while((bool)0)

#define V2_FLEXOP_SET_ADDR(h, kvp, dest) do { \
        union { int32_t i32v; uint64_t u64v; } high; \
        union { int32_t i32v; uint64_t u64v; } low; \
        high.u64v = ((uint64_t)dest >> 32ull); \
        low.u64v = ((uint64_t)dest & 0xFFFFFFFFull); \
        V2_FLEXOP_SET(h, kvp##_H, high.i32v); \
        V2_FLEXOP_SET(h, kvp##_L, low.i32v); \
    } while((bool)0)

#define V2_FLEXOP_CSI_GET_ADDR(h, kvp, dest) do { \
        union { int32_t i32v; uint64_t u64v; } high; \
        union { int32_t i32v; uint64_t u64v; } low; \
        high.u64v = 0ull; low.i32v = 0; \
        V2_FLEXOP_CSI_GET(h, kvp##_H, high.i32v); \
        V2_FLEXOP_CSI_GET(h, kvp##_L, low.i32v); \
        dest = ((high.u64v << 32u) & 0xFFFFFFFF00000000ull) \
             | (low.u64v & 0XFFFFFFFFull); \
    } while((bool)0)

#define V2_FLEXOP_CSI_SET_ADDR(h, kvp, dest) do { \
        union { int32_t i32v; uint64_t u64v; } high; \
        union { int32_t i32v; uint64_t u64v; } low; \
        low.u64v = ((uint64_t)dest & 0xFFFFFFFFull); \
        high.u64v = ((uint64_t)dest >> 32ull); \
        V2_FLEXOP_CSI_SET(h, kvp##_H, high.i32v); \
        V2_FLEXOP_CSI_SET(h, kvp##_L, low.i32v); \
    } while((bool)0)


/* -----------------------------------------------------------------------------
 * ioctl type specific flex io getter setter
 * -------------------------------------------------------------------------- */
#define V2D_SET_SEQ_OUTPUT(h, kvp, val) do { \
        if (h->numfields < h->maxfields) {  \
            h->op_keys |= V2D_KO_DECSEQ_##kvp; \
            h->fields[V2D_FO_##kvp] = val;  \
        } h->numfields++; \
    } while((bool)0)


#if defined(__cplusplus)
}
#endif

#endif // _VPU2_IOPARAM_FLEX_H_
