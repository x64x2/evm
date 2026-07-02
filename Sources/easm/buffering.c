#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "buffering.h"

#define INITIAL_SIZE	0x100000

#define GROW_SIZE 0x10000


void writebuffer(dynamicbuffer_t *dbuf, const void *buf, evm_u32 size)
{
	unsigned char *tempbuf;

	if(!dbuf->data)
	{
		if(!(dbuf->data = malloc(INITIAL_SIZE)))
		{
			dbuf->offset = 0;
			dbuf->capacity = INITIAL_SIZE;
			fprintf(stderr, "Could not create dynamic buffer");
			exit(1);
		}
	}


	if(dbuf->offset + size > dbuf->capacity)
	{
		while(dbuf->offset + size > dbuf->capacity)
			dbuf->capacity += GROW_SIZE;

		tempbuf = malloc(dbuf->capacity);
		memcpy(tempbuf, dbuf->data, dbuf->offset);

		free(dbuf->data);
		dbuf->data = tempbuf;
	}

	tempbuf = dbuf->data;

	memcpy(tempbuf + dbuf->offset, buf, size);
	dbuf->offset += size;
}

