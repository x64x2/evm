#include "evm_internal.h"


evm_result_t evm_MarkAPIPoint(evm_symbol_t *sym)
{
	sym->linkFlags |= EVM_LINKFLAG_ENTRYPOINT;

	return EVM_OK;
}


evm_result_t evm_Link(evm_ocpstate_t *state, const char **conflict)
{
	evm_u32 i, numSymbols;
	evm_u32 currentSymbol;
	evm_u32 currentLinkSymbol;

	unsigned char linkflag;

	evm_symbolhash_t *symbols;

	// Check all symbols in order
	numSymbols = state->numSymbols;
	symbols = state->symbols;

	for(i=0;i<numSymbols;i++)
	{
		symbols[i].keep = 1;
		symbols[i].relocRemap = i;
	}

	for(currentSymbol=0;currentSymbol<numSymbols;currentSymbol++)
	{
		if(!(symbols[currentSymbol].sym.linkFlags & EVM_EXPORTED_MASK))
			continue;		// Only scan with exports

		// Look for a match
		currentLinkSymbol = state->symbolHashChains[symbols[currentSymbol].hash];
		while(currentLinkSymbol)
		{
			currentLinkSymbol--;	// To get the real index

			linkflag = (unsigned char)(symbols[currentLinkSymbol].sym.linkFlags & EVM_LINKFLAG_MASK);

			if(currentLinkSymbol != currentSymbol && linkflag != EVM_LINKFLAG_LOCAL && !strcmp(symbols[currentSymbol].sym.name, symbols[currentLinkSymbol].sym.name))
			{
				// Symbol match
				if(linkflag & EVM_EXPORTED_MASK)
				{
					// Conflicting exports
					*conflict = symbols[currentSymbol].sym.name;
					return EVM_ERR_SYMBOL_CONFLICT;
				}

				// Define the imported symbol
				symbols[currentLinkSymbol].keep = 0;
				symbols[currentLinkSymbol].relocRemap = currentSymbol;
			}

			currentLinkSymbol = symbols[currentLinkSymbol].nextSymbol;
		}
	}

	return evm_SymbolCleanup(state);
}


evm_result_t evm_SymbolCleanup(evm_ocpstate_t *state)
{
	evm_u32 i, validSymbols, numSymbols, numRelocations;
	evm_symbolhash_t *symbols;
	evm_relocation_t *relocations;

	symbols = state->symbols;
	numSymbols = state->numSymbols;

	relocations = state->relocations;
	numRelocations = state->numRelocations;

	validSymbols = 0;

	// Count and enumerate valid symbols
	for(i=0;i<numSymbols;i++)
	{
		if(symbols[i].keep)
			symbols[i].renumber = validSymbols++;
	}

	// Update relocations
	for(i=0;i<numRelocations;i++)
		relocations[i].symbolIndex = symbols[symbols[relocations[i].symbolIndex].relocRemap].renumber;

	// Resize
	for(i=0;i<numSymbols;i++)
	{
		if(symbols[i].keep && symbols[i].renumber != i)
			memcpy(symbols + symbols[i].renumber, symbols + i, sizeof(evm_symbolhash_t));
	}
	evm_saferealloc(state->symbols, sizeof(evm_symbolhash_t) * validSymbols);

	state->numSymbols = validSymbols;

	evm_RebuildHashChains(state);

	return EVM_OK;
}

evm_result_t evm_RelocationCleanup(evm_ocpstate_t *state)
{
	evm_u32 i, validRelocations, numSymbols, numRelocations;
	evm_symbolhash_t *symbols;
	evm_relocation_t *relocations;

	symbols = state->symbols;
	numSymbols = state->numSymbols;

	relocations = state->relocations;
	numRelocations = state->numRelocations;

	validRelocations = 0;
	for(i=0;i<numRelocations;i++)
	{
		if(!(symbols[relocations[i].symbolIndex].sym.linkFlags & EVM_LINKFLAG_RESOLVED))
		{
			// This relocation hasn't been resolved, so move or leave it
			if(validRelocations != i)
				memcpy(relocations + validRelocations, relocations + i, sizeof(evm_relocation_t));
			validRelocations++;
		}
	}

	// Resize
	evm_saferealloc(state->relocations, sizeof(evm_relocation_t) * validRelocations);

	state->numRelocations = validRelocations;

	return EVM_OK;
}

evm_result_t evm_Resolve(evm_ocpstate_t *state, evm_program_t *program, evm_u32 resolveFlags)
{
	evm_u32 i, numSymbols;
	evm_symbolhash_t *symbols;

	evm_u32 linkflags;

	if(!(resolveFlags & EVM_RESOLVE_MASK))
		return EVM_ERR_BAD_RESOLVE_FLAGS;

	symbols = state->symbols;
	numSymbols = state->numSymbols;

	// If the VM hasn't initialized any memory for finalized programs yet,
	// then set the existing size to 4 to compensate for NULL reads
	if(program)
	{
		if(!program->vmMemoryConsumed)
			program->vmMemoryConsumed = 4;
		if(!program->vmInstructionCount)
			program->vmInstructionCount = 1;
	}

	for(i=0;i<numSymbols;i++)
	{
		linkflags = symbols[i].sym.linkFlags;

		if(linkflags & EVM_LINKFLAG_RESOLVED)
			symbols[i].keep = 1;	// Keep symbols already resolved
		else if((linkflags & EVM_LINKFLAG_MASK) == EVM_LINKFLAG_IMPORT)
		{
			if(resolveFlags & EVM_RESOLVE_IMPORTS)
			{
				SAFECALL(evm_ResolveSymbol(state, program, symbols + i));
				symbols[i].keep = 0;
			}
			else
				symbols[i].keep = 1;
		}
		else
		{
			// Unresolved export or local
			if(resolveFlags & EVM_RESOLVE_NONIMPORTS)
			{
				SAFECALL(evm_ResolveSymbol(state, program, symbols + i));
				symbols[i].keep = (unsigned char)((symbols[i].sym.linkFlags & EVM_LINKFLAG_MASK) != EVM_LINKFLAG_LOCAL);
			}
			else
				symbols[i].keep = 1;
		}

		symbols[i].relocRemap = i;	// So I don't have to do this later
	}

	// Apply relocations to all resolved symbols
	evm_ApplyRelocations(state);

	// Clean up the symbol table
	evm_SymbolCleanup(state);

	return EVM_OK;
}

void evm_RebuildHashChains(evm_ocpstate_t *state)
{
	evm_u32 i, numSymbols;
	evm_symbolhash_t *symbolhashes;
	evm_u32 *hashchains;

	hashchains = state->symbolHashChains;
	symbolhashes = state->symbols;
	numSymbols = state->numSymbols;

	for(i=0;i<256;i++)
		hashchains[i] = 0;


	for(i=0;i<numSymbols;i++)
	{
		if((symbolhashes[i].sym.linkFlags & EVM_LINKFLAG_MASK) != EVM_LINKFLAG_LOCAL)
		{
			// Add to the hash chain
			symbolhashes[i].nextSymbol = hashchains[symbolhashes[i].hash]+1;
			hashchains[symbolhashes[i].hash] = i+1;
		}
	}
}

evm_result_t evm_ResolveSymbol(evm_ocpstate_t *state, evm_program_t *program, evm_symbolhash_t *sym)
{
	if((sym->sym.linkFlags & EVM_LINKFLAG_MASK) == EVM_LINKFLAG_IMPORT)
	{
		if(!state->resolve_symbol)
			return EVM_ERR_COULD_NOT_RESOLVE_IMPORT;

		if(state->resolve_symbol(state, sym->sym.name, &sym->sym.value) != EVM_OK)
			return EVM_ERR_COULD_NOT_RESOLVE_IMPORT;
	}
	else
	{
		switch(sym->sym.linkFlags & EVM_SEG_MASK)
		{
		case EVM_SEG_CODE:
			sym->sym.value += program->vmInstructionCount;
			break;
		case EVM_SEG_LIT:
			sym->sym.value += state->dataSegSizeDecoded;
		case EVM_SEG_DATA:
			sym->sym.value += state->bssSegSize;
		case EVM_SEG_BSS:
			sym->sym.value += program->vmMemoryConsumed;
		}
	}

	// Mark as resolved
	sym->sym.linkFlags |= EVM_LINKFLAG_RESOLVED;

	return EVM_OK;
}


evm_result_t evm_ApplyRelocations(evm_ocpstate_t *state)
{
	evm_u32 i, numRelocations;
	evm_symbolhash_t *symbolhashes;
	evm_symbolhash_t *symbolhash;
	evm_u32 *hashchains;
	evm_relocation_t *relocations;

	evm_u32 result, offset;

	void *resolutionbuffer;
	evm_u32 resolutionlimit;

	hashchains = state->symbolHashChains;
	symbolhashes = state->symbols;

	numRelocations = state->numRelocations;
	relocations = state->relocations;

	for(i=0;i<numRelocations;i++)
	{
		symbolhash = symbolhashes + relocations[i].symbolIndex;

		if(symbolhashes[relocations[i].symbolIndex].sym.linkFlags & EVM_LINKFLAG_RESOLVED)
		{
			// Resolve this relocation
			if(relocations[i].table  == EVM_SEG_CODE)
			{
				resolutionbuffer = state->codeseg;
				resolutionlimit = state->codeSegSize;
			}
			else
			{
				resolutionbuffer = state->dataseg;
				resolutionlimit = state->dataSegSize;
			}

			// Make sure it'll copy correctly
			offset = relocations[i].tableOffset;
			if(offset + 4 > resolutionlimit)
				return EVM_ERR_BAD_RELOCATION;

			// Relocations are always to sections that haven't been byteswapped yet,
			// so copy the data unbyteswapped
			result = evm_BigLong(relocations[i].valueIncrement + symbolhashes[relocations[i].symbolIndex].sym.value);

			// Copy it
			memcpy(BYTEOFFSET(resolutionbuffer, offset), &result, 4);
		}
	}

	return evm_RelocationCleanup(state);
}


evm_result_t EVM_EXPORT evm_FindExport(evm_ocpstate_t *state, const char *name, evm_symbol_t **sym)
{
	evm_symbolhash_t *symbols;
	evm_u32 numSymbols, i;

	unsigned char hash;

	symbols = state->symbols;
	numSymbols = state->numSymbols;

	hash = evm_hashstring(name);

	// TODO: Use the hash chains
	for(i=0;i<numSymbols;i++)
	{
		if((symbols[i].sym.linkFlags & EVM_EXPORTED_MASK) && symbols[i].hash == hash && !strcmp(symbols[i].sym.name, name))
		{
			*sym = &symbols[i].sym;
			return EVM_OK;
		}
	}

	return EVM_ERR_SYMBOL_NOT_FOUND;
}

evm_result_t evm_Localize(evm_ocpstate_t *state)
{
	evm_symbolhash_t *symbols;
	evm_u32 numSymbols, i;

	symbols = state->symbols;
	numSymbols = state->numSymbols;

	for(i=0;i<numSymbols;i++)
	{
		if((symbols[i].sym.linkFlags & EVM_LINKFLAG_MASK) == EVM_LINKFLAG_EXPORT)
		{
			// Convert to local
			symbols[i].sym.name[0] = '\0';
			symbols[i].sym.linkFlags = (symbols[i].sym.linkFlags & ~EVM_LINKFLAG_MASK) | EVM_LINKFLAG_LOCAL;
		}
	}

	evm_RebuildHashChains(state);

	return EVM_OK;
}
