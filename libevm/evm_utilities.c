#include "evm_internal.h"

evm_result_t evm_uResolveUsingAPIBlock(evm_apiblock_t *block, const char *name, evm_u32 *resolution)
{
	evm_u32 i, numEntries;
	evm_apientry_t *entries;

	numEntries = block->numEntries;
	entries = block->apientries;

	for(i=0;i<numEntries;i++)
	{
		if(!strcmp(entries[i].name, name))
		{
			*resolution = i + block->pcBase;
			return EVM_OK;
		}
	}

	return EVM_ERR_SYMBOL_NOT_FOUND;
}

evm_apifunc_t evm_uGetAPIFunction(evm_apiblock_t *block, evm_u32 pc)
{
	// Make sure it's in-range
	pc -= block->pcBase;

	if(pc >= block->numEntries)
		return NULL;

	// Just return
	return block->apientries[pc].func;
}
