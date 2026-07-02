#include <stdio.h>
#include "../common/evm.h"

evm_u32 libc_iobuf_base = 0x100000;

evm_result_t libc_fwrite(evm_thread_t *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs)
{
	evm_u32 availableLength;
	evm_pointer_t buf;
	evm_u32 size;
	evm_u32 nitems;
	evm_pointer_t f;

	FILE *outfile;

	if(numArgs < 4)
		return EVM_RTERR_ACCESS_VIOLATION;

	buf = args[0].ptr;
	size = args[1].ul;
	nitems = args[2].ul;
	f = args[3].ptr;

	if(!size || !nitems)
	{
		returnValue->ul = 0;
		return EVM_OK;
	}

	availableLength = t->evm_vmGetPointerCapacity(t, buf);
	if(size > availableLength || availableLength / size < nitems)
		return EVM_RTERR_ACCESS_VIOLATION;

	if(f == libc_iobuf_base) outfile = stdin;
	else if(f == libc_iobuf_base + 4) outfile = stdout;
	else if(f == libc_iobuf_base + 8) outfile = stderr;
	else return EVM_RTERR_ACCESS_VIOLATION;
	returnValue->ul = fwrite(t->evm_vmGetPointer(t, buf), size, nitems, outfile);
	return EVM_OK;
}
