/*
	functions.hh
	------------
*/

#ifndef VLIB_FUNCTIONS_HH
#define VLIB_FUNCTIONS_HH


namespace vlib
{
	
	struct proc_info;
	
	extern const proc_info proc_abs;
	extern const proc_info proc_areaof;
	extern const proc_info proc_half;
	extern const proc_info proc_head;
	extern const proc_info proc_hex;
	extern const proc_info proc_mince;
	extern const proc_info proc_rep;
	extern const proc_info proc_sha256;
	extern const proc_info proc_substr;
	extern const proc_info proc_tail;
	extern const proc_info proc_trans;
	extern const proc_info proc_unbin;
	extern const proc_info proc_unhex;
	
	extern const proc_info proc_mkpub;
	extern const proc_info proc_sign;
	extern const proc_info proc_verify;
	
}

#endif
