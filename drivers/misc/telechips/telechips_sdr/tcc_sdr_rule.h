/****************************************************************************
 * Copyright (C) 2022 Telechips Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
 * for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 ****************************************************************************/

#ifndef TCC_SDR_RULE_H
#define TCC_SDR_RULE_H
#include <linux/kernel.h>

#define info_int(f, ...)		(void)pr_info(f "%s", __VA_ARGS__)
#define err_int(f, ...)			(void)pr_err(f "%s", __VA_ARGS__)

#define tcc_audio_info(...)		info_int(__VA_ARGS__, "\n")
#define tcc_audio_dbg(...)		info_int(__VA_ARGS__, "\n")
#define tcc_audio_err(...)		err_int(__VA_ARGS__, "\n")

#ifdef CONFIG_64BIT
#define TCC_AUDIO_INT_MAX		(0x7FFFFFFF)
#define TCC_AUDIO_INT_MIN		(-TCC_AUDIO_INT_MAX-1)
#define TCC_AUDIO_LONG_MAX_UL	(~0UL>>1)
#define TCC_AUDIO_LONG_MAX		(0x7FFFFFFFFFFFFFFF)
#else
#define TCC_AUDIO_INT_MAX		(0x7FFFFFFF)
#define TCC_AUDIO_INT_MIN		(-TCC_AUDIO_INT_MAX-1)
#define TCC_AUDIO_LONG_MAX_UL	(~0UL>>1)
#define TCC_AUDIO_LONG_MAX		TCC_AUDIO_INT_MAX
#endif
/* Returns the number of set bits */
static inline uint32_t popcount(uint32_t num) {
	uint32_t precision = 0;
	while (num != 0U) {
		if (((num % 2U) == 1U) && (precision < UINT_MAX)) {
			precision++;
		}
		num >>= 1;
	}
	return precision;
}

#define PRECISION(umax_value) popcount(umax_value)


//CERT C INT30-C (Addition)
static inline uint32_t ui_add(uint32_t ui_a, uint32_t ui_b)
{
	uint32_t ret = 0;
	if((UINT_MAX - ui_a) < ui_b){
		tcc_audio_err("Unsigned integer + operaion is wrap!!!");
	} else {
		ret = ui_a + ui_b;
	}
	return ret;
}

//CERT C INT30-C (Subtraction)
static inline uint32_t ui_sub(uint32_t ui_a, uint32_t ui_b)
{
	uint32_t ret = 0;
	if (ui_a < ui_b) {
		tcc_audio_err("Unsigned integer - operaion is wrap!!!");
	} else {
		ret = ui_a - ui_b;
	}
	return ret;
}


//CERT C INT30-C ui to ui (Multiplication)
static inline uint32_t ui_to_ui_mul(uint32_t ui_a, uint32_t ui_b)
{
	uint32_t ret = 0;
	if ((ui_b == 0U) || (ui_a > (UINT_MAX / ui_b))) {
		tcc_audio_err("Unsigned integer * operaion is wrap!!!");
	} else {
		ret = ui_a * ui_b;
	}
	return ret;
}

//CERT C INT30-C ui to ull (Multiplication)
static inline unsigned long long ui_to_ull_mul(uint32_t ui_a, uint32_t ui_b)
{
	unsigned long long ret = 0;
	if ((ui_b == 0U) || (ui_a > (ULLONG_MAX / ui_b))) {
		tcc_audio_err("Unsigned integer * operaion is wrap!!!");
	} else {
		ret = (unsigned long long)ui_a * (unsigned long long)ui_b;
	}
	return ret;
}

//CERT C INT31-C long to int (Signed, Loss of Precision)
static inline int sl_to_si(long in)
{
	int out = 0;
#ifdef CONFIG_64BIT
	if ((in >= TCC_AUDIO_INT_MIN) && (in <= TCC_AUDIO_INT_MAX)) {
		out = (int)in;
	}
#else
	out = (int)in;
#endif
	return out;
}

//CERT C INT31-C ulong to uint (Unsigned, Loss of Precision)
static inline uint32_t ul_to_ui(unsigned long in)
{
	uint32_t out = 0;
#ifdef CONFIG_64BIT
	if (in <= UINT_MAX) {
		out = (uint32_t)in;
	}
#else
	out = (uint32_t)in;
#endif
	return out;
}

//CERT C INT02-C ull to uint (Unsigned, Loss of Precision)
static inline uint32_t ull_to_ui(unsigned long long in)
{
	uint32_t out = 0;
	if (in <= UINT_MAX) {
		out = (uint32_t)in;
	}
	return out;
}

//CERT C EXP37-C uint64 to uint32 (Unsigned, Loss of Precision)
static inline uint32_t ui64_to_ui(uint64_t in)
{
	uint32_t out = 0;
	if (in <= UINT_MAX) {
		out = (uint32_t)in;
	}
	return out;
}

//CERT C INT31-C int to uint (Signed to Unsigned)
static inline uint32_t si_to_ui(int in)
{
	uint32_t out = 0;
	if (in >= 0) {
		out = (uint32_t)in;
	}
	return out;
}


//CERT C INT31-C long to uint (Signed to Unsigned)
static inline uint32_t sl_to_ui(long in)
{
	uint32_t out = 0;
	if (in >= 0) {
		out = (uint32_t)in;
	}
	return out;
}

//CERT C INT02-C long to uint64_t (Signed to Unsigned)
static inline uint64_t sl_to_ui64(long in)
{
	uint64_t out = 0;
	if (in >= 0) {
		out = (uint64_t)in;
	}
	return out;
}

//CERT C INT31-C int to ulong (Signed to Unsigned)
static inline unsigned long si_to_ul(int in)
{
	unsigned long out = 0;
	if (in >= 0) {
		out = (unsigned long)in;
	}
	return out;
}

//CERT C INT02-C long to ulong (Signed to Unsigned)
static inline unsigned long sl_to_ul(long in)
{
	unsigned long out = 0;
	if (in >= 0) {
		out = (unsigned long)in;
	}
	return out;
}

//CERT C INT02-C ulong to long (Unsigned, Loss of Precision)
static inline long ul_to_sl(unsigned long in)
{
	long out = 0;
	if (in <= TCC_AUDIO_LONG_MAX_UL) {
		out = (long)in;
	}
	return out;
}

//CERT C INT31-C unsigned int to signed int (Unsigned to Signed)
static inline int ui_to_si(uint32_t in)
{
	int out = 0;
	if (in <= si_to_ui(TCC_AUDIO_INT_MAX)) {
		out = (int)in;
	}
	return out;
}

//CERT C INT02-C ulong to signed int (Unsigned to Signed)
static inline int ul_to_si(unsigned long in)
{
	int out = 0;
	if (in <= si_to_ul(TCC_AUDIO_INT_MAX)) {
		out = (int)in;
	}
	return out;
}

//CERT C INT34-C (Left Shift, Unsigned Type)
static inline uint32_t ui_lshift(uint32_t ui_a, uint32_t ui_b)
{
	uint32_t ret = 0;
	if (ui_b < PRECISION(UINT_MAX)) {
		ret = ui_a << ui_b;
	}
	return ret;
}

//CERT C INT34-C (Right Shift, Unsigned Type)
static inline uint32_t ui_rshift(uint32_t ui_a, uint32_t ui_b)
{
	uint32_t ret = 0;
	if (ui_b < PRECISION(UINT_MAX)) {
		ret = ui_a >> ui_b;
	}
	return ret;
}

//CERT C INT36-C ulong to ptr(unsigned int *) (Integer to Pointer)
static inline unsigned int * ul_to_ptr(unsigned long in)
{
	volatile uintptr_t iptr = in;

	unsigned int *ptr = (unsigned int *)iptr;

	return ptr;
}
#endif /*TCC_SDR_RULE_H*/
