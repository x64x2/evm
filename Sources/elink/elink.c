#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../common/evm.h"

void usage(void)
{
	fprintf(stderr, "evm linker\n");
	fprintf(stderr, "Usage: elink <app|lib|obj> <parameters>\n");
	fprintf(stderr, "     In app mode, first parameter is stack size\n");
	fprintf(stderr, "          +symbol   : Mark symbol as an entry point\n");
	fprintf(stderr, "          -filename : Set output filename\n");
	fprintf(stderr, "          filename  : Add object file\n");
}


void writebytes(const void *buf, evm_u32 size, void *handle)
{
	fwrite(buf, size, 1, handle);
}



static unsigned char fakeobjectfile[] = {
	'D', '3', 'O', 'B', 0, 0, 0, 18, 0, 0, 0, 1, 0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,
	255,255,255,255,
	9,12,
	'$', '_', '_', 'e', 'v', 'm', '_', 's', 't', 'a', 'c', 'k',
	255,255,255,255,
};

#define FOBJ_OFFSET1 36
#define FOBJ_OFFSET2 54
#define FOBJ_SIZE    58



int main(int argc, char **argv)
{
	int i;
	int firstparm;
	evm_ocpstate_t state;
	evm_program_t emptyProgram;
	evm_header_t header;
	const char *outname;
	FILE *f;

	evm_symbol_t *sym;

	evm_u32 size;

	unsigned char *filebuffer;

	const char *conflict;

	evm_result_t result;

	outname = NULL;

	memset(&state, 0, sizeof(evm_ocpstate_t));
	memset(&emptyProgram, 0, sizeof(evm_program_t));

	if(argc < 3 || (strcmp(argv[1], "app") && strcmp(argv[1], "add") && strcmp(argv[1], "obj") && strcmp(argv[1], "lib")))
	{
		usage();
		return 1;
	}

	firstparm = 2;

	if(!strcmp(argv[1], "app"))
	{
		if(argc < 4)
		{
			usage();
			return 1;
		}

		size = atoi(argv[2]);

		fakeobjectfile[FOBJ_OFFSET1+0] = fakeobjectfile[FOBJ_OFFSET2+0] = (size >> 24) & 255;
		fakeobjectfile[FOBJ_OFFSET1+1] = fakeobjectfile[FOBJ_OFFSET2+1] = (size >> 16) & 255;
		fakeobjectfile[FOBJ_OFFSET1+2] = fakeobjectfile[FOBJ_OFFSET2+2] = (size >> 8 ) & 255;
		fakeobjectfile[FOBJ_OFFSET1+3] = fakeobjectfile[FOBJ_OFFSET2+3] = (size >> 0 ) & 255;

		result = evm_LoadObjectFile(&state, fakeobjectfile, FOBJ_SIZE);
		if(result)
		{
			fprintf(stderr, "Stack insertion: evm_LoadObjectFile: %i\n", result);
			return 1;
		}

		evm_FindExport(&state, "$__evm_stack", &sym);
		evm_MarkAPIPoint(sym);

		firstparm = 3;
	}


	for(i=firstparm;i<argc;i++)
	{
		switch(argv[i][0])
		{
		case '\0':
			fprintf(stderr, "Empty argument?");
			return 1;
		case '+':
			if(evm_FindExport(&state, argv[i]+1, &sym) == EVM_ERR_SYMBOL_NOT_FOUND)
			{
				fprintf(stderr, "Could not find symbol: %s", argv[i]+1);
				return 1;
			}
			evm_MarkAPIPoint(sym);
			break;
		case '-':
			outname = argv[i]+1;
			break;
		default:
			if(!(f = fopen(argv[i], "rb")))
			{
				fprintf(stderr, "Couldn't open %s\n", argv[i]);
				return 1;
			}

			if(!fread(&header, sizeof(evm_header_t), 1, f))
			{
				fprintf(stderr, "Read error in %s\n", argv[i]);
				return 1;
			}
			evm_CalculateObjectFileSize(&header, &size);
			if(!(filebuffer = malloc(size)))
			{
				fprintf(stderr, "Out of memory: %lu", size);
				return 1;
			}
			memcpy(filebuffer, &header, sizeof(evm_header_t));
			if(!fread(filebuffer+sizeof(evm_header_t), size - sizeof(evm_header_t), 1, f))
			{
				fprintf(stderr, "Read error in %s\n", argv[i]);
				return 1;
			}
			result = evm_LoadObjectFile(&state, filebuffer, size);
			if(result)
			{
				fprintf(stderr, "%s: evm_LoadObjectFile: %d\n", argv[i], result);
				return 1;
			}
			free(filebuffer);
			fclose(f);
			break;
		}
	}

	if(evm_Link(&state, &conflict) == EVM_ERR_SYMBOL_CONFLICT)
	{
		fprintf(stderr, "Conflicting symbol: %s\n", conflict);
		exit(1);
	}

	if(strcmp(argv[1], "obj"))
		evm_Localize(&state);
	if(!strcmp(argv[1], "app") || !strcmp(argv[1], "add"))
		evm_Resolve(&state, &emptyProgram, EVM_RESOLVE_NONIMPORTS);


	if(!outname)
		f = stdout;
	else
	{
		if(!(f = fopen(outname, "wb")))
		{
			fprintf(stderr, "Could not open output file");
			return 1;
		}
	}
	evm_Dump(&state, writebytes, f);
	if(f != stdout)
		fclose(f);

	return 0;
}
