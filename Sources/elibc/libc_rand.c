#include "../common/evm.h"
#include <stdlib.h>

evm_u32 libc_rand_max = RAND_MAX;
evm_result_t libc_srand(evm_thread_t *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs)
{
	if(numArgs < 1)
		return EVM_RTERR_ACCESS_VIOLATION;
	srand(args[0].ul);
	return EVM_OK;
}

evm_result_t libc_rand(evm_thread_t *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs)
{
	returnValue->l = rand();
	return EVM_OK;
}
