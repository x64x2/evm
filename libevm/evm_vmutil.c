#include "evm_internal.h"


static void evm_DecodeDataSeg(const void *in, void *out, evm_u32 encodedSize)
{
	evm_u32 offset;
	const unsigned char *inb;
	unsigned char *outb;

	unsigned char runLength;
	unsigned int i;

	int needByteswap;

	needByteswap = (evm_BigLong(1) != 1);

	inb = in;
	outb = out;

	for(offset=0;offset<encodedSize;)
	{
		runLength = *inb;
		inb++;
		offset++;

		// Load in native byte order
		if(!needByteswap)
			memcpy(outb, inb, runLength);
		else
		{
			for(i=0;i<runLength;i++)
				outb[i] = inb[runLength-i-1];
		}

		inb += runLength;
		offset += runLength;

		outb += runLength;
	}
}

static void evm_ParseCodeSeg(const void *in, evm_instruction_t *out, evm_u32 encodedSize)
{
	evm_u32 offset;
	evm_u32 arg;

	const unsigned char *inb;

	unsigned char opcode;

	inb = in;

	for(offset=0;offset<encodedSize;)
	{
		out->opcode = opcode = *inb++;
		offset++;

		// Load the argument
		if(opcode >= EVM_OP_ARGOPS_START)
		{
			memcpy(&arg, inb, 4);
			out->argument.l = evm_BigLong(arg);

			inb+=4;
			offset += 4;
		}

		out++;
	}
}


evm_result_t evm_AddObjects(evm_ocpstate_t *state, evm_program_t *program, evm_u32 flags)
{
	evm_u32 dataOffset, bssOffset, litOffset, newMemoryNeeded;

	// Relocations must all be resolved
	if(state->numRelocations && !(flags & EVM_RELOCATIONS_ALLOWED))
		return EVM_ERR_RELOCATIONS_NOT_RESOLVED;

	// Verify CODE and DATA integrity
	SAFECALL(evm_ObjectIntegrityCheck(state));

	// If no memory is allocated, allocate 4 bytes for NULL reads
	// This is usually done via relocates, but not if the object file
	// loaded is a compiled app.
	if(!program->vmMemoryConsumed)
		program->vmMemoryConsumed = 4;
	if(!program->vmInstructionCount)
		program->vmInstructionCount = 1;

	bssOffset = program->vmMemoryConsumed;
	dataOffset = bssOffset + state->bssSegSize;

	litOffset = bssOffset + state->dataSegSizeDecoded;

	newMemoryNeeded = program->vmMemoryConsumed + state->dataSegSizeDecoded + state->bssSegSize + state->litSegSize;

	// 4-byte align in case another object gets linked in
	newMemoryNeeded = (newMemoryNeeded + 3) & ~3;

	// Resize existing memory
	evm_saferealloc(program->vmMemory, newMemoryNeeded);

	// Resize instructions list
	evm_saferealloc(program->vmInstructions, (program->vmInstructionCount + state->codeSegNumInstructions) * sizeof(evm_instruction_t));

	// Decode the data segment
	evm_DecodeDataSeg(state->dataseg, BYTEOFFSET(program->vmMemory, dataOffset), state->dataSegSize);

	// Initialize the bss segment
	memset(BYTEOFFSET(program->vmMemory, bssOffset), 0, state->bssSegSize);

	// Copy the lit segment
	memcpy(BYTEOFFSET(program->vmMemory, litOffset), state->litseg, state->litSegSize);

	// Parse the code segment
	evm_ParseCodeSeg(state->codeseg, program->vmInstructions + program->vmInstructionCount, state->codeSegSize);


	// Extend VM parameters
	program->vmInstructionCount += state->codeSegNumInstructions;
	program->vmMemoryConsumed = newMemoryNeeded;

	// Throw out the linker stuff
	if(flags & EVM_RELOCATIONS_ALLOWED)
		return EVM_OK;
	else
		return evm_FreeObjects(state);
}


evm_result_t evm_FreeObjects(evm_ocpstate_t *state)
{
	if(state->codeseg)
		evm_free(state->codeseg);

	if(state->dataseg)
		evm_free(state->dataseg);

	if(state->litseg)
		evm_free(state->litseg);

	state->dataseg = NULL;
	state->dataSegSize = 0;
	state->dataSegSizeDecoded = 0;

	state->bssSegSize = 0;

	state->litseg = NULL;
	state->litSegSize = 0;

	state->codeseg = NULL;
	state->codeSegSize = 0;
	state->codeSegNumInstructions = 0;

	return EVM_OK;
}
