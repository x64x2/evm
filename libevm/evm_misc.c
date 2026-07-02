// This stuff should go somewhere else!!

#include "evm_internal.h"

// NOTE: Block IDs had to be assigned to subs to ensure that jumps could not be
// performed across subroutines.  Otherwise, almost every instruction would
// require a bounds check because a routine could jump into another function
// with a larger spill area.


#define SPILL_UPDATE_BLOCK \
	while(lastBeginSub < i)\
	{\
		switch(program->vmInstructions[lastBeginSub].opcode)\
		{\
		case EVM_OP_BEGINSUB:\
			program->vmInstructions[lastBeginSub].maxStackLevel = spillSize;\
		case EVM_OP_ADDR_ARG:\
		case EVM_OP_ENDSUB:\
		case EVM_OP_DROP8:\
		case EVM_OP_DROP16:\
			program->vmInstructions[lastBeginSub].argument.ul += spillSize * 4;\
		default:\
			break;\
		}\
		lastBeginSub++;\
	}



evm_result_t evm_vmProfileCode(evm_program_t *program)
{
	evm_result_t r;
	evm_u32 nativeCodeSize;
	void *nativeCode;

	evm_u32 blockID = 1;

	evm_u32 i;
	evm_u32 spillSize;
	evm_u32 lastBeginSub;

	evm_u32 frameSize;

	// Profile the code to get stack offsets
	r = evm_vmProfileInstructions(program);
	if(r != EVM_OK)
		return r;

	// Convert to actual offsets
	lastBeginSub = 0;
	spillSize = 0;
	for(i=1;i<program->vmInstructionCount;i++)
	{
		if(program->vmInstructions[i].opcode == EVM_OP_BEGINSUB)
		{
			frameSize = program->vmInstructions[i].argument.ul;
			if(frameSize < 8 || (frameSize & 3))
				return EVM_ERR_PROFILE_FAILED;
			if(i != 1 && program->vmInstructions[i-1].opcode != EVM_OP_ENDSUB)
				return EVM_ERR_PROFILE_FAILED;

			// Create spill offsets for everything else
			if(lastBeginSub)
			{
				// Offset anything affected by the spill size
				SPILL_UPDATE_BLOCK;
			}

			lastBeginSub = i;
			spillSize = 0;
			blockID++;

			program->vmInstructions[i].blockID = 0;
		}
		else
		{
			program->vmInstructions[i].blockID = blockID;

			if(program->vmInstructions[i].opcode == EVM_OP_BEGINSUB &&
				program->vmInstructions[i].argument.ul != frameSize)
				return EVM_ERR_PROFILE_FAILED;

			if(program->vmInstructions[i].maxStackLevel > spillSize)
				spillSize = program->vmInstructions[i].maxStackLevel;

			if(program->vmInstructions[i].opcode == EVM_OP_COPY &&
				program->vmInstructions[i].argument.ul >= program->vmMemoryConsumed)
				return EVM_ERR_PROFILE_FAILED;
		}
	}

	SPILL_UPDATE_BLOCK;

	return EVM_OK;
}




typedef evm_result_t (*compiledfunc_t) (evm_thread_t *, evm_u32 *);


evm_result_t evm_vmJITRun(evm_thread_t *t, evm_val32_t *rval)
{
	compiledfunc_t fptr;
	evm_program_t *prog;
	unsigned char *vmMemory;

	evm_result_t r;

	if(t->pc >= t->programState->vmInstructionCount)
	{
		prog = t->programState;
		vmMemory = prog->vmMemory;

		// Native call
		if(!prog->apifunction)
			return EVM_RTERR_CODE_ACCESS_VIOLATION;
		r = prog->apifunction(t, t->pc, rval, (evm_val32_t *)(vmMemory + t->sp + 8), (prog->vmMemoryConsumed - t->sp - 8) / 4);
		if(r != EVM_OK) return r;
		return EVM_THREAD_EXITED;
	}

	fptr = (compiledfunc_t)(t->programState->vmCompiledCode);

	return fptr(t, &rval->ul);
}
