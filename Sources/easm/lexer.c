#include <stdio.h>
#include <stdlib.h>

#include "../common/evm_types.h"

#define MAX_TOKEN	62
#define BUFFERSIZE	16384

static char tokentext[MAX_TOKEN];
const char *lextoken = tokentext;

int lex_line = 1;

static char lexbuffer[BUFFERSIZE];
static evm_u32 bufferoffset = 0;
static evm_u32 bufferlength;
static int lexeof = 0;
static int lexinit = 0;


static void fillbuffer(FILE *f)
{
	size_t bytesread;

	bytesread = bufferlength = fread(lexbuffer, 1, BUFFERSIZE, f);

	if(!bytesread)
		lexeof = 1;

	lexinit = 1;
	bufferoffset = 0;
}


static char peekbuffer(FILE *f)
{
	return lexbuffer[bufferoffset];
}


static char readbuffer(FILE *f)
{
	char r;

	r = lexbuffer[bufferoffset++];

	if(bufferoffset == bufferlength)
		fillbuffer(f);
	return r;
}




void gettoken(FILE *f)
{
	int i;
	char c;

	if(!lexinit)
		fillbuffer(f);
	lextoken = tokentext;

	if(lexeof)
	{
		lextoken = NULL;
		return;
	}

	c = peekbuffer(f);
	if(c <= ' ')
	{
		while(c <= ' ')
		{
			if(c == '\n')
				lex_line++;

			readbuffer(f);

			if(lexeof)
			{
				lextoken = NULL;
				return;
			}
			c = peekbuffer(f);
		}
	}

	if(c == ';')
	{
		while(c != '\n')
		{
			readbuffer(f);
			if(lexeof)
			{
				lextoken = NULL;
				return;
			}
			c = peekbuffer(f);
		}
	}

	if(c == '+' || c == '-')
	{
		tokentext[0] = c;
		tokentext[1] = '\0';
		readbuffer(f);
		return;
	}

	if(c == '\"')
	{
		i = 0;
		for(;;)
		{
			if(lexeof)
			{
				lextoken = NULL;
				return;
			}

			c = readbuffer(f);

			if(c == '\n')
			{
				fprintf(stderr, "[%i]: Endline in filename", lex_line);
				exit(1);
			}

			if(i == MAX_TOKEN)
				i--;	

			if(c == '\"')
			{
				tokentext[i] = '\0';
				readbuffer(f);
				return;
			}
			tokentext[i++] = c;
		}
	}

	i = 0;
	for(;;)
	{
		if(lexeof)
		{
			tokentext[i] = '\0';
			return;
		}
		c = peekbuffer(f);

		if(i == MAX_TOKEN - 1)
		{
			fprintf(stderr, "[%i]: Token too long\n", lex_line);
			exit(1);
		}
		tokentext[i++] = c;

		if(c <= ' ' || c == '+' || c == '-')
		{
			tokentext[i-1] = '\0';
			return;
		}
		readbuffer(f);
	}
}
