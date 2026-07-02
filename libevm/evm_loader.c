#include "evm_internal.h"
#include "../common/evm_endian.h"

// VM object file parser

static evm_result_t evm_BufferRead(void *out, evm_u32 size, evm_membuffer_t *buf)
{
	if(buf->offset + size > buf->size)
		return EVM_ERR_UNEXPECTED_EOF;

	memcpy(out, ((char *)(buf->memory))+buf->offset, size);
	buf->offset += size;

	return EVM_OK;
}

evm_result_t evm_ObjectIntegrityCheck(evm_ocpstate_t *state)
{
	evm_u32 segOffset, segExpectedSize;
	unsigned char runLength, opcode;
	char temp[255];

	evm_membuffer_t membuf;

	membuf.memory = state->dataseg;
	membuf.offset = 0;
	membuf.size = state->dataSegSize;

	segExpectedSize = state->dataSegSizeDecoded;

	for(segOffset=0;segOffset<segExpectedSize;)
	{
		SAFECALLCUSTOM(EVM_ERR_CORRUPT_DATA_SEGMENT, evm_BufferRead(&runLength, 1, &membuf));

		if(!runLength)
			return EVM_ERR_CORRUPT_DATA_SEGMENT;

		SAFECALLCUSTOM(EVM_ERR_CORRUPT_DATA_SEGMENT, evm_BufferRead(temp, runLength, &membuf));

		segOffset += runLength;
	}

	// Must be an exact match
	if(segOffset != segExpectedSize || membuf.offset != membuf.size)
		return EVM_ERR_CORRUPT_DATA_SEGMENT;


	membuf.memory = state->codeseg;
	membuf.offset = 0;
	membuf.size = state->codeSegSize;

	segExpectedSize = state->codeSegNumInstructions;
	for(segOffset=0;segOffset<segExpectedSize;)
	{
		SAFECALLCUSTOM(EVM_ERR_CORRUPT_CODE_SEGMENT, evm_BufferRead(&opcode, 1, &membuf));

		if(opcode >= EVM_OP_NUM_OPCODES)
			return EVM_ERR_CORRUPT_CODE_SEGMENT;

		if(opcode >= EVM_OP_ARGOPS_START)
			SAFECALLCUSTOM(EVM_ERR_CORRUPT_CODE_SEGMENT, evm_BufferRead(temp, 4, &membuf));

		segOffset++;
	}

	// Must be an exact match
	if(segOffset != segExpectedSize || membuf.offset != membuf.size)
		return EVM_ERR_CORRUPT_CODE_SEGMENT;

	// Integrity OK
	return EVM_OK;
}

evm_result_t evm_LoadObjectFile(evm_ocpstate_t *state, const void *buffer, evm_u32 size)
{
	evm_membuffer_t membuf;
	evm_membuffer_t segmembuf;

	evm_header_t header;
	evm_relocation_t *relocations;
	evm_relocation_t relocation;
	evm_symbol_t symbol;

	evm_u32 readOffset=0;

	evm_u32 i;

	unsigned char runLength;
	unsigned char hash;

	membuf.memory = buffer;
	membuf.size = size;
	membuf.offset = 0;

	// Read the header
	SAFECALL(evm_BufferRead(&header, sizeof(header), &membuf));

	if(header.identifier[0] != 'D' ||
		header.identifier[1] != '3' ||
		header.identifier[2] != 'O' ||
		header.identifier[3] != 'B')
		return EVM_ERR_NOT_AN_OBJECT_FILE;


	ByteSwapLong(header.symbolTableSize);
	ByteSwapLong(header.symbolCount);

	ByteSwapLong(header.relocationCount);

	ByteSwapLong(header.dataTableSize);
	ByteSwapLong(header.dataTableSizeDecoded);

	ByteSwapLong(header.litTableSize);

	ByteSwapLong(header.codeTableSize);
	ByteSwapLong(header.codeTableNumInstructions);

	ByteSwapLong(header.bssSize);

	// BSS and DATA must be 4-byte aligned
	if(header.bssSize & 3)
		return EVM_ERR_INVALID_OBJECT_FILE;
	if(header.dataTableSizeDecoded & 3)
		return EVM_ERR_INVALID_OBJECT_FILE;

	// Sanity-check the file size.  It should be exactly large enough for the data.
	if(sizeof(evm_header_t) + header.dataTableSize + header.litTableSize + header.codeTableSize + header.symbolTableSize + header.relocationCount*sizeof(evm_relocation_t) != size)
		return EVM_ERR_INVALID_OBJECT_FILE;

	// The previous check can return a false positive if overflowed values are given,
	// so check all seg sizes manually
	if(header.dataTableSize >= size ||
		header.litTableSize >= size ||
		header.codeTableSize >= size ||
		header.symbolTableSize >= size ||
		header.relocationCount*sizeof(evm_relocation_t) >= size)
		return EVM_ERR_INVALID_OBJECT_FILE;

	readOffset += sizeof(evm_header_t);


	// Load relocations
	evm_saferealloc(state->relocations, (state->numRelocations + header.relocationCount) * sizeof(evm_relocation_t));

	segmembuf.memory = BYTEOFFSET(membuf.memory, readOffset);
	segmembuf.offset = 0;
	segmembuf.size = header.relocationCount * sizeof(evm_relocation_t);
	relocations = state->relocations + state->numRelocations;

	readOffset += header.relocationCount * sizeof(evm_relocation_t);
	membuf.offset = readOffset;

	for(i=0;i<header.relocationCount;i++)
	{
		SAFECALL(evm_BufferRead(&relocation, sizeof(evm_relocation_t), &segmembuf));

		relocations[i].symbolIndex = evm_BigLong(relocation.symbolIndex);
		relocations[i].table = relocation.table;

		if(relocations[i].symbolIndex >= header.symbolCount)
			return EVM_ERR_CORRUPT_RELOCATION_TABLE;

		relocations[i].symbolIndex += state->numSymbols;

		// Localize relocations
		switch(relocation.table)
		{
		case EVM_SEG_CODE:
			relocations[i].tableOffset = evm_BigLong(relocation.tableOffset) + state->codeSegSize;
			break;
		case EVM_SEG_DATA:
			relocations[i].tableOffset = evm_BigLong(relocation.tableOffset) + state->dataSegSize;
			break;
		default:
			// Relocations are only allowed in CODE and DATA
			return EVM_ERR_CORRUPT_RELOCATION_TABLE;
		}

		relocations[i].valueIncrement = evm_BigLong(relocation.valueIncrement);
	}

	// Load the data table
	evm_saferealloc(state->dataseg, state->dataSegSize + header.dataTableSize);

	SAFECALL(evm_BufferRead(BYTEOFFSET(state->dataseg, state->dataSegSize), header.dataTableSize, &membuf));

	readOffset += header.dataTableSize;


	// Load the literal table
	evm_saferealloc(state->litseg, state->litSegSize + header.litTableSize);

	SAFECALL(evm_BufferRead(BYTEOFFSET(state->litseg, state->litSegSize), header.litTableSize, &membuf));

	readOffset += header.litTableSize;

	// Load the code table
	evm_saferealloc(state->codeseg, state->codeSegSize + header.codeTableSize);

	SAFECALL(evm_BufferRead(BYTEOFFSET(state->codeseg, state->codeSegSize), header.codeTableSize, &membuf));

	readOffset += header.codeTableSize;

	// Load the symbol table
	evm_saferealloc(state->symbols, (state->numSymbols + header.symbolCount) * sizeof(evm_symbolhash_t));

	segmembuf.memory = BYTEOFFSET(membuf.memory, readOffset);
	segmembuf.offset = 0;
	segmembuf.size = header.symbolTableSize;

	for(i=0;i<header.symbolCount;i++)
	{
		SAFECALL(evm_BufferRead(&symbol.linkFlags, 1, &segmembuf));

		if((symbol.linkFlags & EVM_LINKFLAG_MASK) == EVM_LINKFLAG_LOCAL)
			symbol.name[0] = '\0';
		else
		{
			SAFECALL(evm_BufferRead(&runLength, 1, &segmembuf));

			if(runLength > 254 || !runLength)
				return EVM_ERR_CORRUPT_SYMBOL_TABLE;

			SAFECALL(evm_BufferRead(symbol.name, runLength, &segmembuf));

			symbol.name[runLength] = '\0';

			// Add it to the lookup chains
			hash = evm_hashstring(symbol.name);
			state->symbols[state->numSymbols+i].nextSymbol = state->symbolHashChains[hash];
			state->symbols[state->numSymbols+i].hash = hash;
			state->symbolHashChains[hash] = state->numSymbols + i + 1;
		}


		// If it's not an import, read the value and localize it
		if((symbol.linkFlags & EVM_LINKFLAG_MASK) != EVM_LINKFLAG_IMPORT)
		{
			SAFECALL(evm_BufferRead(&symbol.value, 4, &segmembuf));

			symbol.value = evm_BigLong(symbol.value);

			switch(symbol.linkFlags & EVM_SEG_MASK)
			{
			case EVM_SEG_CODE:
				symbol.value += state->codeSegNumInstructions;
				break;
			case EVM_SEG_BSS:
				symbol.value += state->bssSegSize;
				break;
			case EVM_SEG_DATA:
				symbol.value += state->dataSegSizeDecoded;
				break;
			case EVM_SEG_LIT:
				symbol.value += state->litSegSize;
				break;
			}
		}

		// If it's named, add it to the hash chain
		memcpy(&state->symbols[state->numSymbols+i].sym, &symbol, sizeof(evm_symbol_t));
	}

	// This should fit exactly
	if(segmembuf.offset != segmembuf.size)
		return EVM_ERR_CORRUPT_SYMBOL_TABLE;

	// Update the state with new values
	state->numSymbols += header.symbolCount;
	state->numRelocations += header.relocationCount;

	state->dataSegSize += header.dataTableSize;
	state->dataSegSizeDecoded += header.dataTableSizeDecoded;

	state->litSegSize += header.litTableSize;

	state->codeSegSize += header.codeTableSize;
	state->codeSegNumInstructions += header.codeTableNumInstructions;

	state->bssSegSize += header.bssSize;

	return EVM_OK;
}


evm_result_t evm_CalculateObjectFileSize(evm_header_t *header, evm_u32 *size)
{
	*size = sizeof(evm_header_t) + evm_BigLong(header->dataTableSize) + evm_BigLong(header->litTableSize) + evm_BigLong(header->codeTableSize) + evm_BigLong(header->symbolTableSize) + evm_BigLong(header->relocationCount)*sizeof(evm_relocation_t);
	return EVM_OK;
}
