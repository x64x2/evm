#include <stdio.h>
#include <string.h>

#include "../common/evm.h"
#include "../common/evm_opcodes.h"

#define ENDL "\r\n"


static const char *opnames[] =
{
	"store1     ",
	"store2     ",
	"store4     ",


	"load1      ",
	"load2      ",
	"load4      ",


	"ltof       ",
	"utof       ",
	"ftol       ",


	"sex8       ",
	"sex16      ",


	"zhi24      ",
	"zhi16      ",


	"negl       ",
	"negf       ",


	"call       ",


	"bsl        ",
	"bsrl       ",
	"bsru       ",


	"bnot       ",


	"band       ",
	"bor        ",
	"bxor       ",


	"addf       ",
	"subf       ",
	"mulf       ",
	"divf       ",


	"mulu       ",
	"divu       ",
	"modu       ",


	"addl       ",
	"subl       ",
	"mull       ",
	"divl       ",
	"modl       ",


	"jump       ",


	"discard    ",


	"skipnext   ",







	"addr_spr   ", 
	"addr_arg   ",


	"beginsub   ",
	"endsub     ",


	"copy       ",


	"beq        ",
	"beqf       ",
	"bne        ",
	"bnef       ",


	"bgef       ",
	"bgeu       ",
	"bgel       ",


	"bgtf       ",
	"bgtu       ",
	"bgtl       ",


	"blef       ",
	"bleu       ",
	"blel       ",


	"bltf       ",
	"bltu       ",
	"bltl       ",


	"immediate  ",


	"store_spr4 ",

	"drop8      ",
	"drop16     ",
};

#define NUM_OPNAMES (sizeof(opnames) / sizeof(opnames[0]))


typedef struct evm_symbolhash_s
{
	evm_symbol_t sym;

	unsigned char hash;
	unsigned char keep;

	evm_u32 nextSymbol;

	evm_u32 relocRemap;		// Symbol index to remap relocations to
	evm_u32 renumber;		// What this symbol's index will be after cleanup
} evm_symbolhash_t;




void dumpsymbols(evm_ocpstate_t *state, FILE *outfile)
{
	evm_symbolhash_t *symbols;
	evm_u32 numSymbols, i;
	unsigned char linkFlags;

	symbols = state->symbols;
	numSymbols = state->numSymbols;

	fprintf(outfile, "---- Symbols ----" ENDL);

	for(i=0;i<numSymbols;i++)
	{
		linkFlags = symbols[i].sym.linkFlags;

		fprintf(outfile, "%6lu : ", i);

		if(linkFlags & EVM_LINKFLAG_RESOLVED)
			fprintf(outfile, "resolved   ");
		else
			fprintf(outfile, "unresolved ");

		switch(linkFlags & EVM_LINKFLAG_MASK)
		{
		case EVM_LINKFLAG_LOCAL:
			fprintf(outfile, "local      ");
			break;
		case EVM_LINKFLAG_EXPORT:
			fprintf(outfile, "export     ");
			break;
		case EVM_LINKFLAG_IMPORT:
			fprintf(outfile, "import     ");
			break;
		case EVM_LINKFLAG_ENTRYPOINT:
			fprintf(outfile, "entrypoint ");
			break;
		}

		if((linkFlags & EVM_LINKFLAG_MASK) != EVM_LINKFLAG_IMPORT)
		{
			switch(linkFlags & EVM_SEG_MASK)
			{
			case EVM_SEG_CODE:
				fprintf(outfile, "code ");
				break;
			case EVM_SEG_DATA:
				fprintf(outfile, "data ");
				break;
			case EVM_SEG_LIT:
				fprintf(outfile, "lit  ");
				break;
			case EVM_SEG_BSS:
				fprintf(outfile, "bss  ");
				break;
			}

			fprintf(outfile, "%08lx  ", symbols[i].sym.value);
		}
		else
			fprintf(outfile, "               ");

		if((linkFlags & EVM_LINKFLAG_MASK) != EVM_LINKFLAG_LOCAL)
			fprintf(outfile, "%s ", symbols[i].sym.name);

		fprintf(outfile, ENDL);
	}
	fprintf(outfile, ENDL);
}


void dumprelocations(evm_ocpstate_t *state, FILE *outfile)
{
	evm_relocation_t *relocations;
	evm_u32 numRelocations, i;

	relocations = state->relocations;
	numRelocations = state->numRelocations;

	fprintf(outfile, "---- Relocations ----" ENDL);

	fprintf(outfile, "seg     sym addr           inc" ENDL);
	for(i=0;i<numRelocations;i++)
	{
		switch(relocations[i].table)
		{
		case EVM_SEG_CODE:
			fprintf(outfile, "code ");
			break;
		case EVM_SEG_DATA:
			fprintf(outfile, "data ");
			break;
		case EVM_SEG_LIT:
			fprintf(outfile, "lit  ");
			break;
		case EVM_SEG_BSS:
			fprintf(outfile, "bss  ");
			break;
		}
		fprintf(outfile, "%6lu %08lx %9li" ENDL, relocations[i].symbolIndex, relocations[i].tableOffset, relocations[i].valueIncrement);
	}
	fprintf(outfile, ENDL);
}


const char asciidump[] = "................................ !\"#$%&\'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~.................................................................................................................................";


void writebytedump(FILE *outfile, unsigned char *buf, const char *overflowstr, evm_u32 len, evm_u32 rowlen, int dumpascii)
{
	evm_u32 rowOffset=0;
	evm_u32 i;

	while(len)
	{
		if(rowOffset == rowlen)
		{
			fputs(overflowstr, outfile);
			rowOffset = 0;
		}

		fprintf(outfile, "%02x ", *buf);

		len--;
		buf++;
		rowOffset++;

		if(rowOffset == rowlen || !len)
		{
			if(dumpascii)
			{
				fputs(overflowstr, outfile);
				buf -= rowOffset;
				for(i=0;i<rowOffset;i++)
					fprintf(outfile, "%c  ", asciidump[*buf++]);
			}
		}
	}
}


void dumpdata(evm_ocpstate_t *state, FILE *outfile)
{
	evm_u32 appMemOffset=4;
	evm_u32 objectMemPos=0;
	evm_u32 segMemPos=0;

	evm_u32 dataSegSize;
	evm_u32 dataSegSizeDecoded;

	unsigned char *buf;
	unsigned char runLength;

	dataSegSize = state->dataSegSize;
	dataSegSizeDecoded = state->dataSegSizeDecoded;

	buf = state->dataseg;

	if(!dataSegSize)
		return;

	fprintf(outfile, "---- Data ----" ENDL);
	fprintf(outfile, "seg      app      reloc    " ENDL);

	while(objectMemPos != dataSegSize || segMemPos != dataSegSizeDecoded)
	{
		if(objectMemPos >= dataSegSize)
		{
			fprintf(stderr, "Corrupt data segment");
			exit(1);
		}

		runLength = *buf++;
		objectMemPos++;

		if(!runLength || objectMemPos >= dataSegSize)
		{
			fprintf(stderr, "Corrupt data segment");
			exit(1);
		}

		// Write the byte run
		if(objectMemPos + runLength > dataSegSize || segMemPos + runLength > dataSegSizeDecoded)
		{
			fprintf(stderr, "Corrupt data segment");
			exit(1);
		}

		fprintf(outfile, "%08lx %08lx %08lx : ", segMemPos, segMemPos + appMemOffset, objectMemPos);
		writebytedump(outfile, buf, ENDL "                             ", runLength, 17, 0);
		fprintf(outfile, ENDL);

		buf += runLength;
		objectMemPos += runLength;
		segMemPos += runLength;
	}
	fprintf(outfile, ENDL);
}

void dumplit(evm_ocpstate_t *state, FILE *outfile)
{
	evm_u32 appMemOffset=4;
	evm_u32 segMemPos=0;

	evm_u32 litSegSize;

	evm_u32 runlen;

	unsigned char *buf;

	appMemOffset += state->bssSegSize + state->dataSegSizeDecoded;

	litSegSize = state->litSegSize;

	buf = state->litseg;

	if(!litSegSize)
		return;

	fprintf(outfile, "---- Lit ----" ENDL);
	fprintf(outfile, "addr     app      " ENDL);

	while(segMemPos < litSegSize)
	{
		runlen = litSegSize - segMemPos;

		if(runlen > 20)
			runlen = 20;

		fprintf(outfile, "%08lx %08lx : ", segMemPos, segMemPos + appMemOffset);
		writebytedump(outfile, buf, ENDL "                    ", runlen, 90, 1);
		fprintf(outfile, ENDL ENDL);

		buf += runlen;

		segMemPos += 20;
	}
	fprintf(outfile, ENDL);
}

#include "../libevm/evm_stackoffsets.h"

void dumpcode(evm_ocpstate_t *state, FILE *outfile)
{
	evm_u32 instructionNum=0;
	evm_u32 segMemPos=0;

	evm_u32 numInstructions;
	evm_u32 codeSegSize;

	evm_relocation_t *relocations;
	evm_u32 numRelocations;

	evm_symbol_t *sym;

	evm_symbolhash_t *symbols;
	evm_u32 numSymbols;

	unsigned char *buf;

	int foundReloc;
	int resolved;

	evm_i32 stackoffset;

	unsigned char opcode;
	evm_u32 arg;
	evm_u32 i;

	codeSegSize = state->codeSegSize;
	numInstructions = state->dataSegSizeDecoded;

	buf = state->codeseg;

	if(!codeSegSize)
		return;

	fprintf(outfile, "---- Code ----" ENDL);
	fprintf(outfile, "ic       appic      reloc    " ENDL);

	numRelocations = state->numRelocations;
	relocations = state->relocations;

	symbols = state->symbols;
	numSymbols = state->numSymbols;

	stackoffset = 0;

	while(segMemPos != codeSegSize && instructionNum != numInstructions)
	{
		if(segMemPos >= codeSegSize)
		{
			fprintf(stderr, "Corrupt code segment");
			exit(1);
		}

		for(i=0;i<numSymbols;i++)
		{
			sym = &symbols[i].sym;

			resolved = sym->linkFlags & EVM_LINKFLAG_RESOLVED;

			if( (sym->linkFlags & EVM_SEG_MASK) == EVM_SEG_CODE &&
					( (resolved && sym->value == instructionNum+1) ||
					(!resolved && sym->value == instructionNum) )
				)
			{
				if(!(sym->linkFlags & EVM_EXPORTED_MASK))
					break;

				fprintf(outfile, "%s:" ENDL, sym->name);
				break;
			}
		}

		opcode = *buf++;
		segMemPos++;

		if(opcode >= EVM_OP_NUM_OPCODES || segMemPos >= codeSegSize)
		{
			fprintf(stderr, "Corrupt code segment");
			exit(1);
		}


		for(i=0;i<NUM_STACK_OFFSETS;i++)
		{
			if(evm_stackoffsets[i].opcode == opcode)
				stackoffset += evm_stackoffsets[i].stackmod;
		}

		fprintf(outfile, "%08lx %08lx %08lx %s s(%2li) ", instructionNum, instructionNum+1, segMemPos, opnames[opcode], stackoffset);

		if(opcode >= EVM_OP_ARGOPS_START)
		{
			if(segMemPos+4 > codeSegSize)
			{
				fprintf(stderr, "Corrupt code segment");
				exit(1);
			}

			foundReloc = 0;

			for(i=0;i<numRelocations;i++)
			{
				if(relocations[i].table == EVM_SEG_CODE && relocations[i].tableOffset == segMemPos)
				{
					sym = &state->symbols[relocations[i].symbolIndex].sym;

					if((sym->linkFlags & EVM_LINKFLAG_MASK) != EVM_LINKFLAG_IMPORT)
					{
						switch(sym->linkFlags & EVM_SEG_MASK)
						{
						case EVM_SEG_CODE:
							fprintf(outfile, "code");
							break;
						case EVM_SEG_DATA:
							fprintf(outfile, "data");
							break;
						case EVM_SEG_LIT:
							fprintf(outfile, "lit");
							break;
						case EVM_SEG_BSS:
							fprintf(outfile, "bss");
							break;
						}
						fprintf(outfile, "[%08lx] ", sym->value);
					}

					if((sym->linkFlags & EVM_LINKFLAG_MASK) != EVM_LINKFLAG_LOCAL)
						fprintf(outfile, "!%s! ", sym->name);

					if(relocations[i].valueIncrement)
						fprintf(outfile, "+ %li", relocations[i].valueIncrement);

					foundReloc = 1;
					break;
				}
			}

			if(!foundReloc)
			{
				arg = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
				fprintf(outfile, "%08lx ", arg);
			}
			buf += 4;
			segMemPos += 4;
		}


		instructionNum++;
		fprintf(outfile, ENDL);
	}
}


void disassemble(FILE *infile, const char *outname)
{
	evm_header_t header;
	evm_ocpstate_t state;
	evm_u32 size;
	unsigned char *filebuffer;
	FILE *outfile;

	evm_result_t result;

	if(NUM_OPNAMES != EVM_OP_NUM_OPCODES)
	{
		fprintf(stderr, "Opcode miscount: %zu vs %i", NUM_OPNAMES, EVM_OP_NUM_OPCODES);
		exit(1);
	}

	memset(&state, 0, sizeof(evm_ocpstate_t));

	if(!fread(&header, sizeof(evm_header_t), 1, infile))
	{
		fprintf(stderr, "Read error");
		exit(1);
	}

	// Get file size
	evm_CalculateObjectFileSize(&header, &size);

	if(!(filebuffer = malloc(size)))
	{
		fprintf(stderr, "Out of memory");
		exit(1);
	}

	// Copy the header
	memcpy(filebuffer, &header, sizeof(evm_header_t));

	// Load the rest
	if(!fread(filebuffer+sizeof(evm_header_t), size - sizeof(evm_header_t), 1, infile))
	{
		fprintf(stderr, "Read error");
		exit(1);
	}

	// Load the object file
	result = evm_LoadObjectFile(&state, filebuffer, size);
	if(result)
	{
		fprintf(stderr, "evm_LoadObjectFile: %i\n", result);
		exit(1);
	}

	// Free up
	free(filebuffer);

	if(infile != stdin)
		fclose(infile);

	// Dump info
	if(!outname)
		outfile = stdout;
	else
	{
		if(!(outfile = fopen(outname, "wb")))
		{
			fprintf(stderr, "evm_LoadObjectFile: %i\n", result);
			exit(1);
		}
	}

	fprintf(outfile, "dataSeg offset: 00000004" ENDL);
	fprintf(outfile, "dataSeg size:   %lu bytes" ENDL, state.dataSegSizeDecoded);
	fprintf(outfile, "bssSeg  offset: %08lx" ENDL, 4+state.dataSegSizeDecoded);
	fprintf(outfile, "bssSeg  size:   %lu bytes" ENDL, state.bssSegSize);
	fprintf(outfile, "litSeg  offset: %08lx" ENDL, 4+state.dataSegSizeDecoded+state.bssSegSize);
	fprintf(outfile, "litSeg  size:   %lu bytes" ENDL, state.litSegSize);
	fprintf(outfile, "memory bounds:  %08lx" ENDL ENDL, 4+state.dataSegSizeDecoded+state.bssSegSize+state.litSegSize);

	dumpsymbols(&state, outfile);
	dumprelocations(&state, outfile);
	dumpdata(&state, outfile);
	dumplit(&state, outfile);
	dumpcode(&state, outfile);

	if(outname)
		fclose(outfile);
}
