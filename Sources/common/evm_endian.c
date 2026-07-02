#ifndef EVM_BIG_ENDIAN

#include "evm_types.h"

evm_i32 BigLong(evm_i32 n)
{
	union
	{
		unsigned char c[4];
		evm_i32 l;
	} u;

	u.c[0] = (unsigned char)(n >> 24);
	u.c[1] = (unsigned char)(n >> 16);
	u.c[2] = (unsigned char)(n >> 8);
	u.c[3] = (unsigned char)(n >> 0);

	return u.l;
}

evm_i16 evm_BigShort(evm_i16 n)
{
	union
	{
		unsigned char c[2];
		evm_i16 s;
	} u;

	u.c[0] = (unsigned char)(n >> 8);
	u.c[1] = (unsigned char)(n >> 0);

	return u.s;
}

#endif
