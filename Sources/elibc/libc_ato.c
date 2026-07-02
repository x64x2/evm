#include "../common/evm.h"

evm_result_t libc_atoi(evm_thread_t *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs)
{
	void *ptr;
	const char *c;
	evm_u32 capacity;
	int r;
	int negate=0;

	r = 0;

	if(numArgs < 1) return EVM_RTERR_ACCESS_VIOLATION;

	c = t->evm_vmGetPointer(t, args[0].ptr);
	if(!c) return EVM_RTERR_ACCESS_VIOLATION;

	capacity = t->evm_vmGetPointerCapacity(t, args[0].ptr);

	if(!capacity) return EVM_RTERR_ACCESS_VIOLATION;

	if(*c == '-')
	{
		negate = 1;
		capacity--;
		c++;
	}

	while(capacity && *c)
	{
		r*=10;

		if(*c >= '0' && *c <= '9')
			r+=*c-'0';

		c++;
		capacity--;
	}
	if(!capacity) return EVM_RTERR_ACCESS_VIOLATION;
	if(negate) returnValue->l = -r; else returnValue->l = r;
	return EVM_OK;
}
