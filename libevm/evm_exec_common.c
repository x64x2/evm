#include "evm_internal.h"

evm_result_t evm_vmGenericCall(evm_thread_t *t, evm_u32 address, evm_val32_t *parameters, evm_u32 numParameters)
{
	evm_u32 spaceNeeded;
	evm_u32 *p;
	evm_u32 i;

	spaceNeeded = numParameters*4 + 8;

	if(t->sp <= spaceNeeded)
		return EVM_RTERR_STACK_OVERFLOW;
	t->sp -= spaceNeeded;
	t->pc = address;

	p = ((evm_u32 *)t->programState->vmMemory) + t->sp / 4;

	// Use 0 as the return PC, which signifies an exit
	p[0] = 0;
	p[1] = 0;

	for(i=0;i<numParameters;i++)
		p[i+2] = parameters[i].ul;

	return 0;
}


evm_result_t evm_vmGenericEndCall(evm_thread_t *t, evm_u32 numParameters)
{
	t->sp += numParameters*4 + 8;

	return EVM_OK;
}

void *evm_GenericGetPointer(evm_thread_t *t, evm_pointer_t ptr)
{
	if(ptr < t->programState->vmMemoryConsumed)
		return ((char *)t->programState->vmMemory) + ptr;
	return t->programState->access_host_memory(ptr, 0);
}


evm_u32 evm_GenericGetPointerCapacity(evm_thread_t *t, evm_pointer_t ptr)
{
	if(ptr < t->programState->vmMemoryConsumed)
		return t->programState->vmMemoryConsumed - ptr;
	return t->programState->host_memory_capacity(ptr);
}


void *evm_GenericGetSpanPointer(evm_thread_t *t, evm_pointer_t ptr, evm_u32 size)
{
	if(ptr < t->programState->vmMemoryConsumed)
	{
		if(ptr+size <= t->programState->vmMemoryConsumed)
			return ((char *)t->programState->vmMemory) + ptr;
		return NULL;
	}
	return t->programState->access_host_memory(ptr, size);
}


evm_result_t evm_vmGenericInitializeThread(evm_thread_t *t, evm_ocpstate_t *state, evm_program_t *program)
{
	evm_symbol_t *sym;
	evm_result_t r;

	memset(t, 0, sizeof(evm_thread_t));

	t->programState = program;

	r = evm_FindExport(state, "$__evm_stack", &sym);

	if(r != EVM_OK)
		return r;

	t->sp = sym->value;
	t->evm_vmGetPointer = evm_GenericGetPointer;
	t->evm_vmGetPointerCapacity = evm_GenericGetPointerCapacity;
	t->evm_vmGetSpanPointer = evm_GenericGetSpanPointer;

	return EVM_OK;
}
