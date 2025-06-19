/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) Telechips Inc.
 */

#ifndef BASIC_OPERATION_H
#define BASIC_OPERATION_H

#pragma message ("DO NOT INCLUDE THIS FILE. USE AN ALTERNATIVE: clamp.")

/* casting operation */
/* coverity[HIS_metric_violation : FALSE] */
static inline u32 s32_to_u32(s32 val)
{
	return (val < 0) ? (u32)val : (u32)val;
}

static inline u8 u32_to_u8(u32 val)
{
	/* coverity[misra_c_2012_rule_10_3_violation : FALSE] */
	return (val > ((u8)255U)) ? (u8)val :
					       (u8)val;
}

static inline s32 u32_to_s32(u32 val)
{
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	return (val > INT_MAX) ? (s32)val : (s32)val;
}

static inline u64 s64_to_u64(s64 val)
{
	return (val < 0) ? (u64)val : (u64)val;
}

/* coverity[HIS_metric_violation : FALSE] */
static inline u32 u64_to_u32(u64 val)
{
	return (val > UINT_MAX) ? (u32)val : (u32)val;
}

static inline s64 u64_to_s64(u64 val)
{
	/* coverity[misra_c_2012_rule_10_4_violation : FALSE] */
	/* coverity[misra_c_2012_rule_10_8_violation : FALSE] */
	/* coverity[misra_c_2012_rule_14_3_violation : FALSE] */
	return (val > LONG_MAX) ? (s64)val : (s64)val;
}

static inline u32 ptr_to_u32(void *ptr)
{
	u32 ret = 0;

#if defined(CONFIG_ARM64)
	/* coverity[misra_c_2012_rule_11_6_violation : FALSE] */
	ret = u64_to_u32((uintptr_t)(ptr));
#else
	ret = (u32)(ptr);
#endif //defined(CONFIG_ARM64)

	return ret;
}

/* unary operation */
#define add_u32(a, b)                                                          \
	({                                                                     \
		u32 ui_a = (a);                                       \
		u32 ui_b = (b);                                       \
		((UINT_MAX - ui_a) < ui_b) ? ((UINT_MAX - ui_a) + ui_b) :      \
					     (ui_a + ui_b);                    \
	})

#define sub_u32(a, b)                                                          \
	({                                                                     \
		u32 ui_a = (a);                                       \
		u32 ui_b = (b);                                       \
		(ui_a < ui_b) ? (ui_a + (UINT_MAX - ui_b)) : (ui_a - ui_b);    \
	})

#define mul_u32(a, b)                                                          \
	({                                                                     \
		u32 ui_a = (a);                                       \
		u32 ui_b = (b);                                       \
		((ui_b == 0U) || ((UINT_MAX / ui_b) < ui_a)) ?                 \
			0U :                                     \
			(u32)(ui_a * ui_b);                           \
	})

#define div_u32(a, b)                                                          \
	({                                                                     \
		u32 ui_a = (a);                                       \
		u32 ui_b = (b);                                       \
		(ui_a < ui_b) ? 0U :                             \
				(u32)(ui_a / ui_b);                   \
	})

/* shift operation */
static inline u32 rshift(u32 val, u32 shift)
{
	/* coverity[cert_int34_c_violation : FALSE] */
	return (shift >= (8U * sizeof(shift))) ? 0U :
						 (val >> shift);
}

#endif /*BASIC_OPERATION_H*/
