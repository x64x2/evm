#include "evm_internal.h"

// Code flow profiling to determine the possible stack levels at all instructions
#include "evm_stackoffsets.h"


evm_result_t evm_vmProfileInstructions(evm_program_t *program)
{
	evm_u32 i, j, numInstructions;
	evm_instruction_t *inst;

	inst = program->vmInstructions;
	numInstructions = program->vmInstructionCount;

	if(!numInstructions || inst[1].opcode != EVM_OP_BEGINSUB)
		return EVM_ERR_PROFILE_FAILED;

	inst[0].maxStackLevel = 0;

	for(i=1;i<numInstructions;i++)
	{
		if(inst[i].opcode == EVM_OP_BEGINSUB)
		{
			// Must be coming off of a clean function
			if(inst[i-1].maxStackLevel)
				return EVM_ERR_PROFILE_FAILED;

			inst[i].maxStackLevel = 0;
		}
		else
		{
			if(inst[i].opcode == EVM_OP_DROP8 && (inst[i].argument.ul & 3))
				return EVM_ERR_PROFILE_FAILED;
			if(inst[i].opcode == EVM_OP_DROP16 && (inst[i].argument.ul & 3))
				return EVM_ERR_PROFILE_FAILED;

			if(inst[i].opcode == EVM_OP_STORE_SPR4)
			{
				if(inst[i].argument.ul & 3)
					return EVM_ERR_PROFILE_FAILED;
				if(inst[i].argument.ul >= program->vmMemoryConsumed)
					return EVM_ERR_PROFILE_FAILED;
			}

			if(inst[i].opcode >= EVM_OP_BRANCHOPS_START &&
				inst[i].opcode < EVM_OP_BRANCHOPS_END)
			{
				if(inst[i].argument.ul >= program->vmInstructionCount)
					return EVM_ERR_PROFILE_FAILED;
			}

			if(inst[i].opcode == EVM_OP_SKIPNEXT &&
				((i+2) >= numInstructions
				|| inst[i+1].opcode == EVM_OP_ENDSUB
				|| inst[i+1].opcode == EVM_OP_SKIPNEXT)
				)
				return EVM_ERR_PROFILE_FAILED;

			// Look up the opcode
			for(j=0;j<NUM_STACK_OFFSETS;j++)
			{
				if(evm_stackoffsets[j].opcode == inst[i].opcode)
				{
					// Modify stack offset
					inst[i].maxStackLevel = inst[i-1].maxStackLevel + evm_stackoffsets[j].stackmod;

					if(inst[i].maxStackLevel > 0xFFFFFFF0)
						return EVM_ERR_PROFILE_FAILED;

					break;
				}
			}

			if(j == NUM_STACK_OFFSETS)
				return EVM_ERR_PROFILE_FAILED;
		}
	}

	if(inst[numInstructions-1].maxStackLevel != 0 || inst[numInstructions-1].opcode != EVM_OP_ENDSUB)
		return EVM_ERR_PROFILE_FAILED;

	return EVM_OK;
}
