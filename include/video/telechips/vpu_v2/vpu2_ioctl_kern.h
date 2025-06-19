/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _VPU2_IOCTL_CMDS_H_
#define _VPU2_IOCTL_CMDS_H_

#include "vpu2_ioctl_uapi.h"

// work-log: tcc_video_common.h

/*
 * [Acronyms]
 *
 * V2:  Vpu version 2 interface
 *
 * |M|D|E|: vpu device marker (M: Mgr, D: Dec, E, Enc)
 * |FS|BS|: vpu memory marker (FS: Frame buffer Slot, BS: BitStream buffer)
 *
 * VUID: Vpu enc|dec multi driver UID (a.k.a. vpu instance index)
 *
 * IOP: Input/Output Process
 *  IP: Input Process
 *  OP: Output Process, OPeration
 *
 * USER|U: USER   space ioctl caller
 * KERN|K: KERNel space host application
 *
 * REQ: REQuest to  vpu hw
 * RES: RESult from vpu hw
 *
 * DD: Device Driver
 * DS: Decode bitStream
 * FS: Frame Slot
 * BS: BitStream
 *
 * DRV: DRiVer
 * DEC: DEcode
 * FRM: FRaMe
 * RNG: RiNG buffer for bitstream
 *
 */

#define V2M_KERN_IOP_BASE (0x800)
#define V2D_KERN_IOP_BASE (0x800)
#define V2E_KERN_IOP_BASE (0x800)


#define V2M_KERN_MAGIC(__opmacro, __opname) \
	(V2M_KERN_IOP_BASE + __opmacro##(##__opname##))
#define V2D_KERN_MAGIC(__opmacro, __opname) \
	(V2D_KERN_IOP_BASE + __opmacro##(##__opname##))
#define V2E_KERN_MAGIC(__opmacro, __opname) \
	(V2E_KERN_IOP_BASE + __opmacro##(##__opname##))

#define V2_CMD_K(__devmarker, __dir, __opname) \
	V2##__devmarker##_##__dir##P_##__opname##_KERN


////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/* IOCTL cmds: vpu mgr d/d */
// driver id: index in the en/decoder driver instances (vpu instance index)
//
#define V2M_IOP_VUID_QUERY_KERN    V2M_KERN_MAGIC(V2M_IOP_VUID, QUERY)
#define V2M_IOP_VUID_ACQUIRE_KERN  V2M_KERN_MAGIC(V2M_IOP_VUID, ACQUIRE)
#define V2M_IOP_VUID_RELEASE_KERN  V2M_KERN_MAGIC(V2M_IOP_VUID, RELEASE)

////////////////////////////////////////////////////////////////////////////////
/* IOCTL cmds: vpu dec[n] d/d */
// target host appl. layer - user/kernel space
#define V2D_OP_RES_KERN            V2D_KERN_MAGIC(V2D_OP, RES)

/*
 * vpu2 decoder driver (V2D) control
 */
// V2_DEC_BRINGUP = (v1: VPU_SET_MEM_ALLOC_MODE + DEVICE_INITIALIZE + V_DEC_INIT)
#define V2D_IP_DRV_BRINGUP_KERN    V2D_KERN_MAGIC(V2D_IP_DD, BRINGUP)
#define V2D_OP_BRINGUP_KERN        V2D_KERN_MAGIC(V2D_OP_DD, BRINGUP)

// V2_DEC_CLEANUP = (v1: V_DEC_CLOSE + V_DEC_FREE_MEMORY)
#define V2D_IP_DRV_CLEANUP_KERN    V2D_KERN_MAGIC(V2D_IP_DD, CLEANUP)
#define V2D_OP_CLEANUP_KERN        V2D_KERN_MAGIC(V2D_OP_DD, CLEANUP)

/*
 * vpu2 decoder process (V2DP) control
 */
// V2_DEC_DECODE_HEADER = (v1: V_DEC_SEQ_HEADER + V_DEC_REG_FRAME_BUFFER)
#define V2D_IP_DEC_SEQDATA_KERN    V2D_KERN_MAGIC(V2D_IP_DS, SEQDATA)
#define V2D_OP_DEC_SEQDATA_KERN    V2D_KERN_MAGIC(V2D_OP_DS, SEQDATA)
#define V2D_IP_DEC_FRMDATA_KERN    V2D_KERN_MAGIC(V2D_IP_DS, FRMDATA)
#define V2D_OP_DEC_FRMDATA_KERN    V2D_KERN_MAGIC(V2D_OP_DS, FRMDATA)

/*
 * vpu frame buffer index (FRMSLOT) control
 */
#define V2D_IP_FRM_CLEAR_KERN      V2D_KERN_MAGIC(V2D_IP_FS, CLEAR)
#define V2D_OP_FRM_CLEAR_KERN      V2D_KERN_MAGIC(V2D_OP_FS, CLEAR)

#define V2D_IP_FRM_FLUSH_KERN      V2D_KERN_MAGIC(V2D_IP_FS, FLUSH)
#define V2D_OP_FRM_FLUSH_KERN      V2D_KERN_MAGIC(V2D_OP_FS, FLUSH)

#define V2D_IP_FRM_DRAIN_KERN      V2D_KERN_MAGIC(V2D_IP_FS, DRAIN)
#define V2D_OP_FRM_DRAIN_KERN      V2D_KERN_MAGIC(V2D_OP_FS, DRAIN)

/*
 * vpu bitstream buffer (BSBUFF) control (ringbuffer mode only)
 */
#define V2D_IP_RNG_GETPOS_KERN     V2D_KERN_MAGIC(V2D_IP_BS, GETPOS)
#define V2D_OP_RNG_GETPOS_KERN     V2D_KERN_MAGIC(V2D_OP_BS, GETPOS)

#define V2D_IP_RNG_SETPOS_KERN     V2D_KERN_MAGIC(V2D_IP_BS, SETPOS)
#define V2D_OP_RNG_SETPOS_KERN     V2D_KERN_MAGIC(V2D_OP_BS, SETPOS)

#endif // _VPU2_IOCTL_CMDS_H_
