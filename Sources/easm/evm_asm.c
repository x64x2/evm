#include <cstddef>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/evm.h"
#include "../common/evm_endian.h"
#include "../common/evm_opcodes.h"
#include "lexer.h"
#include "buffering.h"

void disassemble(FILE *f, const char *outname);

static FILE *infile;

dynamicbuffer_t codebuffer;
dynamicbuffer_t databuffer;
dynamicbuffer_t litbuffer;
dynamicbuffer_t relocationbuffer;
dynamicbuffer_t symbolreflist;

evm_u32 currentSymbol = 0;
evm_u32 dataSizeDecoded = 0;
evm_u32 bssSize = 0;

evm_u32 argSize;
evm_u32 localSize;

evm_u32 argOffset = 0;

evm_u32 numSymbols = 0;

unsigned char currentSegment = EVM_SEG_CODE;

typedef struct
{
	const char *opname;
	unsigned char opcode;
	int hasargument;
} opconversion_t;

#define EASM_OP_IGNORE 255




opconversion_t conversions[] = {
	//{"ARG, ...},					// converted into OP_STORE_SPR4
	//{"CVII4", ...},				// converted into sign extension
	//{"RET", EVM_OP_ENDSUB},		// argument is special-case
	//{"COMPILER", ...},				// other arguments get converted into SPR compileresses
	// F = parameter, L = local

	{"CNST", EVM_OP_IMMEDIATE, 1},

	{"ASGN?4", EVM_OP_STORE4},
	{"ASGN?2", EVM_OP_STORE2},
	{"ASGN?1", EVM_OP_STORE1},
	{"ASGNB", EVM_OP_COPY, 1},

	{"INDIR?4", EVM_OP_LOAD4},
	{"INDIR?2", EVM_OP_LOAD2},
	{"INDIR?1", EVM_OP_LOAD1},
	{"INDIRB", EASM_OP_IGNORE},	// ASGNB always follows anyway

	{"CVFI", EVM_OP_FTOL, 1},
	{"CVIF", EVM_OP_LTOF, 1},
	{"CVUF", EVM_OP_UTOF, 1},
	{"CVUU1", EVM_OP_ZHI24, 1},
	{"CVUU2", EVM_OP_ZHI16, 1},
	{"CV", EASM_OP_IGNORE, 1},		// ignore all other conversions

	{"CALLV", EVM_OP_CALL},

	//{"COMPILERG", EVM_OP_IMMEDIATE},

	{"LSH", EVM_OP_BSL},
	{"RSHI", EVM_OP_BSRI},
	{"RSHU", EVM_OP_BSRU},

	{"BAND", EVM_OP_BAND},
	{"BCOM", EVM_OP_BNOT},
	{"BOR", EVM_OP_BOR},
	{"BXOR", EVM_OP_BXOR},
#define EVM_OP_COMPILEF
	{"COMPILEF", EVM_OP_COMPILEF},
	{"SUBF", EVM_OP_SUBF},
	{"MULF", EVM_OP_MULF},
	{"DIVF", EVM_OP_DIVF},
	{"NEGF", EVM_OP_NEGF},

	{"MULU", EVM_OP_MULU},
	{"DIVU", EVM_OP_DIVU},
	{"MODU", EVM_OP_MODU},
#define EVM_OP_COMPILEL
	{"COMPILE", EVM_OP_COMPILEL},
	{"SUB", EVM_OP_SUBL},
	{"MUL", EVM_OP_MULL},
	{"DIV", EVM_OP_DIVL},
	{"MOD", EVM_OP_MODL},
	{"NEG", EVM_OP_NEGL},

	{"EQF", EVM_OP_BEQF, 1},
	{"EQ", EVM_OP_BEQ, 1},
	{"NEF", EVM_OP_BNEF, 1},
	{"NE", EVM_OP_BNE, 1},

	{"GEI", EVM_OP_BGEL, 1},
	{"GEF", EVM_OP_BGEF, 1},
	{"GE", EVM_OP_BGEU, 1},

	{"GTI", EVM_OP_BGTL, 1},
	{"GTF", EVM_OP_BGTF, 1},
	{"GT", EVM_OP_BGTU, 1},

	{"LEI", EVM_OP_BLEL, 1},
	{"LEF", EVM_OP_BLEF, 1},
	{"LE", EVM_OP_BLEU, 1},

	{"LTI", EVM_OP_BLTL, 1},
	{"LTF", EVM_OP_BLTF, 1},
	{"LT", EVM_OP_BLTU, 1},

	{"JUMP", EVM_OP_JUMP},

	{"DROP8", EVM_OP_DROP8, 1},
	{"DROP16", EVM_OP_DROP16, 1},

	{"LOGICSKIP", EVM_OP_SKIPNEXT},

	{"pop", EVM_OP_DISCARD},
};



typedef struct evm_hashlink_s
{
	evm_symbol_t sym;

	evm_u16 hashValue;

	evm_u32 symbolNum;
	evm_u32 renumber;

	int defined;
	int uses;

	struct evm_hashlink_s *next;
} easm_hashlink_t;

easm_hashlink_t *symbolHashChains[0x10000];


void expecttoken(FILE *f)
{
	gettoken(f);
	if(!lextoken)
	{
		fprintf(stderr, "[%i] Unexpected end of file", lex_line);
		exit(1);
	}
}


evm_u16 hashstring(const char *str)
{
	evm_u16 hash = 0x7FF;

	while(*str)
	{
		if(hash & 0x800)
			hash = hash + (hash << 1) + (*str++) + 1;
		else
			hash = hash + (hash << 1) + (*str++);
	}

	return hash;
}



static int matchstring(const char *str, const char *mask)
{
	char m, s;

	for(;;)
	{
		m = *mask++;
		s = *str++;

		if(!m)
			return 1;	// mask ended, so it's good

		if(!s || (m != '?' && m != s))
			return 0;	// string ended before the mask did, or char mismatched
	}
}


evm_u32 relocationoffset(void)
{
	switch(currentSegment)
	{
	case EVM_SEG_CODE:
		// +1 because arguments are parsed before the opcode is written
		return codebuffer.offset+1;
	case EVM_SEG_BSS:
		return bssSize;
	case EVM_SEG_LIT:
		return litbuffer.offset;
	case EVM_SEG_DATA:
		// +1 because run length is written after the symbol is parsed
		return databuffer.offset+1;
	}

	return 0;	// stfu compiler
}


evm_u32 symboloffset(void)
{
	switch(currentSegment)
	{
	case EVM_SEG_CODE:
		return currentSymbol;
	case EVM_SEG_BSS:
		return bssSize;
	case EVM_SEG_LIT:
		return litbuffer.offset;
	case EVM_SEG_DATA:
		return dataSizeDecoded;
	}

	return 0;	// stfu compiler
}


easm_hashlink_t *findsymbol(const char *sym, evm_u16 hash)
{
	easm_hashlink_t *link;
	evm_u32 symbolID;

	// Search the hash chain for a match
	link = symbolHashChains[hash];

	while(link)
	{
		if(link->hashValue == hash && !strcmp(sym, link->sym.name))
			return link;

		link = link->next;
	}

	// Doesn't exist, make it
	link = malloc(sizeof(easm_hashlink_t));
	if(!link)
	{
		fprintf(stderr, "Could not allocate easm_hashlink_t");
		exit(1);
	}

	// Create a new symbol table entry
	symbolID = numSymbols++;

	link->defined = 0;
	link->uses = 0;
	link->hashValue = hash;
	link->next = symbolHashChains[hash];
	strcpy(link->sym.name, sym);
	link->sym.value = 0;
	link->sym.linkFlags = EVM_LINKFLAG_LOCAL;
	link->symbolNum = symbolID;

	symbolHashChains[hash] = link;

	// Compile a reference to the symbol list
	writebuffer(&symbolreflist, &link, sizeof(&link));

	return link;
}


void createrelocation(const char *sym, evm_u16 hash, unsigned char seg, evm_u32 tableoffset, evm_u32 increment)
{
	easm_hashlink_t *link;
	evm_relocation_t reloc;

	link = findsymbol(sym, hash);

	link->uses++;

	reloc.symbolIndex = link->symbolNum;
	reloc.table = seg;
	reloc.tableOffset = tableoffset;
	reloc.valueIncrement = increment;

	writebuffer(&relocationbuffer, &reloc, sizeof(evm_relocation_t));
}


evm_i32 parsesymbol(FILE *f, int *isSymbol)
{
	char symboltext[256];
	const char *numCheck;
	char c;
	int isNumber = 1;

	evm_u16 hash;

	int negate = 0;

	evm_i32 returnValue=0;
	evm_i32 returnOffset = 0;

	// See if it's a number
	if(lextoken[0] == '-')
	{
		negate = 1;
		expecttoken(f);
	}

	numCheck = lextoken;
	while((c = (*numCheck++)))
	{
		if(c > '9' || c < '0')
		{
			isNumber = 0;
			break;
		}
	}

	if(isNumber)
		returnValue = atol(lextoken);
	else
		strcpy(symboltext, lextoken);

	if(negate)
	{
		if(!isNumber)
		{
			fprintf(stderr, "[%i] Non-number after -", lex_line);
			exit(1);
		}

		returnValue = -returnValue;
	}

	gettoken(f);

	if(!lextoken)
		returnOffset = 0;
	else if(lextoken[0] == '+')
	{
		expecttoken(f);
		returnOffset = atol(lextoken);
		gettoken(f);
	}
	else if(lextoken[0] == '-')
	{
		expecttoken(f);
		returnOffset = -atol(lextoken);
		gettoken(f);
	}

	// If it's not a symbol, return now
	if(isNumber)
	{
		if(isSymbol)
			*isSymbol = 0;
		return returnValue + returnOffset;
	}

	if(isSymbol)
		*isSymbol = 0;

	// Create a relocation for the symbol
	hash = hashstring(symboltext);

	createrelocation(symboltext, hash, currentSegment, relocationoffset(), returnOffset);

	return 0;
}


static int numopconversions = sizeof(conversions) / sizeof(conversions[0]);


evm_i32 parsenumber(FILE *f)
{
	evm_i32 base, offset;

	if(lextoken[0] == '-')
	{
		expecttoken(f);
		base = -atol(lextoken);
	}
	else
		base = atol(lextoken);

	gettoken(f);

	if(lextoken)
	{
		if(lextoken[0] == '+')
		{
			expecttoken(f);
			offset = atol(lextoken);
			gettoken(f);

			return base + offset;
		}
		else if(lextoken[0] == '-')
		{
			expecttoken(f);
			offset = atol(lextoken);
			gettoken(f);

			return base - offset;
		}
	}

	return base;
}


void parsecompileress(FILE *f)
{
	unsigned char buf[5];

	evm_i32 value;

	if(currentSegment != EVM_SEG_DATA)
	{
		fprintf(stderr, "[%i] byte 4 and compileress are not valid outside of data", lex_line);
		exit(1);
	}

	value = parsesymbol(f, NULL);

	// Write a big-endian run
	buf[0] = 4;
	buf[1] = (unsigned char)(value >> 24);
	buf[2] = (unsigned char)(value >> 16);
	buf[3] = (unsigned char)(value >> 8);
	buf[4] = (unsigned char)(value);

	dataSizeDecoded += 4;

	writebuffer(&databuffer, buf, 5);
}


void parsebyte4(FILE *f)
{
	unsigned char buf[5];

	evm_i32 value;

	if(currentSegment != EVM_SEG_DATA)
	{
		fprintf(stderr, "[%i] byte 4 and compileress are not valid outside of data", lex_line);
		exit(1);
	}

	value = parsenumber(f);

	// Write a big-endian run
	buf[0] = 4;
	buf[1] = (unsigned char)(value >> 24);
	buf[2] = (unsigned char)(value >> 16);
	buf[3] = (unsigned char)(value >> 8);
	buf[4] = (unsigned char)(value);

	dataSizeDecoded += 4;

	writebuffer(&databuffer, buf, 5);
}

void parsebyte2(FILE *f)
{
	unsigned char buf[3];

	evm_i32 value;

	if(currentSegment != EVM_SEG_DATA)
	{
		fprintf(stderr, "[%i] byte 2 is not valid outside of data", lex_line);
		exit(1);
	}

	value = parsenumber(f);

	// Write a big-endian run
	buf[0] = 2;
	buf[1] = (unsigned char)(value >> 8);
	buf[2] = (unsigned char)(value);

	dataSizeDecoded += 2;

	writebuffer(&databuffer, buf, 3);
}

void parsebyte1(FILE *f)
{
	unsigned char buf[2];

	evm_i32 value;

	if(currentSegment != EVM_SEG_DATA && currentSegment != EVM_SEG_LIT)
	{
		fprintf(stderr, "[%i] byte 1 is not valid outside of data and lit", lex_line);
		exit(1);
	}

	value = parsenumber(f);

	buf[0] = 1;
	buf[1] = (unsigned char)(value);

	if(currentSegment == EVM_SEG_DATA)
	{
		dataSizeDecoded += 1;
		writebuffer(&databuffer, buf, 2);
	}
	else
		writebuffer(&litbuffer, buf+1, 1);
}


void blankrun(evm_u32 numbytes)
{
	unsigned char zero = 0;
	unsigned char smallbytes;

	if(currentSegment == EVM_SEG_CODE)
	{
		fprintf(stderr, "[%i] skip and align are not valid in code", lex_line);
		exit(1);
	}

	if(!numbytes)
		return;

	switch(currentSegment)
	{
	case EVM_SEG_DATA:
		// Data encoding only allows 255 char runs.
		while(numbytes > 255)
		{
			blankrun(255);
			numbytes -= 255;
		}

		dataSizeDecoded += numbytes;
		writebuffer(&databuffer, &smallbytes, 1);
		while(numbytes)
		{
			writebuffer(&databuffer, &zero, 1);
			numbytes--;
		}
		break;
	case EVM_SEG_LIT:
		while(numbytes)
		{
			writebuffer(&litbuffer, &zero, 1);
			numbytes--;
		}
		break;
	case EVM_SEG_BSS:
		bssSize += numbytes;
		break;
	}
}


void writesymbol(unsigned char code, evm_u32 argument)
{
	unsigned char buffer[5];

	buffer[0] = code;

	if(code < EVM_OP_ARGOPS_START)
		writebuffer(&codebuffer, buffer, 1);
	else
	{
		// Write argument in big endian
		buffer[1] = (unsigned char)(argument >> 24);
		buffer[2] = (unsigned char)(argument >> 16);
		buffer[3] = (unsigned char)(argument >> 8);
		buffer[4] = (unsigned char)(argument);

		writebuffer(&codebuffer, buffer, 5);
	}

	currentSymbol++;
}


void writeobjectfile(const char *outname)
{
	evm_header_t header;
	evm_u32 v;
	evm_u32 symbolTableSize=0;
	unsigned char linkmode;

	evm_u32 realNumSymbols;

	easm_hashlink_t **hashlist;
	evm_relocation_t *relocationref;

	int nitems, i;

	FILE *outfile;

	// Make sure all symbols were defined and calculate how much space
	// they will consume
	nitems = symbolreflist.offset / sizeof(easm_hashlink_t *);
	hashlist = symbolreflist.data;

	realNumSymbols = 0;
	for(i=0;i<nitems;i++)
	{
		if(!hashlist[i]->defined)
		{
			fprintf(stderr, "Symbol %s not defined", hashlist[i]->sym.name);
			exit(1);
		}

		if(!(hashlist[i]->sym.linkFlags & EVM_EXPORTED_MASK) && !hashlist[i]->uses)
			continue;

		hashlist[i]->renumber = realNumSymbols++;

		linkmode = hashlist[i]->sym.linkFlags & EVM_LINKFLAG_MASK;

		if(linkmode != EVM_LINKFLAG_LOCAL)
			symbolTableSize += strlen(hashlist[i]->sym.name) + 1;

		if(linkmode != EVM_LINKFLAG_IMPORT)
			symbolTableSize += sizeof(evm_u32);

		symbolTableSize++;
	}

	// Open output
	if(outname)
	{
		if(!(outfile = fopen(outname, "wb")))
		{
			fprintf(stderr, "could not open output file");
			exit(1);
		}
	}
	else
		outfile = stdout;

	// Write the header
	header.identifier[0] = 'D';
	header.identifier[1] = '3';
	header.identifier[2] = 'O';
	header.identifier[3] = 'B';

	header.symbolCount = evm_BigLong(realNumSymbols);
	header.symbolTableSize = evm_BigLong(symbolTableSize);

	header.relocationCount = evm_BigLong(relocationbuffer.offset / sizeof(evm_relocation_t));

	header.dataTableSize = evm_BigLong(databuffer.offset);
	header.dataTableSizeDecoded = evm_BigLong(dataSizeDecoded);

	header.litTableSize = evm_BigLong(litbuffer.offset);

	header.codeTableSize = evm_BigLong(codebuffer.offset);
	header.codeTableNumSymbols = evm_BigLong(currentSymbol);

	header.bssSize = evm_BigLong(bssSize);

	fwrite(&header, sizeof(header), 1, outfile);

	// Write the relocation table
	nitems = relocationbuffer.offset / sizeof(evm_relocation_t);
	relocationref = relocationbuffer.data;
	for(i=0;i<nitems;i++)
	{
		relocationref->symbolIndex = evm_BigLong(hashlist[relocationref->symbolIndex]->renumber);
		relocationref->tableOffset = evm_BigLong(relocationref->tableOffset);
		relocationref->valueIncrement = evm_BigLong(relocationref->valueIncrement);

		memset(relocationref->unused, 0, sizeof(relocationref->unused));

		relocationref++;
	}

	fwrite(relocationbuffer.data, sizeof(evm_relocation_t), nitems, outfile);

	// Write the data table
	fwrite(databuffer.data, databuffer.offset, 1, outfile);

	// Write the lit table
	fwrite(litbuffer.data, litbuffer.offset, 1, outfile);

	// Write the code table
	fwrite(codebuffer.data, codebuffer.offset, 1, outfile);

	// Write the symbol table
	nitems = symbolreflist.offset / sizeof(easm_hashlink_t *);
	for(i=0;i<nitems;i++)
	{
		if(!(hashlist[i]->sym.linkFlags & EVM_EXPORTED_MASK) && !hashlist[i]->uses)
			continue;

		linkmode = hashlist[i]->sym.linkFlags;
		fputc(linkmode, outfile);

		linkmode &= EVM_LINKFLAG_MASK;

		if(linkmode != EVM_LINKFLAG_LOCAL)
		{
			fputc(strlen(hashlist[i]->sym.name), outfile);
			fwrite(hashlist[i]->sym.name, strlen(hashlist[i]->sym.name), 1, outfile);
			symbolTableSize += strlen(hashlist[i]->sym.name) + 1;
		}

		if(linkmode != EVM_LINKFLAG_IMPORT)
		{
			v = evm_BigLong(hashlist[i]->sym.value);
			fwrite(&v, sizeof(v), 1, outfile);
		}
	}

	if(outname)
		fclose(outfile);
}



void assemble(FILE *infile, const char *outname)
{
	easm_hashlink_t *link;
	evm_u32 argvalue;
	evm_u32 offset;
	int i;

	gettoken(infile);

	// Lex stuff
	while(lextoken)
	{
		if(!strcmp(lextoken, "compileress"))
		{
			expecttoken(infile);
			parsecompileress(infile);
		}
		else if(!strcmp(lextoken, "byte"))
		{
			expecttoken(infile);

			switch(atoi(lextoken))
			{
			case 1:
				expecttoken(infile);
				parsebyte1(infile);
				break;
			case 2:
				expecttoken(infile);
				parsebyte2(infile);
				break;
			case 4:
				expecttoken(infile);
				parsebyte4(infile);
				break;
			default:
				exit(1);
				break;
			}
		}
		else if(!strcmp(lextoken, "import"))
		{
			expecttoken(infile);
			link = findsymbol(lextoken, hashstring(lextoken));

			link->sym.linkFlags = EVM_LINKFLAG_IMPORT;
			link->defined = 1;
			gettoken(infile);
		}
		else if(!strcmp(lextoken, "export"))
		{
			expecttoken(infile);
			link = findsymbol(lextoken, hashstring(lextoken));

			link->sym.linkFlags = (unsigned char)((link->sym.linkFlags & ~EVM_LINKFLAG_MASK) | EVM_LINKFLAG_EXPORT);
			gettoken(infile);
		}
		else if(!strcmp(lextoken, "code"))
		{
			currentSegment = EVM_SEG_CODE;
			gettoken(infile);
		}
		else if(!strcmp(lextoken, "data"))
		{
			currentSegment = EVM_SEG_DATA;
			gettoken(infile);
		}
		else if(!strcmp(lextoken, "bss"))
		{
			currentSegment = EVM_SEG_BSS;
			gettoken(infile);
		}
		else if(!strcmp(lextoken, "lit"))
		{
			currentSegment = EVM_SEG_LIT;
			gettoken(infile);
		}
		else if(!strcmp(lextoken, "proc"))
		{
			expecttoken(infile);

			// Define the procedure
			link = findsymbol(lextoken, hashstring(lextoken));
			link->defined = 1;
			link->sym.linkFlags |= EVM_SEG_CODE;
			link->sym.value = symboloffset();

			expecttoken(infile);
			localSize = atoi(lextoken);
			expecttoken(infile);
			argSize = atoi(lextoken);

			// Align
			localSize = (localSize + 3) & ~3;
			argSize = (argSize + 3) & ~3;

			writesymbol(EVM_OP_BEGINSUB, localSize + argSize + 8);

			gettoken(infile);
		}
		else if(!strcmp(lextoken, "endproc"))
		{
			expecttoken(infile);	// I already know the details
			expecttoken(infile);
			expecttoken(infile);

			writesymbol(EVM_OP_IMMEDIATE, 0);
			writesymbol(EVM_OP_ENDSUB, localSize + argSize + 8);

			gettoken(infile);
		}
		else if(!strcmp(lextoken, "align"))
		{
			expecttoken(infile);

			argvalue = atoi(lextoken);
			offset = symboloffset();

			if(offset % argvalue)
				blankrun(argvalue - (offset % argvalue));

			gettoken(infile);
		}
		else if(!strcmp(lextoken, "skip"))
		{
			expecttoken(infile);

			argvalue = atoi(lextoken);
			blankrun(argvalue);

			gettoken(infile);
		}
		else if(!strcmp(lextoken, "file") || !strcmp(lextoken, "line"))
		{
			// Ignore these
			expecttoken(infile);
			gettoken(infile);
		}
		else if(matchstring(lextoken, "LABEL"))
		{
			expecttoken(infile);

			// Define the symbol
			link = findsymbol(lextoken, hashstring(lextoken));
			link->defined = 1;
			link->sym.linkFlags |= currentSegment;
			link->sym.value = symboloffset();

			gettoken(infile);
		}
		else if(matchstring(lextoken, "ARG"))
		{
			// Store args in marshalling space
			writesymbol(EVM_OP_STORE_SPR4, argOffset + 8);
			argOffset += 4;

			gettoken(infile);
		}
		else if(!strcmp(lextoken, "CVII4"))
		{
			// Sign extensions depend on how many bytes the input is
			expecttoken(infile);

			switch(atoi(lextoken))
			{
			case 1:
				writesymbol(EVM_OP_SEX8, 0);
				break;
			case 2:
				writesymbol(EVM_OP_SEX16, 0);
				break;
			default:
				fprintf(stderr, "[%i] Bad sign extension", lex_line);
				exit(1);
			}

			gettoken(infile);
		}
		else if(matchstring(lextoken, "RET"))
		{
			// Calculate stack increase for returns
			writesymbol(EVM_OP_ENDSUB, 8 + argSize + localSize);
			gettoken(infile);
		}
		else if(matchstring(lextoken, "COMPILER"))
		{
			if(lextoken[4] == 'F')
			{
				expecttoken(infile);

				int EVM_OP_COMPILER_ARG;
				// Parameter compileress, which is in the marshalling space
				// of the previous call
				writesymbol(EVM_OP_COMPILER_ARG, 8 + argSize + localSize + 8 + parsenumber(infile));
			}
			else if(lextoken[4] == 'L')
			{
				expecttoken(infile);

				int EVM_OP_COMPILER_SPR;
				// Local compileress
				writesymbol(EVM_OP_COMPILER_SPR, 8 + argSize + parsenumber(infile));
			}
			else
			{
				// Global compileress, which is just an immediate
				expecttoken(infile);

				writesymbol(EVM_OP_IMMEDIATE, parsesymbol(infile, NULL));
			}
		}
		else if(matchstring(lextoken, "DROP"))
		{
			expecttoken(infile);
			writesymbol( (lextoken[4] == '8') ? EVM_OP_DROP8 : EVM_OP_DROP16, 8 + argSize + localSize + 8 + parsenumber(infile));
		}
		else if(matchstring(lextoken, "CALL"))
		{
			// Reset the arg offset
			argOffset = 0;
			writesymbol(EVM_OP_CALL, 0);
			gettoken(infile);
		}
		else
		{
			// Simple opcodes
			for(i=0;i<numopconversions;i++)
			{
				if(matchstring(lextoken, conversions[i].opname))
				{
					if(conversions[i].hasargument)
					{
						expecttoken(infile);

						if(conversions[i].opcode != EASM_OP_IGNORE)
							writesymbol(conversions[i].opcode, parsesymbol(infile, NULL));
						else
							parsenumber(infile);
						break;
					}
					else
					{
						if(conversions[i].opcode != EASM_OP_IGNORE)
							writesymbol(conversions[i].opcode, 0);
						gettoken(infile);
						break;
					}
				}
			}

			if(i == numopconversions)
			{
				fprintf(stderr, "[%i] Unknown directive: '%s'", lex_line, lextoken);
				exit(1);
			}
		}
	}

	// Align data segment
	if(dataSizeDecoded & 3)
	{
		currentSegment = EVM_SEG_DATA;
		blankrun(4 - (dataSizeDecoded & 3));
	}

	// Align BSS segment
	if(bssSize & 3)
		bssSize = (bssSize + 3) & ~3;

	// Finished
	writeobjectfile(outname);
}



int main(int argc, char **argv)
{
	int disasm=0;

	if(argc >= 2 && !strcmp(argv[1], "-d"))
	{
		disasm = 1;
		argc--;
		argv++;
	}

	if(argc >= 2)
	{
		if(argv[1][0] != '-')
		{
			infile = fopen(argv[1], "rb");
			if(!infile)
			{
				fprintf(stderr, "ERROR: could not open input file");
				exit(1);
			}
		}
	}
	else
		infile = stdin;

	if(argc >= 3)
	{
		if(disasm)
			disassemble(infile, argv[2]);
		else
			assemble(infile, argv[2]);
	}
	else
	{
		if(disasm)
			disassemble(infile, NULL);
		else
			assemble(infile, NULL);
	}

	return 0;
}
