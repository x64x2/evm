#include "evm_types.h"
#ifdef EVM_BIG_ENDIAN

#define evm_bigLong(x) (x)
#define evm_bigShort(x) (x)

#define ByteSwapShort(x)
#define ByteSwapLong(x)

#else

evm_i32 evm_BigLong(evm_i32 l);
evm_i16 evm_BigShort(evm_i16 s);

#define ByteSwapLong(x) ((x) = evm_BigLong(x))
#define ByteSwapShort(x) ((x) = evm_BigShort(x))

#endif
