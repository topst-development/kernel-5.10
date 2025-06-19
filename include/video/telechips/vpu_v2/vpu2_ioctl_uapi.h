/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _VPU2_IOCTL_CMDS_H_
#define _VPU2_IOCTL_CMDS_H_

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

#define V2M_USER_IOP_BASE (0x400)
#define V2D_USER_IOP_BASE (0x400)
#define V2E_USER_IOP_BASE (0x400)

#define V2M_USER_MAGIC(__opcode) \
	(V2M_USER_IOP_BASE + __opcode)
#define V2D_USER_MAGIC(__opcode) \
	(V2D_USER_IOP_BASE + __opcode)
#define V2E_USER_MAGIC(__opcode) \
	(V2E_USER_IOP_BASE + __opcode)

#define V2_CMD(__devmarker, __dir, __opname) \
	V2##__devmarker##_##__dir##P_##__opname

#define V2M_IOP(__opname)       V2M_IOP_##__opname
#define V2M_IOP_VUID(__opname)  V2M_IOP_VUID_##__opname

#define V2D_IOP(__opname)       V2D_IOP_##__opname
#define V2D_IP(__opname)        V2D_IP_##__opname
#define V2D_OP(__opname)        V2D_OP_##__opname

#define V2D_IP_DD(__opname)     V2D_IP_DRV_##__opname
#define V2D_OP_DD(__opname)     V2D_OP_DRV_##__opname
#define V2D_IP_DS(__opname)     V2D_IP_DEC_##__opname
#define V2D_OP_DS(__opname)     V2D_OP_DEC_##__opname
#define V2D_IP_FS(__opname)     V2D_IP_FRM_##__opname
#define V2D_OP_FS(__opname)     V2D_OP_FRM_##__opname
#define V2D_IP_BS(__opname)     V2D_IP_RNG_##__opname
#define V2D_OP_BS(__opname)     V2D_OP_RNG_##__opname

#define V2_IP    (0)
#define V2_OP    (1)
#define V2_IOP   (2)

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/* IOCTL cmds: vpu mgr d/d */
// driver id: index in the en/decoder driver instances (vpu instance index)
//
#define V2M_IOP_VUID_QUERY  \
	V2M_USER_MAGIC(10)         // v1: VPU_CHECK_CODEC_STATUS

#define V2M_IOP_VUID_ACQUIRE  \
	V2M_USER_MAGIC(11)         // v1: VPU_GET_INSTANCE_IDX

#define V2M_IOP_VUID_RELEASE  \
	V2M_USER_MAGIC(12)         // v1: VPU_CLEAR_INSTANCE_IDX

////////////////////////////////////////////////////////////////////////////////
/* IOCTL cmds: vpu dec[n] d/d */
// target host appl. layer - user/kernel space

#define V2D_OP_RES \
	V2D_USER_MAGIC(10)         // v1: V_DEC_GENERAL_RESULT

/*
 * vpu2 decoder driver (V2D) control
 */
// V2_DEC_BRINGUP = (v1: VPU_SET_MEM_ALLOC_MODE + DEVICE_INITIALIZE + V_DEC_INIT)
#define V2D_IP_DRV_BRINGUP  \
	V2D_USER_MAGIC(11)         // v1: V_DEC_INIT

#define V2D_OP_DRV_BRINGUP  \
	V2D_USER_MAGIC(12)         // v1: V_DEC_INIT_RESULT

// V2_DEC_CLEANUP = (v1: V_DEC_CLOSE + V_DEC_FREE_MEMORY)
#define V2D_IP_DRV_CLEANUP \
	V2D_USER_MAGIC(13)         // v1: V_DEC_CLOSE

#define V2D_OP_CLEANUP V2D_OP(RES)

/*
 * vpu2 decoder process (V2DP) control
 */
// V2_DEC_DECODE_HEADER = (v1: V_DEC_SEQ_HEADER + V_DEC_REG_FRAME_BUFFER)
#define V2D_IP_DEC_SEQDATA \
	V2D_USER_MAGIC(14)         // v1: V_DEC_SEQ_HEADER

#define V2D_OP_DEC_SEQDATA \
	V2D_USER_MAGIC(15)         // v1: V_DEC_SEQ_HEADER_RESULT

#define V2D_IP_DEC_FRMDATA \
	V2D_USER_MAGIC(16)         // v1: V_DEC_DECODE

#define V2D_OP_DEC_FRMDATA \
	V2D_USER_MAGIC(17)         // v1: V_DEC_DECODE_RESULT

/*
 * vpu frame buffer index (FRMSLOT) control
 */
#define V2D_IP_FRM_CLEAR \
	V2D_USER_MAGIC(18)         // v1: V_DEC_BUF_FLAG_CLEAR

#define V2D_OP_FRM_CLEAR           V2D_OP(RES)

#define V2D_IP_FRM_FLUSH \
	V2D_USER_MAGIC(19)         // v2: V_DEC_BUF_FLAG_CLEAR for all

#define V2D_OP_FRM_FLUSH           V2D_OP(RES)

#define V2D_IP_FRM_DRAIN \
	V2D_USER_MAGIC(20)         // v1: V_DEC_FLUSH_OUTPUT

#define V2D_OP_FRM_DRAIN \
	V2D_USER_MAGIC(21)         // v1: V_DEC_FLUSH_OUTPUT_RESULT

/*
 * vpu bitstream buffer (BSBUFF) control (ringbuffer mode only)
 */
#define V2D_IP_RNG_GETPOS \
	V2D_USER_MAGIC(22)         // v1: V_GET_RING_BUFFER_STATUS

#define V2D_OP_RNG_GETPOS \
	V2D_USER_MAGIC(23)         // v1: V_GET_RING_BUFFER_STATUS_RESULT

#define V2D_IP_RNG_SETPOS \
	V2D_USER_MAGIC(24)         // v1: V_DEC_UPDATE_RINGBUF_WP

#define V2D_OP_RNG_SETPOS \
	V2D_USER_MAGIC(25)         // v1: V_DEC_UPDATE_RINGBUF_WP_RESULT

#endif // _VPU2_IOCTL_CMDS_H_
