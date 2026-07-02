#include "evm_internal.h"

evm_result_t evm_Dump(evm_ocpstate_t *state, evm_dumpfunc_t dumpfunc, void *outfile)
{
	evm_u32 symbolTableSize, i;
	evm_u32 numSymbols;
	evm_symbolhash_t *symbols;
	evm_header_t header;
	evm_relocation_t relocation;

	evm_relocation_t *relocations;
	evm_u32 numRelocations;

	evm_u32 temp;

	unsigned char linkmode, slen;

	symbols = state->symbols;
	numSymbols = state->numSymbols;

	relocations = state->relocations;
	numRelocations = state->numRelocations;

	// Calculate symbol table size
	symbolTableSize = 0;
	for(i=0;i<numSymbols;i++)
	{
		linkmode = symbols[i].sym.linkFlags & EVM_LINKFLAG_MASK;

		if(linkmode != EVM_LINKFLAG_LOCAL)
			symbolTableSize += strlen(symbols[i].sym.name) + 1;

		if(linkmode != EVM_LINKFLAG_IMPORT)
			symbolTableSize += sizeof(evm_u32);
		symbolTableSize++;
	}

	// Construct a header
	header.identifier[0] = 'D';
	header.identifier[1] = '3';
	header.identifier[2] = 'O';
	header.identifier[3] = 'B';

	header.symbolTableSize = evm_BigLong(symbolTableSize);
	header.symbolCount = evm_BigLong(state->numSymbols);

	header.relocationCount = evm_BigLong(state->numRelocations);

	header.dataTableSize = evm_BigLong(state->dataSegSize);
	header.dataTableSizeDecoded = evm_BigLong(state->dataSegSizeDecoded);

	header.litTableSize = evm_BigLong(state->litSegSize);

	header.codeTableSize = evm_BigLong(state->codeSegSize);
	header.codeTableNumInstructions = evm_BigLong(state->codeSegNumInstructions);

	header.bssSize = evm_BigLong(state->bssSegSize);

	// Write the header
	dumpfunc(&header, sizeof(header), outfile);

	// Write relocations
	for(i=0;i<numRelocations;i++)
	{
		relocation.symbolIndex = evm_BigLong(relocations[i].symbolIndex);
		relocation.table = relocations[i].table;
		relocation.unused[0] = relocation.unused[1] = relocation.unused[2] = 0;
		relocation.tableOffset = evm_BigLong(relocations[i].tableOffset);
		relocation.valueIncrement = evm_BigLong(relocations[i].valueIncrement);

		dumpfunc(&relocation, sizeof(relocation), outfile);
	}

	// Write data
	dumpfunc(state->dataseg, state->dataSegSize, outfile);

	// Write lit
	dumpfunc(state->litseg, state->litSegSize, outfile);

	// Write code
	dumpfunc(state->codeseg, state->codeSegSize, outfile);

	// Write the symbol table
	for(i=0;i<numSymbols;i++)
	{
		linkmode = symbols[i].sym.linkFlags;
		
		dumpfunc(&linkmode, 1, outfile);

		linkmode &= EVM_LINKFLAG_MASK;
		if(linkmode != EVM_LINKFLAG_LOCAL)
		{
			slen = strlen(symbols[i].sym.name);
			dumpfunc(&slen, 1, outfile);
			dumpfunc(symbols[i].sym.name, slen, outfile);
		}

		if(linkmode != EVM_LINKFLAG_IMPORT)
		{
			temp = evm_BigLong(symbols[i].sym.value);
			dumpfunc(&temp, 4, outfile);
		}
	}

	return EVM_OK;
}
