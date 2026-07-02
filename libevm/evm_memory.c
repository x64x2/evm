#include "evm_internal.h"

void *evm_frealloc(evm_memorymanager_t *mm, void *ptr, size_t size)
{
	void *newptr;

	if(!size)
		return NULL;

	if(mm->realloc)
		newptr = mm->realloc(ptr, size, mm->opaque);
	else
		newptr = realloc(ptr, size);

	// Free the buffer if it fails
	if(!newptr && ptr)
	{
		if(mm->free)
			mm->free((ptr), mm->opaque);
		else
			free(ptr);
	}

	return newptr;
}

unsigned char evm_hashstring(const char *str)
{
	int hash=0;

	while(*str)
		hash = hash + *str++;

	return (unsigned char)hash;
}

evm_result_t evm_FreeOCPState(evm_ocpstate_t *state)
{
	if(state->codeseg)
		evm_free(state->codeseg);
	if(state->dataseg)
		evm_free(state->dataseg);
	if(state->litseg)
		evm_free(state->litseg);
	if(state->relocations)
		evm_free(state->relocations);
	if(state->symbols)
		evm_free(state->symbols);

	return EVM_OK;
}


evm_result_t evm_FreeProgram(evm_program_t *state)
{
	if(state->vmInstructions)
		evm_free(state->vmInstructions);
	if(state->vmMemory)
		evm_free(state->vmMemory);
	if(state->vmCompiledCode)
		evm_free(state->vmCompiledCode);

	return EVM_OK;
}
