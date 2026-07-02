#include "evm_internal.h"


// Stack offsets need to be generated before execution
evm_result_t evm_vmInterpretCompileNative(evm_program_t *state)
{
	evm_result_t r;
	evm_instruction_t *inst;

	evm_u32 ci;
	evm_u32 spillOffset;


	r = evm_vmProfileCode(state);
	if(r != EVM_OK)
		return r;

	inst = state->vmInstructions;
	for(ci=1;ci<state->vmInstructionCount;ci++)
	{
		if(inst->opcode == EVM_OP_BEGINSUB)
			spillOffset = inst->argument.ul - (inst->maxStackLevel * 4);

		inst->maxStackLevel = (inst->maxStackLevel * 4) + spillOffset;
		inst++;
	}

	return EVM_OK;
}


#define MEMORY_OFFSET(t, i) ((t *)(vmMemory + (i)))
#define STACK_OFFSET(t, i) ((t *)(vsp + (i)))

#define ERROR_RETURN(value)	\
	do {\
		t->pc = (inst - instructionBase) + 1;\
		t->sp = sp;\
		return value;\
	} while (0)


#define ACCESS_MEMORY(size, stackoffset)	\
	memAddress = *STACK_OFFSET(evm_u32, inst->maxStackLevel + (stackoffset));\
	if(memAddress & (size-1))\
		ERROR_RETURN(EVM_RTERR_ALIGNMENT_ERROR);\
	if(!memAddress)\
		ERROR_RETURN(EVM_RTERR_NULL_ACCESS);\
	if(memAddress >= capacity)\
	{\
		if(!access_host_memory)\
			ERROR_RETURN(EVM_RTERR_ACCESS_VIOLATION);\
		memPointer = access_host_memory(memAddress, 0);\
		if(!memPointer)\
			ERROR_RETURN(EVM_RTERR_ACCESS_VIOLATION);\
	}\
	else\
		memPointer = vmMemory + memAddress

#define ENFORCE_JUMP_LIMIT	\
	do {\
		jumpCounter--;\
		if(!jumpCounter)\
			ERROR_RETURN(EVM_RTERR_JUMP_LIMIT_EXCEEDED);\
	} while (0)


#define MAKE_BRANCH(type, op)	\
	do {\
		if(*STACK_OFFSET(type, inst->maxStackLevel) op *STACK_OFFSET(type, inst->maxStackLevel+1))\
		{\
			pc = inst->argument.ul;\
			blockIDneeded = inst->blockID;\
			inst = instructionBase + (pc - 1);\
		}\
	} while (0)

evm_result_t evm_vmInterpretRun(evm_thread_t *t, evm_val32_t *returnValue)
{
	register unsigned char *vsp;
	unsigned char *vmMemory;
	evm_u32 capacity;
	evm_u32 pc;
	evm_u32 sp;
	evm_u32 jumpCounter;
	evm_program_t *prog;
	evm_instruction_t *inst;
	evm_instruction_t *instructionBase;

	evm_u32 instructionCount;

	evm_u32 memAddress;
	void *memPointer;

	float zeroCheckF;
	evm_u32 zeroCheckU;
	evm_i32 zeroCheckL;

	evm_u32 rvalue;
	evm_u32 saveOffset;

	evm_pointer_t sourceAddress;
	evm_pointer_t destAddress;
	evm_pointer_t blockSize;

	evm_result_t r;


	evm_result_t (*apifunction) (evm_thread_t *t, evm_u32 pc, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs);
	void *(*access_host_memory) (evm_pointer_t ptr, evm_u32 size);
	evm_u32 (*host_memory_capacity) (evm_pointer_t ptr);

	evm_u32 blockIDneeded = 0;

	pc = t->pc;
	sp = t->sp;
	prog = t->programState;

	vmMemory = prog->vmMemory;
	capacity = prog->vmMemoryConsumed;
	instructionCount = prog->vmInstructionCount;
	jumpCounter = prog->jumpLimit;

	apifunction = prog->apifunction;
	access_host_memory = prog->access_host_memory;
	host_memory_capacity = prog->host_memory_capacity;

	instructionBase = prog->vmInstructions;
	inst = instructionBase + pc;

	vsp = vmMemory + sp;

	for(;;)
	{
		if(inst == instructionBase)
			ERROR_RETURN(EVM_RTERR_NULL_ACCESS);

		switch(inst->opcode)
		{
		case EVM_OP_STORE1:
			ACCESS_MEMORY(1, 0);
			*((unsigned char *)memPointer) = *STACK_OFFSET(unsigned char, inst->maxStackLevel+4);
			break;
		case EVM_OP_STORE2:
			ACCESS_MEMORY(1, 0);
			*((evm_u16 *)memPointer) = *STACK_OFFSET(evm_u16, inst->maxStackLevel+4);
			break;
		case EVM_OP_STORE4:
			ACCESS_MEMORY(1, 0);
			*((evm_u32 *)memPointer) = *STACK_OFFSET(evm_u32, inst->maxStackLevel+4);
			break;

		case EVM_OP_LOAD1:
			ACCESS_MEMORY(1, -4);
			*STACK_OFFSET(unsigned char, inst->maxStackLevel-4) = *((unsigned char *)memPointer);
			break;
		case EVM_OP_LOAD2:
			ACCESS_MEMORY(1, -4);
			*STACK_OFFSET(evm_u16, inst->maxStackLevel-4) = *((evm_u16 *)memPointer);
			break;
		case EVM_OP_LOAD4:
			ACCESS_MEMORY(1, -4);
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) = *((evm_u32 *)memPointer);
			break;

#ifndef EVM_NO_FLOATING_POINT
		case EVM_OP_LTOF:
			*STACK_OFFSET(float, inst->maxStackLevel-4) = *STACK_OFFSET(evm_i32, inst->maxStackLevel-4);
			break;
		case EVM_OP_UTOF:
			*STACK_OFFSET(float, inst->maxStackLevel-4) = *STACK_OFFSET(evm_u32, inst->maxStackLevel-4);
			break;
		case EVM_OP_FTOL:
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) = *STACK_OFFSET(float, inst->maxStackLevel-4);
			break;
#endif

		case EVM_OP_SEX8:
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) = (char) (*STACK_OFFSET(evm_u32, inst->maxStackLevel-4));
			break;
		case EVM_OP_SEX16:
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) = (short) (*STACK_OFFSET(evm_u32, inst->maxStackLevel-4));
			break;

		case EVM_OP_ZHI24:
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) = 0x000000FF & (*STACK_OFFSET(evm_u32, inst->maxStackLevel-4));
			break;
		case EVM_OP_ZHI16:
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) = 0x0000FFFF & (*STACK_OFFSET(evm_u32, inst->maxStackLevel-4));
			break;

		case EVM_OP_NEGL:
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) = 0 - (*STACK_OFFSET(evm_u32, inst->maxStackLevel-4));
			break;

#ifndef EVM_NO_FLOATING_POINT
		case EVM_OP_NEGF:
			*STACK_OFFSET(float, inst->maxStackLevel-4) = 0.0f - (*STACK_OFFSET(float, inst->maxStackLevel-4));
			break;
#endif

		case EVM_OP_CALL:
			*STACK_OFFSET(evm_u32, 0) = (inst - instructionBase) + 1;
			*STACK_OFFSET(evm_u32, 4) = inst->maxStackLevel - 4;

			memAddress = *STACK_OFFSET(evm_u32, inst->maxStackLevel-4);

			if(!memAddress)
				ERROR_RETURN(EVM_RTERR_NULL_ACCESS);

			if(memAddress >= instructionCount)
			{
				if(!apifunction)
					ERROR_RETURN(EVM_RTERR_CODE_ACCESS_VIOLATION);

				t->pc = inst - instructionBase;
				t->sp = sp;

				r = apifunction(t, t->pc, STACK_OFFSET(evm_val32_t, inst->maxStackLevel), STACK_OFFSET(evm_val32_t, 8), (capacity - sp) / 4);

				if(r != EVM_OK)
					ERROR_RETURN(r);

				if(t->sp != sp)
					vsp = vmMemory + sp;
			}
			else
			{
				inst = instructionBase + memAddress - 1;

				if(inst[1].opcode != EVM_OP_BEGINSUB)
					ERROR_RETURN(EVM_RTERR_CALL_NOT_AT_SUB_START);

			}
			break;

		case EVM_OP_BSL:
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) <<= (*STACK_OFFSET(evm_u32, inst->maxStackLevel));
			break;
		case EVM_OP_BSRI:
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) >>= (*STACK_OFFSET(evm_i32, inst->maxStackLevel));
			break;
		case EVM_OP_BSRU:
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) >>= (*STACK_OFFSET(evm_u32, inst->maxStackLevel));
			break;

		case EVM_OP_BNOT:
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) = ~ (*STACK_OFFSET(evm_u32, inst->maxStackLevel-4));
			break;

		case EVM_OP_BAND:
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) &= (*STACK_OFFSET(evm_u32, inst->maxStackLevel));
			break;
		case EVM_OP_BOR:
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) |= (*STACK_OFFSET(evm_u32, inst->maxStackLevel));
			break;
		case EVM_OP_BXOR:
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) ^= (*STACK_OFFSET(evm_u32, inst->maxStackLevel));
			break;

#ifndef EVM_NO_FLOATING_POINT
		case EVM_OP_ADDF:
			*STACK_OFFSET(float, inst->maxStackLevel-4) += (*STACK_OFFSET(float, inst->maxStackLevel));
			break;
		case EVM_OP_SUBF:
			*STACK_OFFSET(float, inst->maxStackLevel-4) -= (*STACK_OFFSET(float, inst->maxStackLevel));
			break;
		case EVM_OP_MULF:
			*STACK_OFFSET(float, inst->maxStackLevel-4) *= (*STACK_OFFSET(float, inst->maxStackLevel));
			break;
		case EVM_OP_DIVF:
			zeroCheckF = (*STACK_OFFSET(float, inst->maxStackLevel));
			if(!zeroCheckF)
				ERROR_RETURN(EVM_RTERR_DIVISION_BY_ZERO);
			*STACK_OFFSET(float, inst->maxStackLevel-4) /= zeroCheckF;
			break;
#endif

		case EVM_OP_MULU:
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) *= (*STACK_OFFSET(evm_u32, inst->maxStackLevel));
			break;
		case EVM_OP_DIVU:
			zeroCheckU = (*STACK_OFFSET(evm_u32, inst->maxStackLevel));
			if(!zeroCheckU)
				ERROR_RETURN(EVM_RTERR_DIVISION_BY_ZERO);
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) /= zeroCheckU;
			break;
		case EVM_OP_MODU:
			zeroCheckU = (*STACK_OFFSET(evm_u32, inst->maxStackLevel));
			if(!zeroCheckU)
				ERROR_RETURN(EVM_RTERR_DIVISION_BY_ZERO);
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) %= zeroCheckU;
			break;

		case EVM_OP_ADDL:
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) += (*STACK_OFFSET(evm_u32, inst->maxStackLevel));
			break;
		case EVM_OP_SUBL:
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) -= (*STACK_OFFSET(evm_u32, inst->maxStackLevel));
			break;
		case EVM_OP_MULL:
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) *= (*STACK_OFFSET(evm_i32, inst->maxStackLevel));
			break;
		case EVM_OP_DIVL:
			zeroCheckL = (*STACK_OFFSET(evm_i32, inst->maxStackLevel));
			if(!zeroCheckL)
				ERROR_RETURN(EVM_RTERR_DIVISION_BY_ZERO);
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) /= zeroCheckL;
			break;
		case EVM_OP_MODL:
			zeroCheckL = (*STACK_OFFSET(evm_i32, inst->maxStackLevel));
			if(!zeroCheckL)
				ERROR_RETURN(EVM_RTERR_DIVISION_BY_ZERO);
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) %= zeroCheckL;
			break;

		case EVM_OP_JUMP:
			blockIDneeded = inst->blockID;
			memAddress = *STACK_OFFSET(evm_i32, inst->maxStackLevel);
			if(memAddress >= instructionCount)
				ERROR_RETURN(EVM_RTERR_CODE_ACCESS_VIOLATION);
			inst = instructionBase + memAddress - 1;
			break;

		case EVM_OP_DISCARD:
			break;

		case EVM_OP_SKIPNEXT:
			inst++;
			break;

		case EVM_OP_ADDR_SPR:
		case EVM_OP_ADDR_ARG:
			*STACK_OFFSET(evm_i32, inst->maxStackLevel-4) = sp + inst->argument.ul;
			break;

		case EVM_OP_BEGINSUB:
			// Make sure there's enough space
			if(inst->argument.ul > sp)
				ERROR_RETURN(EVM_RTERR_STACK_OVERFLOW);
			// Move SP
			sp -= inst->argument.ul;
			vsp -= inst->argument.ul;
			break;

		case EVM_OP_ENDSUB:
			rvalue = *STACK_OFFSET(evm_u32, inst->maxStackLevel);
			sp += inst->argument.ul;
			vsp += inst->argument.ul;
			pc = *STACK_OFFSET(evm_u32, 0);
			saveOffset = *STACK_OFFSET(evm_u32, 4);

			// Check thread exit
			if(!pc)
			{
				returnValue->ul = rvalue;
				ERROR_RETURN(EVM_THREAD_EXITED);
			}

			if(saveOffset & 3)
				ERROR_RETURN(EVM_RTERR_ALIGNMENT_ERROR);

			if(sp + saveOffset >= capacity)
				ERROR_RETURN(EVM_RTERR_ACCESS_VIOLATION);

			*STACK_OFFSET(evm_u32, saveOffset) = rvalue;

			inst = instructionBase + (pc - 1);
			break;

		case EVM_OP_COPY:
			destAddress = *STACK_OFFSET(evm_u32, inst->maxStackLevel);
			sourceAddress = *STACK_OFFSET(evm_u32, inst->maxStackLevel+4);
			blockSize = inst->argument.ul;

			if(!sourceAddress)
				ERROR_RETURN(EVM_RTERR_NULL_ACCESS);
			if(sourceAddress >= capacity)
				ERROR_RETURN(EVM_RTERR_NULL_ACCESS);
			if(sourceAddress+blockSize > capacity)
				ERROR_RETURN(EVM_RTERR_NULL_ACCESS);

			if(!destAddress)
				ERROR_RETURN(EVM_RTERR_NULL_ACCESS);
			if(destAddress >= capacity)
				ERROR_RETURN(EVM_RTERR_NULL_ACCESS);
			if(destAddress+blockSize > capacity)
				ERROR_RETURN(EVM_RTERR_NULL_ACCESS);

			memcpy(vmMemory+destAddress, vmMemory+sourceAddress, blockSize);
			break;

		case EVM_OP_BEQ:
			MAKE_BRANCH(evm_u32, ==);
			break;
		case EVM_OP_BNE:
			MAKE_BRANCH(evm_u32, !=);
			break;

		case EVM_OP_BGEU:
			MAKE_BRANCH(evm_u32, >=);
			break;
		case EVM_OP_BGEL:
			MAKE_BRANCH(evm_i32, >=);
			break;

		case EVM_OP_BGTU:
			MAKE_BRANCH(evm_u32, >);
			break;
		case EVM_OP_BGTL:
			MAKE_BRANCH(evm_i32, >);
			break;

		case EVM_OP_BLEU:
			MAKE_BRANCH(evm_u32, <=);
			break;
		case EVM_OP_BLEL:
			MAKE_BRANCH(evm_i32, <=);
			break;

		case EVM_OP_BLTU:
			MAKE_BRANCH(evm_u32, <);
			break;
		case EVM_OP_BLTL:
			MAKE_BRANCH(evm_i32, <);
			break;

#ifndef EVM_NO_FLOATING_POINT
		case EVM_OP_BEQF:
			MAKE_BRANCH(float, ==);
			break;
		case EVM_OP_BNEF:
			MAKE_BRANCH(float, !=);
			break;
		case EVM_OP_BGEF:
			MAKE_BRANCH(float, >=);
			break;
		case EVM_OP_BGTF:
			MAKE_BRANCH(float, >);
			break;
		case EVM_OP_BLEF:
			MAKE_BRANCH(float, <=);
			break;
		case EVM_OP_BLTF:
			MAKE_BRANCH(float, <);
			break;
#endif


		case EVM_OP_IMMEDIATE:
			*STACK_OFFSET(evm_u32, inst->maxStackLevel-4) = inst->argument.ul;
			break;

		case EVM_OP_STORE_SPR4:
			if(sp+inst->argument.ul >= capacity)
				ERROR_RETURN(EVM_RTERR_ACCESS_VIOLATION);
			*STACK_OFFSET(evm_u32, inst->argument.ul) = *STACK_OFFSET(evm_u32, inst->maxStackLevel);
			break;

		case EVM_OP_DROP8:
			if(sp+inst->argument.ul >= capacity)
				ERROR_RETURN(EVM_RTERR_ACCESS_VIOLATION);
			*STACK_OFFSET(unsigned char, inst->argument.ul) = *STACK_OFFSET(evm_u32, inst->maxStackLevel);
			break;
		case EVM_OP_DROP16:
			if(sp+inst->argument.ul >= capacity)
				ERROR_RETURN(EVM_RTERR_ACCESS_VIOLATION);
			*STACK_OFFSET(evm_u16, inst->argument.ul) = *STACK_OFFSET(evm_u32, inst->maxStackLevel);
			break;

#ifdef EVM_NO_FLOATING_POINT
		case EVM_OP_LTOF:
		case EVM_OP_UTOF:
		case EVM_OP_FTOL:
		case EVM_OP_NEGF:
		case EVM_OP_ADDF:
		case EVM_OP_SUBF:
		case EVM_OP_MULF:
		case EVM_OP_DIVF:
		case EVM_OP_BEQF:
		case EVM_OP_BNEF:
		case EVM_OP_BGEF:
		case EVM_OP_BGTF:
		case EVM_OP_BLEF:
		case EVM_OP_BLTF:
			return EVM_RTERR_NO_FLOATING_POINT;
#endif

		}

		inst++;
		if(blockIDneeded && inst->blockID != blockIDneeded)
			ERROR_RETURN(EVM_RTERR_SUBROUTINE_ZONE_VIOLATION);

		blockIDneeded = 0;
	}


	return EVM_OK;
}
