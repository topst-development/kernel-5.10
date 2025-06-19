/* SPDX-License-Identifier: GPL-2.0+ WITH Linux-syscall-note */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef _VPU2_IOCTL_CMDS_H_
#define _VPU2_IOCTL_CMDS_H_

// work-log: tcc_video_common.h
// Remark: inclue at least one kernel vpu header
// (for STC_XXX, mgrname, vputype, etc.)

/* -----------------------------------------------------------------------------
 * naming rule: acronyms
 * -----------------------------------------------------------------------------
 *
 * V2: vpu version 2 (top-level prefix)
 * |M|D|E|: vpu device marker (M: Mgr, D: Dec, E, Enc)
 * |FS|BS|: vpu memory marker (FS: Frame buffer Slot, BS: bitstream buffer)
 * |I|O|P: Input/Output Process
 *
 * USER|U: user   space ioctl caller
 * KERN|K: kernel space host application
 *
 * VUID: Vpu enc|dec multi driver UID (a.k.a. vpu instance index)
 *
 * REQ|RES: REQuest|RESult to  vpu hw
 *
 * DD: Device Driver
 * DS: Decode bitStream
 *
 * DRV: DRiVer
 * DEC: DECode
 * FRM: FRaMe
 * RNG: RiNG buffer for bitstream
 *
 * -------------------------------------------------------------------------- */

enum { V2_HW, V2_SW, V2_MAX };
enum { V2_IP, V2_OP, V2_IOP };
#if !defined(__cplusplus)
enum { Y, U, V };
#endif

// FIXME: R&R repositioning: d/d names are registered/exposed by
//        somewhere in the downstream header of vpu I/F layer

/* d/d name: vpu manager */
// FIXME: Manager D/D is mapped directly on VPU FW IP
// To connect this, U/S should know the target VPU FW IP per codec type
// : If FW Abstraction is supported, u/s and k/s would be more loosely-coupled.
#if defined(__cplusplus)
static constexpr char V2_DEV_MGR_FW_FHD[] = "/dev/vpu_dev_mgr";
static constexpr char V2_DEV_MGR_FW_JPG[] = "/dev/jpu_dev_mgr";
static constexpr char V2_DEV_MGR_FW_UHD[] = "/dev/vpu_4k_d2_dev_mgr";
static constexpr char V2_DEV_MGR_FW_HVC[] = "/dev/hevc_dev_mgr";
static constexpr char V2_DEV_MGR_FW_VP9[] = "/dev/vp9_dev_mgr";
#else
/* specified by: vpu fw type */
#define V2_DEV_MGR_FW_FHD   "/dev/vpu_dev_mgr";
#define V2_DEV_MGR_FW_JPG   "/dev/jpu_dev_mgr";
#define V2_DEV_MGR_FW_UHD   "/dev/vpu_4k_d2_dev_mgr";
#define V2_DEV_MGR_FW_HVC   "/dev/hevc_dev_mgr";
#define V2_DEV_MGR_FW_VP9   "/dev/vp9_dev_mgr";
#define V2_DEV_MGR_FW_NON   "/dev/V2_dummy_mgr"; // N/A
#endif

#define V2_DECNAME_MAXLEN (50)

/* specified by: codec type */
// FIXME: Open/Close strategy, licenced codec strategy
// : DIV3, EXT(RV), AVS|MVC(deprecated), SH263, THEORA(N/A)
#define V2_DEV_MGR__AVC         V2_DEV_MGR_FW_FHD
#define V2_DEV_MGR_CD_VC1       V2_DEV_MGR_FW_FHD
#define V2_DEV_MGR_CD_MPEG2     V2_DEV_MGR_FW_FHD
#define V2_DEV_MGR_CD_MPEG4     V2_DEV_MGR_FW_FHD
#define V2_DEV_MGR_CD_H263      V2_DEV_MGR_FW_FHD
#define V2_DEV_MGR_CD_DIV3      V2_DEV_MGR_FW_FHD
#define V2_DEV_MGR_CD_EXT       V2_DEV_MGR_FW_NON
#define V2_DEV_MGR_CD_AVS       V2_DEV_MGR_FW_NON
#define V2_DEV_MGR_CD_SH263     V2_DEV_MGR_FW_NON
#define V2_DEV_MGR_CD_MJPG      V2_DEV_MGR_FW_JPU
#define V2_DEV_MGR_CD_VP8       V2_DEV_MGR_FW_FHD
#define V2_DEV_MGR_CD_THEORA    V2_DEV_MGR_FW_NON
#define V2_DEV_MGR_CD_UHD       V2_DEV_MGR_FW_UHD
#define V2_DEV_MGR_CD_HEVC      V2_DEV_MGR_FW_HVC
#define V2_DEV_MGR_CD_VP9       V2_DEV_MGR_FW_VP9

/* d/d name: vpu decode agent */
#if defined(__cplusplus)
constexpr char V2_DEV_AGENT_DEC_0[] = "/dev/vpu_vdec";
constexpr char V2_DEV_AGENT_DEC_1[] = "/dev/vpu_vdec_ext";
constexpr char V2_DEV_AGENT_DEC_2[] = "/dev/vpu_vdec_ext2";
constexpr char V2_DEV_AGENT_DEC_3[] = "/dev/vpu_vdec_ext3";
#else
#define V2_DEV_AGENT_DEC_0  "/dev/vpu_vdec"
#define V2_DEV_AGENT_DEC_1  "/dev/vpu_vdec_ext"
#define V2_DEV_AGENT_DEC_2  "/dev/vpu_vdec_ext2"
#define V2_DEV_AGENT_DEC_3  "/dev/vpu_vdec_ext3"
#endif

#define DECLARE_V2_STATIC_DECDEV_LIST \
	char v2_dev_agent_dec_list[4] { \
		V2_DEV_AGENT_DEC_0, \
		V2_DEV_AGENT_DEC_1, \
		V2_DEV_AGENT_DEC_2, \
		V2_DEV_AGENT_DEC_3, \
	}

#define ASSIGN_V2_STATIC_DECDEV_NAME(X, IDX) do {\
	X = v2_dev_agent_dec_list[IDX]; \
} while (0)

// macro helper function
#define RESET_V2_POLLER(__poller, __fd) do { \
	__poller##[0].fd = __fd; \
	__poller##[0].events = POLLIN; \
	__poller##[0].revents = 0; \
} while (0)

// RULE: each ipcmd shoule be followed by the conterpart opcmd
#define V2_CHANGE_IP_TO_OP_CMD(__ipcmd) do { \
	__ipcmd += 1; \
} while (0)

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/* IOCTL flex meta print sentence */
#define V2_FLXMETA_PRINTFMT(flx) \
	"%s: ver(%u), ip(%#llx), op(%#llx), bl(%#x), res(%d), tot(%u), num(%u)", \
__func__, flx->version, flx->ip_keys, flx->op_keys, flx->boolean, \
flx->result, flx->maxfields, flx->numfields

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/* IOCTL cmds: Base Macro and Token Generator */
#define V2M_USER_IOP_BASE (0x400)
#define V2D_USER_IOP_BASE (0x400)
#define V2E_USER_IOP_BASE (0x400)

#define V2M_KERN_IOP_BASE (0x800)
#define V2D_KERN_IOP_BASE (0x800)
#define V2E_KERN_IOP_BASE (0x800)


#define V2M_USER_MAGIC(__opcode) \
	(V2M_USER_IOP_BASE + __opcode)
#define V2M_KERN_MAGIC(__opmacro, __opname) \
	(V2M_KERN_IOP_BASE + __opmacro##(##__opname##))
#define V2D_USER_MAGIC(__opcode) \
	(V2D_USER_IOP_BASE + __opcode)
#define V2D_KERN_MAGIC(__opmacro, __opname) \
	(V2D_KERN_IOP_BASE + __opmacro##(##__opname##))
#define V2E_USER_MAGIC(__opcode) \
	(V2E_USER_IOP_BASE + __opcode)
#define V2E_KERN_MAGIC(__opmacro, __opname) \
	(V2E_KERN_IOP_BASE + __opmacro##(##__opname##))

#define V2_CMD(__devmarker, __dir, __opname) \
	V2##__devmarker##_##__dir##P_##__opname
#define V2_CMD_K(__devmarker, __dir, __opname) \
	V2##__devmarker##_##__dir##P_##__opname##_KERN

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

////////////////////////////////////////////////////////////////////////////////
////////////////////////////////////////////////////////////////////////////////
/* IOCTL cmds: vpu mgr d/d */
// driver id: index in the en/decoder driver instances (vpu instance index)
//
#define V2M_IOP_VUID_QUERY  \
	V2M_USER_MAGIC(10)         // v1: VPU_CHECK_CODEC_STATUS
#define V2M_IOP_VUID_QUERY_KERN    V2M_KERN_MAGIC(V2M_IOP_VUID, QUERY)

#define V2M_IOP_VUID_ACQUIRE  \
	V2M_USER_MAGIC(11)         // v1: VPU_GET_INSTANCE_IDX
#define V2M_IOP_VUID_ACQUIRE_KERN  V2M_KERN_MAGIC(V2M_IOP_VUID, ACQUIRE)

#define V2M_IOP_VUID_RELEASE  \
	V2M_USER_MAGIC(12)         // v1: VPU_CLEAR_INSTANCE_IDX
#define V2M_IOP_VUID_RELEASE_KERN  V2M_KERN_MAGIC(V2M_IOP_VUID, RELEASE)

////////////////////////////////////////////////////////////////////////////////
/* IOCTL cmds: vpu dec[n] d/d */
// target host appl. layer - user/kernel space

#define V2D_OP_RES \
	V2D_USER_MAGIC(10)         // v1: V_DEC_GENERAL_RESULT
#define V2D_OP_RES_KERN            V2D_KERN_MAGIC(V2D_OP, RES)

/* helper: macro constant for calc. total num of ioctls */
#define V2D_IOP_START              V2D_OP_RES
#define V2D_IOP_START_KERN         (2D_OP_RNG_SETPOS_KERN)

/*
 * vpu2 decoder driver (V2D) control
 */
// V2_DEC_INI = (v1: VPU_SET_MEM_ALLOC_MODE + DEVICE_INITIALIZE + V_DEC_INIT)
#define V2D_IP_DRV_INI  \
	V2D_USER_MAGIC(11)         // v1: V_DEC_INIT
#define V2D_IP_DRV_INI_KERN        V2D_KERN_MAGIC(V2D_IP_DD, INI)

#define V2D_OP_DRV_INI  \
	V2D_USER_MAGIC(12)         // v1: V_DEC_INIT_RESULT
#define V2D_OP_INI_KERN            V2D_KERN_MAGIC(V2D_OP_DD, INI)

// V2_DEC_RST = (v1: V_DEC_CLOSE + V_DEC_FREE_MEMORY)
#define V2D_IP_DRV_RST \
	V2D_USER_MAGIC(13)         // v1: V_DEC_CLOSE
#define V2D_IP_DRV_RST_KERN        V2D_KERN_MAGIC(V2D_IP_DD, RST)

#define V2D_OP_DRV_RST \
	V2D_USER_MAGIC(14)         // v1: V_DEC_CLOSE
#define V2D_OP_DRV_RST_KERN        V2D_KERN_MAGIC(V2D_OP_DD, RST)

/*
 * vpu2 hw-buf registration (V2BR) control between vpu device driver and vpu lib.
 */
// V2_DEC_REG = (v1: V_DEC_REG_FRAME_BUFFER)
#define V2D_IP_DRV_REG \
	V2D_USER_MAGIC(15)         // v1: V_DEC_REG_FRAME_BUFFER
#define V2D_IP_DRV_REG_KERN        V2D_KERN_MAGIC(V2D_IP_DD, REG)

#define V2D_OP_DRV_REG \
	V2D_USER_MAGIC(16)         // v1: V_DEC_REG_FRAME_BUFFER
#define V2D_OP_DRV_REG_KERN        V2D_KERN_MAGIC(V2D_OP_DD, REG)

/*
 * vpu2 decoder process (V2DP) control
 */
// V2_DEC_DECODE_HEADER = (v1: V_DEC_SEQ_HEADER + V_DEC_REG_FRAME_BUFFER)
#define V2D_IP_DEC_SEQDATA \
	V2D_USER_MAGIC(17)         // v1: V_DEC_SEQ_HEADER
#define V2D_IP_DEC_SEQDATA_KERN    V2D_KERN_MAGIC(V2D_IP_DS, SEQDATA)

#define V2D_OP_DEC_SEQDATA \
	V2D_USER_MAGIC(18)         // v1: V_DEC_SEQ_HEADER_RESULT
#define V2D_OP_DEC_SEQDATA_KERN    V2D_KERN_MAGIC(V2D_OP_DS, SEQDATA)

#define V2D_IP_DEC_FRMDATA \
	V2D_USER_MAGIC(19)         // v1: V_DEC_DECODE
#define V2D_IP_DEC_FRMDATA_KERN    V2D_KERN_MAGIC(V2D_IP_DS, FRMDATA)

// V2D_USER_MAGIC(20) is for dual input buffer internally

#define V2D_OP_DEC_FRMDATA \
	V2D_USER_MAGIC(21)         // v1: V_DEC_DECODE_RESULT
#define V2D_OP_DEC_FRMDATA_KERN    V2D_KERN_MAGIC(V2D_OP_DS, FRMDATA)

/*
 * vpu Frame Buffer index (FRMSLOT) Control
 */
#define V2D_IP_FRM_CLEAR \
	V2D_USER_MAGIC(22)         // v1: V_DEC_BUF_FLAG_CLEAR
#define V2D_IP_FRM_CLEAR_KERN      V2D_KERN_MAGIC(V2D_IP_FS, CLEAR)

#define V2D_OP_FRM_CLEAR \
	V2D_USER_MAGIC(23)         // v1: V_DEC_GENERAL_RESULT
#define V2D_OP_FRM_CLEAR_KERN      V2D_KERN_MAGIC(V2D_OP_FS, CLEAR)

#define V2D_IP_FRM_FLUSH \
	V2D_USER_MAGIC(24)         // v2: V_DEC_BUF_FLAG_CLEAR for all
#define V2D_IP_FRM_FLUSH_KERN      V2D_KERN_MAGIC(V2D_IP_FS, FLUSH)

#define V2D_OP_FRM_FLUSH \
	V2D_USER_MAGIC(25)         // v1: V_DEC_GENERAL_RESULT
#define V2D_OP_FRM_FLUSH_KERN      V2D_KERN_MAGIC(V2D_OP_FS, FLUSH)

#define V2D_IP_FRM_DRAIN \
	V2D_USER_MAGIC(26)         // v1: V_DEC_FLUSH_OUTPUT
#define V2D_IP_FRM_DRAIN_KERN      V2D_KERN_MAGIC(V2D_IP_FS, DRAIN)

#define V2D_OP_FRM_DRAIN \
	V2D_USER_MAGIC(27)         // v1: V_DEC_FLUSH_OUTPUT_RESULT
#define V2D_OP_FRM_DRAIN_KERN      V2D_KERN_MAGIC(V2D_OP_FS, DRAIN)

/*
 * vpu bitstream buffer (BSBUFF) control (ringbuffer mode only)
 */
#define V2D_IP_RNG_GETPOS \
	V2D_USER_MAGIC(28)         // v1: V_GET_RING_BUFFER_STATUS
#define V2D_IP_RNG_GETPOS_KERN     V2D_KERN_MAGIC(V2D_IP_BS, GETPOS)

#define V2D_OP_RNG_GETPOS \
	V2D_USER_MAGIC(29)         // v1: V_GET_RING_BUFFER_STATUS_RESULT
#define V2D_OP_RNG_GETPOS_KERN     V2D_KERN_MAGIC(V2D_OP_BS, GETPOS)

#define V2D_IP_RNG_SETPOS \
	V2D_USER_MAGIC(30)         // v1: V_DEC_UPDATE_RINGBUF_WP
#define V2D_IP_RNG_SETPOS_KERN     V2D_KERN_MAGIC(V2D_IP_BS, SETPOS)

#define V2D_OP_RNG_SETPOS \
	V2D_USER_MAGIC(31)         // v1: V_DEC_UPDATE_RINGBUF_WP_RESULT
#define V2D_OP_RNG_SETPOS_KERN     V2D_KERN_MAGIC(V2D_OP_BS, SETPOS)

/*
 * vpu2 decoder memory (V2D) control
 */
#define V2D_IP_HWBUF_ASGN  \
	V2D_USER_MAGIC(32)         // v1: V_DEC_ALLOC_MEMORY
#define V2D_IP_HWBUF_ASGN_KERN      V2D_KERN_MAGIC(V2D_IP_HWBUF, ASGN)

#define V2D_IP_HWBUF_FREE  \
	V2D_USER_MAGIC(33)         // v1: V_DEC_FREE_MEMORY
#define V2D_IP_HWBUF_FREE_KERN      V2D_KERN_MAGIC(V2D_IP_HWBUF, FREE)


/* helper: macro constant for calc. total num of ioctls */
#define V2D_IOP_END                V2D_OP_RNG_SETPOS
#define V2D_IOP_END_KERN           V2D_OP_RNG_SETPOS_KERN

////////////////////////////////////////////////////////////////////////////////
/* IOCTL index: vpu dec[n] d/d : index of flex array */
#define V2D_IOP_MAX                (V2D_IOP_END - V2D_IOP_START)
#define V2D_IOP_MAX_KERN           (V2D_IOP_END_KERN - V2D_IOP_START_KERN)

#define V2D_FINDEX(v2_ioctl)       (v2_ioctl - V2D_OP_RES)

#endif // _VPU2_IOCTL_CMDS_H_
