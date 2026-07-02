#ifndef __BUFFERING_H__
#define __BUFFERING_H__

#include "../common/evm_types.h"

typedef struct
{
	void *data;
	evm_u32 offset;
	evm_u32 capacity;
} dynamicbuffer_t;

void writebuffer(dynamicbuffer_t *dbuf, const void *buf, evm_u32 size);

#endif

