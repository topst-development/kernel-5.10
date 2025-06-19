/* SPDX-License-Identifier: GPL-2.0-or-later
 *
 * tcc_math.h
 *
 * Copyright (C) 2021 Telechips Inc.
 * Authors:
 *	Jayden Kim
 *
 * This program is free software; you can redistribute  it and/or modify it
 * under  the terms of  the GNU General  Public License as published by the
 * Free Software Foundation;  either version 2 of the  License, or (at your
 * option) any later version.
 */

#ifndef TELECHIPS_MATH_API_H
#define TELECHIPS_MATH_API_H

/* OLD defines */
#define tcc_safe_ulong2uint(in) \
({	unsigned long ts_ulin = (in);		\
	unsigned int ts_uout;			\
	const unsigned int ts_umax = UINT_MAX;	\
						\
	if (ts_ulin > ts_umax) {		\
		ts_uout = (unsigned int)ts_umax;\
	} else {				\
		ts_uout = (unsigned int)ts_ulin;\
	}					\
	ts_uout;				})

/* unsigned int to int */
#define tcc_safe_uint2int(in) \
({	unsigned int ts_uin = (in);		\
	int ts_iout;				\
	const unsigned int ts_imax =		\
		(UINT_MAX >> 1); 		\
						\
	if (ts_uin > ts_imax) {			\
		ts_iout = (int)ts_imax;		\
	} else {				\
		ts_iout = (int)ts_uin;		\
	}					\
	ts_iout;				})

/* int to unsigned int */
#define tcc_safe_int2uint(in) \
({	unsigned int ts_uout;			\
	int ts_iin = (in);			\
						\
	ts_uout = (ts_iin < 0) ? 0U : (unsigned int)ts_iin;\
	ts_uout;	})

/* uint64_t to unsigned int */
#define tcc_safe_u642uint(in) \
({	uint64_t ts_uin64 = (in);		\
	unsigned int ts_uout;			\
	const unsigned int ts_umax = UINT_MAX;	\
						\
	ts_uin64 =				\
		(ts_uin64 > ts_umax) ? ts_umax : ts_uin64;\
	ts_uout = (unsigned int)ts_uin64;		\
	ts_uout;					})

/*
 * This macro defines pluse between int
 * tcc_safe_int_pluse(si_a, si_b)
 * ts_iout =  si_a + si_b
 * If si_a + si_b is larger than INT_MAX, the ts_iout is INT_MAX.
 * If si_a + si_b is less than INT_MIN, the ts_iout is INT_MIN.
 */
#define tcc_safe_int_pluse(si_a, si_b)				\
({	int ts_iout;						\
	int ts_ia = (si_a);					\
	int ts_ib = (si_b);					\
	const unsigned int ts_uimax = (UINT_MAX >> 1);		\
	const int ts_imax = (int)ts_uimax;			\
	const int ts_imin = (-ts_imax -1);			\
								\
	if ((ts_ib > 0) && (ts_ia > (ts_imax - ts_ib))) {	\
		ts_iout = ts_imax;				\
	} else if ((ts_ib < 0) && (ts_ia < (ts_imin - ts_ib))) {\
		ts_iout = ts_imin;				\
	} else {						\
		ts_iout = ts_ia + ts_ib;			\
	}							\
	ts_iout;						})


/*
 * This macro defines minus between int
 * tcc_safe_int_minus(si_a, si_b)
 * ts_iout =  si_a - si_b
 * If si_a - si_b is larger than INT_MAX, the ts_iout is INT_MAX.
 * If si_a - si_b is less than INT_MIN, the ts_iout is INT_MIN.
 */
#define tcc_safe_int_minus(si_a, si_b)				\
({	unsigned int ts_iout;					\
	unsigned int ts_ia = (si_a);				\
	unsigned int ts_ib = (si_b);				\
	const unsigned int ts_uimax = (UINT_MAX >> 1);		\
	const int ts_imax = (int)ts_uimax;			\
	const int ts_imin = (-ts_imax -1);			\
								\
	if (ts_ib > 0 && (si_a < (ts_imin + ts_ib))) {		\
		ts_iout = ts_imin;				\
	} else if (ts_ib < 0 && si_a > (ts_imax + ts_ib)) {	\
		ts_iout = ts_imax;				\
	} else {						\
		ts_iout = ts_ia - ts_ib;			\
	}							\
	ts_iout; 						})

#define tcc_safe_uint_pluse(in_a, in_b)		\
({	unsigned int ts_uout;			\
	unsigned int ts_ua = (in_a);		\
	unsigned int ts_ub = (in_b);		\
	const unsigned int ts_umax = UINT_MAX;	\
						\
	ts_uout = ((ts_umax - ts_ua) < ts_ub) ? ts_umax : (ts_ua + ts_ub);\
	ts_uout;							})
/*
 * This macro defines minus between unsigned int
 * tcc_safe_uint_minus(in_a, inb)
 * ts_uout =  in_a - in_b
 * If in_a is less than in_b, the ts_uout is 0.
 */
#define tcc_safe_uint_minus(in_a, in_b)		\
({	unsigned int ts_uout;			\
	unsigned int ts_ua = (in_a);		\
	unsigned int ts_ub = (in_b);		\
						\
	ts_uout = (ts_ua < ts_ub) ? 0U : (ts_ua - ts_ub);\
	ts_uout;				})

#define tcc_safe_uint_mul(in_a, in_b)		\
({	unsigned int ts_uout;			\
	unsigned int ts_ua = (in_a);		\
	unsigned int ts_ub = (in_b);		\
	const unsigned int ts_umax = UINT_MAX;	\
						\
	if ((ts_ua == 0U) || (ts_ub == 0U)) {		\
		ts_uout = 0U;				\
	} else if (ts_ua == 1U) {			\
		ts_uout = ts_ub;			\
	} else if (ts_ub == 1U) {			\
		ts_uout = ts_ua;			\
	} else {					\
		ts_uout = ((ts_umax / ts_ua) < ts_ub) ? \
			  ts_umax : (ts_ua * ts_ub);	\
	}						\
	ts_uout;					})

#define tcc_safe_ulong_mul(in_a, in_b)		\
({	unsigned long ts_ulout;			\
	unsigned long ts_ula = (in_a);		\
	unsigned long ts_ulb = (in_b);		\
	const unsigned long ts_umax = ULONG_MAX;\
						\
	if ((ts_ula == 0UL) || (ts_ulb == 0UL)) {	\
		ts_ulout = 0UL;				\
	} else if (ts_ula == 1UL) {			\
		ts_ulout = ts_ulb;			\
	} else if (ts_ulb == 1UL) {			\
		ts_ulout = ts_ula;			\
	} else {					\
		ts_ulout = ((ts_umax / ts_ula) < ts_ulb) ? \
			  ts_umax : (ts_ula * ts_ulb);	\
	}						\
	ts_ulout;					})

/* This api works only for positive int */
#define tcc_safe_div_round_up_int(in_a, in_b)	\
({									\
		int ts_iout;						\
		const unsigned int ts_uimax = (UINT_MAX >> 1);		\
		const int ts_imax = (int)ts_uimax;			\
									\
		if (((in_a) < 0) || ((in_b) < 0)) {			\
			ts_iout = 0;					\
		} else {						\
			ts_iout = ((ts_imax - (in_a)) < (in_b)) ? 	\
				(ts_imax - (in_b)) : (in_a);		\
			ts_iout = DIV_ROUND_UP((in_a), (in_b));		\
		}							\
		ts_iout;						})

/* New defines */
/*
 * This macro checks if an unsigned long value is greater than the unsigned
 * int maximum value.
 */
 
 /*
 * This macro checks if an unsigned long value is greater than the unsigned
 * int maximum value.
 */
#define tcc_math_ulong_gt_uintmax(in) 		\
({	unsigned long ts_ulin = (in);		\
	bool ts_bout = (bool)false;		\
	const unsigned int ts_umax = UINT_MAX;	\
	const unsigned long ts_ul_umax =	\
		(const unsigned long)ts_umax;	\
						\
	if (ts_ulin > ts_ul_umax) {		\
		ts_bout = (bool)true;		\
	}					\
	ts_bout;				})

/*
 * This macro checks if an uint64_t value is greater than the unsigned
 * int maximum value.
 */
#define tcc_math_u64_gt_uintmax(in) 		\
({	uint64_t ts_u64in = (in);		\
	bool ts_bout = (bool)false;		\
	const unsigned int ts_umax = UINT_MAX;	\
	const uint64_t ts_u64_umax =		\
		(const uint64_t)ts_umax;	\
						\
	if (ts_u64in > ts_u64_umax) {		\
		ts_bout = (bool)true;		\
	}					\
	ts_bout;				})

/*
 * This macro checks if an unsigned long value is greater than the int maximum
 * value.
 */
#define tcc_math_ulong_gt_intmax(in)		\
({	unsigned long ts_ulin = (in);		\
	bool ts_bout = (bool)false;		\
	const unsigned long ts_imax =		\
		(UINT_MAX >> 1UL); 		\
						\
	if (ts_ulin > ts_imax) {		\
		ts_bout = (bool)true;		\
	}					\
	ts_bout;				})

/*
 * This macro checks if an unsigned long value is greater than the long maximum
 * value.
 */
#define tcc_math_ulong_gt_longmax(in)		\
({	unsigned long ts_ulin = (in);		\
	bool ts_bout = (bool)false;		\
	const unsigned long ts_longmax =	\
		(ULONG_MAX >> 1UL); 		\
						\
	if (ts_ulin > ts_longmax) {		\
		ts_bout = (bool)true;		\
	}					\
	ts_bout;				})

/*
 * This macro checks if an uint64_t value is greater than the int maximum
 * value.
 */
#define tcc_math_u64_gt_intmax(in)		\
({	uint64_t ts_u64in = (in);		\
	bool ts_bout = (bool)false;		\
	const unsigned long ts_imax =		\
		(UINT_MAX >> 1UL); 		\
	const uint64_t ts_u64_imax =		\
		(const uint64_t)ts_imax;	\
						\
	if (ts_u64in > ts_u64_imax) {		\
		ts_bout = (bool)true;		\
	}					\
	ts_bout;				})

/*
 * This macro checks if an unsigned int value is greater than the int maximum
 * value.
 */
#define tcc_math_uint_gt_intmax(in)		\
({	unsigned int ts_uin = (in);		\
	bool ts_bout = (bool)false;		\
	const unsigned int ts_imax =		\
		(UINT_MAX >> 1U); 		\
						\
	if (ts_uin > ts_imax) {			\
		ts_bout = (bool)true;		\
	}					\
	ts_bout;				})

/* ------------------- PLUS------------------------------------------*/
/*
 * INT32-C
 * This macro checks if int variables can be added together.
 * If si_a + si_b is larger than INT_MAX, the ts_iout is INT_MAX.
 * If si_a + si_b is less than INT_MIN, the ts_iout is INT_MIN.
 * Otherwise ts_iout is 0, which means it can be added.
 */
#define tcc_math_check_int_plus_int(si_a, si_b)	\
({	int ts_iout;						\
	int ts_ia = (si_a);					\
	int ts_ib = (si_b);					\
	const unsigned int ts_uimax = (UINT_MAX >> 1);		\
	const int ts_imax = (int)ts_uimax;			\
	const int ts_imin = (-ts_imax -1);			\
								\
	if ((ts_ib > 0) && (ts_ia > (ts_imax - ts_ib))) {	\
		ts_iout = ts_imax;				\
	} else if ((ts_ib < 0) && (ts_ia < (ts_imin - ts_ib))) {\
		ts_iout = ts_imin;				\
	} else {						\
		ts_iout = 0;					\
	}							\
	ts_iout;						\
})

/*
 * INT32-C
 * This macro checks if int variables can be added together.
 * If si_a + si_b is larger than INT_MAX, the ts_iout is INT_MAX.
 * If si_a + si_b is less than INT_MIN, the ts_iout is INT_MIN.
 * Otherwise ts_iout is 0, which means it can be added.
 */
#define tcc_math_check_long_plus_long(sl_a, sl_b)		\
({	long ts_lout;						\
	long ts_la = (sl_a);					\
	long ts_lb = (sl_b);					\
	const unsigned long ts_ulmax = (ULONG_MAX >> 1);	\
	const long ts_lmax = (long)ts_ulmax;			\
	const long ts_lmln = (-ts_lmax -1);			\
								\
	if ((ts_lb > 0) && (ts_la > (ts_lmax - ts_lb))) {	\
		ts_lout = ts_lmax;				\
	} else if ((ts_lb < 0) && (ts_la < (ts_lmln - ts_lb))) {\
		ts_lout = ts_lmln;				\
	} else {						\
		ts_lout = 0;					\
	}							\
	ts_lout;						\
})

/*
 * INT30-C
 * This macro checks if unsigned int variables can be added together.
 * If two variables can be added, tsout returns true.
 * Otherwise tsout returns false.
 */
#define tcc_math_check_uint_plus_uint(in_ua, in_ub)	\
({	bool ts_bout = (bool)true;			\
	unsigned int ts_ua = (in_ua);			\
	unsigned int ts_ub = (in_ub);			\
	const unsigned int ts_umax = UINT_MAX;		\
							\
	if ((ts_umax - ts_ua) < ts_ub) {		\
		ts_bout = (bool)false;			\
	}						\
	ts_bout;					\
})

/*
 * INT30-C
 * This macro checks if unsigned long variables can be added together.
 * If two variables can be added, tsout returns true.
 * Otherwise tsout returns false.
 */
#define tcc_math_check_ulong_plus_ulong(in_ula, in_ulb)	\
({	bool ts_bout = (bool)true;			\
	unsigned long ts_ula = (in_ula);		\
	unsigned long ts_ulb = (in_ulb);		\
	const unsigned long ts_ulmax = ULONG_MAX;	\
							\
	if ((ts_ulmax - ts_ula) < ts_ulb) {		\
		ts_bout = (bool)false;			\
	}						\
	ts_bout;					\
})

/* ------------------- MINUS-----------------------------------------*/
/*
 * INT32-C
 * This macro checks if int si_a can be subtracted by int si_b
 * If si_a - si_b is larger than INT_MAX, the ts_iout is INT_MAX.
 * If si_a - si_b is less than INT_MIN, the ts_iout is INT_MIN.
 * Otherwise ts_iout is 0, which means it can be subtracted.
 */
#define tcc_math_check_int_minus_int(si_a, si_b)	\
({	unsigned int ts_iout;					\
	unsigned int ts_ia = (si_a);				\
	unsigned int ts_ib = (si_b);				\
	const unsigned int ts_uimax = (UINT_MAX >> 1);		\
	const int ts_imax = (int)ts_uimax;			\
	const int ts_imin = (-ts_imax -1);			\
								\
	if (ts_ib > 0 && (si_a < (ts_imin + ts_ib))) {		\
		ts_iout = ts_imin;				\
	} else if (ts_ib < 0 && si_a > (ts_imax + ts_ib)) {	\
		ts_iout = ts_imax;				\
	} else {						\
		ts_iout = 0;					\
	}							\
	ts_iout; 						})

/*
 * INT30-C
 * This macro check the unsigned operands of the subtraction operation to
 * guarantee there is no possibility of unsigned wrap
 * If ui_b larget than ui_a, there is a possibility of unsigned wrap.
 * Otherwise ts_bout is true, which means it can be subtracted.
 */
#define tcc_math_check_uint_minus_uint(ui_a, ui_b)	\
({	bool ts_bout =  (bool)true;			\
	unsigned int ts_ua = (ui_a);			\
	unsigned int ts_ub = (ui_b);			\
							\
	if (ts_ua < ts_ub) {				\
		ts_bout = (bool)false;			\
	}						\
	ts_bout; 					})

/*
 * INT30-C
 * This macro check the unsigned operands of the subtraction operation to
 * guarantee there is no possibility of unsigned wrap
 * If ui_b larget than ui_a, there is a possibility of unsigned wrap.
 * Otherwise ts_bout is true, which means it can be subtracted.
 */
#define tcc_math_check_ulong_minus_ulong(ul_a, ul_b)	\
({	bool ts_bout =  (bool)true;			\
	unsigned long ts_ula = (ul_a);			\
	unsigned long ts_ulb = (ul_b);			\
							\
	if (ts_ula < ts_ulb) {				\
		ts_bout = (bool)false;			\
	}						\
	ts_bout; 					})

/*
 * INT32-C
 * This macro check if int si_a can be multiplied by int si_b.
 * If two variables can be multiplied, tsout returns true.
 * Otherwise tsout returns false.
 */
#define tcc_math_check_int_mul_int(si_a, si_b)				\
({	bool ts_bout = (bool)true;					\
	int ts_sia = (si_a);						\
	int ts_sib = (si_b);						\
	const unsigned int ts_simax = (UINT_MAX >> 1);			\
	const int ts_imax = (int)ts_simax;				\
	const int ts_imin = (-ts_imax -1);				\
									\
	if (ts_sia > 0) {  						\
		/* ts_sia is positive */				\
		if (ts_sib > 0) {  					\
			/* ts_sia and ts_sib are positive */		\
			if (ts_sia > (ts_imax / ts_sib)) {		\
				/* Handle error */			\
				ts_bout = (bool)false;			\
			}						\
		} else {						\
			/* ts_sia positive, ts_sib nonpositive */ 	\
			if (ts_sib < (ts_imin / ts_sia)) {		\
				/* Handle error */			\
				ts_bout = (bool)false;			\
			}						\
		} /* ts_sia positive, ts_sib nonpositive */		\
	} else {							\
		/* ts_sia is nonpositive */				\
		if (ts_sib > 0) {					\
			/* ts_sia is nonpositive, ts_sib is positive */	\
			if (ts_sia < (ts_imin / ts_sib)) {		\
				/* Handle error */			\
				ts_bout = (bool)false;			\
			}						\
		} else {						\
			/* ts_sia and ts_sib are nonpositive */		\
			if ( (ts_sia != 0) &&				\
				(ts_sib < (ts_imax / ts_sia))) {	\
				/* Handle error */			\
				ts_bout = (bool)false;			\
			}						\
		} /* End if ts_sia and ts_sib are nonpositive */	\
	} /* End if ts_sia is nonpositive */				\
									\
	ts_bout;							\
})

/*
 * INT32-C
 * This macro check if int sl_a can be multiplied by int sl_b.
 * If two variables can be multiplied, tsout returns true.
 * Otherwise tsout returns false.
 */
#define tcc_math_check_long_mul_long(sl_a, sl_b)			\
({	bool ts_bout = (bool)true;					\
	long ts_sla = (sl_a);						\
	long ts_slb = (sl_b);						\
	const unsigned long ts_ulmax = (ULONG_MAX >> 1);		\
	const long ts_lmax = (long)ts_ulmax;				\
	const long ts_lmin = (-ts_lmax -1);				\
									\
	if (ts_sla > 0) {  						\
		/* ts_sla is posltive */				\
		if (ts_slb > 0) {  					\
			/* ts_sla and ts_slb are posltive */		\
			if (ts_sla > (ts_lmax / ts_slb)) {		\
				/* Handle error */			\
				ts_bout = (bool)false;			\
			}						\
		} else {						\
			/* ts_sla posltive, ts_slb nonposltive */ 	\
			if (ts_slb < (ts_lmin / ts_sla)) {		\
				/* Handle error */			\
				ts_bout = (bool)false;			\
			}						\
		} /* ts_sla posltive, ts_slb nonposltive */		\
	} else {							\
		/* ts_sla is nonposltive */				\
		if (ts_slb > 0) {					\
			/* ts_sla is nonposltive, ts_slb is posltive */	\
			if (ts_sla < (ts_lmin / ts_slb)) {		\
				/* Handle error */			\
				ts_bout = (bool)false;			\
			}						\
		} else {						\
			/* ts_sla and ts_slb are nonposltive */		\
			if ( (ts_sla != 0) &&				\
				(ts_slb < (ts_lmax / ts_sla))) {	\
				/* Handle error */			\
				ts_bout = (bool)false;			\
			}						\
		} /* End if ts_sla and ts_slb are nonposltive */	\
	} /* End if ts_sla is nonposltive */				\
									\
	ts_bout;							\
})

/*
 * INT30-C
 * This macro check if unsigned int in_ua can be multiplied by
 * unsigned int in_ub.
 * If two variables can be multiplied, tsout returns true.
 * Otherwise tsout returns false.
 */
#define tcc_math_check_uint_mul_uint(in_ua, in_ub)	\
({	bool ts_bout = (bool)true;			\
	unsigned int ts_ua = (in_ua);			\
	unsigned int ts_ub = (in_ub);			\
	const unsigned int ts_umax = UINT_MAX;		\
							\
	if ((ts_ub > 0U) &&				\
	    (ts_ua > (ts_umax / ts_ub))) {		\
		ts_bout = (bool)false;			\
	} 						\
	ts_bout;					\
})

/*
 * INT30-C
 * This macro check if unsigned long in_ula can be multiplied by
 * unsigned long in_ulb.
 * If two variables can be multiplied, tsout returns true.
 * Otherwise tsout returns false.
 */
#define tcc_math_check_ulong_mul_ulong(in_ula, in_ulb)	\
({	bool ts_bout = (bool)true;			\
	unsigned long ts_ula = (in_ula);		\
	unsigned long ts_ulb = (in_ulb);		\
	const unsigned long ts_ulmax = ULONG_MAX;	\
							\
	if ((ts_ulb > 0UL) &&				\
	    (ts_ula > (ts_ulmax / ts_ulb))) {		\
		ts_bout = (bool)false;			\
	}						\
	ts_bout;					\
})


#endif
