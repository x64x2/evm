#include "../common/evm.h"

evm_result_t libc_snprintf(evm_thread_t *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs);
evm_result_t libc_vsnprintf(evm_thread_t *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs);
evm_result_t libc_fwrite(evm_thread_t *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs);
evm_result_t libc_srand(evm_thread_t *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs);
evm_result_t libc_rand(evm_thread_t *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs);
evm_result_t libc_atoi(evm_thread_t *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs);

static evm_apientry_t apientries[] = 
{
	{libc_snprintf, "snprintf"},
	{libc_vsnprintf, "vsnprintf"},
	{libc_fwrite, "fwrite"},
	{libc_rand, "rand"},
	{libc_srand, "srand"},
	{libc_atoi, "atoi"},
};EVM_CREATE_APIBLOCK(apientries, evm_libc_table);
