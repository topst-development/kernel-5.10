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

/* New defines */
/*
 * This macro checks if an unsigned long value is greater than the unsigned
 * int maximum value.
 */

#define tcc_math_ulong_gt_ctmax(in) 		\
({	unsigned long ts_ulin = (in);		\
	bool ts_bout = (bool)false;		\
	const int ts_cmax = SCHAR_MAX;		\
	const unsigned long ts_ul_cmax =	\
		(const unsigned long)ts_cmax;	\
						\
	if (ts_ulin > ts_ul_cmax) {		\
		ts_bout = (bool)true;		\
	}					\
	ts_bout;				\
})

#define tcc_math_ulong_gt_uctmax(in) 			\
({	unsigned long ts_ulin = (in);			\
	bool ts_bout = (bool)false;			\
	const unsigned long ts_ul_ucmax = UCHAR_MAX;	\
							\
	if (ts_ulin > ts_ul_ucmax) {			\
		ts_bout = (bool)true;			\
	}						\
	ts_bout;					\
})

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
 * This macro checks if an uint64_t value is greater than the unsigned
 * int maximum value.
 */
#define tcc_math_u64_gt_ulongmax(in) 		\
({	uint64_t ts_u64in = (in);		\
	bool ts_bout = (bool)false;		\
	const unsigned long ts_umax = ULONG_MAX;\
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

/*
 * This macro checks if an uint64_t value is greater than the unsigned
 * int maximum value.
 */
#define tcc_math_u64_gt_size_tmax(in) 		\
({	uint64_t ts_u64in = (in);		\
	bool ts_bout = (bool)false;		\
	const size_t ts_umax = SIZE_MAX;	\
	const uint64_t ts_u64_umax =		\
		(const uint64_t)ts_umax;	\
						\
	if (ts_u64in > ts_u64_umax) {		\
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
({	int ts_iout = 0;					\
	int ts_ia = (si_a);					\
	int ts_ib = (si_b);					\
	bool ts_bout = (bool)true;				\
	const unsigned int ts_uimax = (UINT_MAX >> 1);	\
	const int ts_imax = (int)ts_uimax;			\
	const int ts_imin = (-ts_imax -1);			\
								\
	if ((ts_ib > 0) && (ts_ia > (ts_imax - ts_ib))) {	\
		ts_iout = ts_imax;				\
		ts_bout = (bool)false;				\
	} else if ((ts_ib < 0) && (ts_ia < (ts_imin - ts_ib))) {\
		ts_iout = ts_imin;				\
		ts_bout = (bool)false;				\
	}							\
	ts_bout;						\
})

/*
 * INT32-C
 * This macro checks if int variables can be added together.
 * If si_a + si_b is larger than INT_MAX, the ts_iout is INT_MAX.
 * If si_a + si_b is less than INT_MIN, the ts_iout is INT_MIN.
 * Otherwise ts_iout is 0, which means it can be added.
 */
#define tcc_math_check_long_plus_long(sl_a, sl_b)		\
({	long ts_lout = 0;					\
	long ts_la = (sl_a);					\
	long ts_lb = (sl_b);					\
	bool ts_bout = (bool)true;				\
	const unsigned long ts_ulmax = (ULONG_MAX >> 1);	\
	const long ts_lmax = (long)ts_ulmax;			\
	const long ts_lmln = (-ts_lmax -1);			\
								\
	if ((ts_lb > 0) && (ts_la > (ts_lmax - ts_lb))) {	\
		ts_lout = ts_lmax;				\
		ts_bout = (bool)false;				\
	} else if ((ts_lb < 0) && (ts_la < (ts_lmln - ts_lb))) {\
		ts_lout = ts_lmln;				\
		ts_bout = (bool)false;				\
	}							\
	ts_bout;						\
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

/*
 * INT30-C
 * This macro checks if unsigned long variables can be added together.
 * If two variables can be added, tsout returns true.
 * Otherwise tsout returns false.
 */
#define tcc_math_check_size_t_plus_size_t(in_size_ta, in_size_tb) 	\
({	bool ts_bout = (bool)true;					\
	size_t ts_size_ta = (in_size_ta);				\
	size_t ts_size_tb = (in_size_tb);				\
	const size_t ts_size_tmax = SIZE_MAX;				\
									\
	if ((ts_size_tmax - ts_size_ta) < ts_size_tb) {			\
		ts_bout = (bool)false;					\
	}								\
	ts_bout;							\
})

/*
 * INT30-C
 * This macro checks if unsigned long variables can be added together.
 * If two variables can be added, tsout returns true.
 * Otherwise tsout returns false.
 */
#define tcc_math_check_u64_plus_u64(in_u64a, in_u64b)	\
({	bool ts_bout = (bool)true;			\
	u64 ts_u64a = (in_u64a);		\
	u64 ts_u64b = (in_u64b);		\
	const u64 ts_ulmax = U64_MAX;		\
							\
	if ((ts_ulmax - ts_u64a) < ts_u64b) {		\
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
