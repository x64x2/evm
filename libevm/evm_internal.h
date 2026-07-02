#ifndef __EVM_INTERNAL_H__
#define __EVM_INTERNAL_H__

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "../common/evm.h"
#include "../common/evm_endian.h"
#include "../common/evm_opcodes.h"

typedef struct
{
	evm_u32 offset;
	evm_u32 size;
	const void *memory;
} evm_membuffer_t;

typedef struct evm_symbolhash_s
{
	evm_symbol_t sym;

	unsigned char hash;
	unsigned char keep;

	evm_u32 nextSymbol;

	evm_u32 relocRemap;		// Symbol index to remap relocations to
	evm_u32 renumber;		// What this symbol's index will be after cleanup
} evm_symbolhash_t;


void *evm_frealloc(evm_memorymanager_t *mm, void *ptr, size_t size);
unsigned char evm_hashstring(const char *str);
evm_result_t evm_SymbolCleanup(evm_ocpstate_t *state);
void evm_RebuildHashChains(evm_ocpstate_t *state);

evm_result_t evm_ResolveSymbol(evm_ocpstate_t *state, evm_program_t *program, evm_symbolhash_t *sym);

evm_result_t evm_ApplyRelocations(evm_ocpstate_t *state);
evm_result_t evm_ObjectIntegrityCheck(evm_ocpstate_t *state);

#define evm_malloc(size) (state->mm.malloc ? state->mm.malloc((size), state->mm.opaque) : malloc(size))
#define evm_free(ptr) if(state->mm.free) state->mm.free((ptr), state->mm.opaque); else free(ptr)
#define evm_realloc(ptr, size) (evm_frealloc(&(state->mm), (ptr), (size)))


#define evm_saferealloc(ptr, size) if(!((ptr) = evm_realloc((ptr), (size))) && (size)) return EVM_ERR_OUT_OF_MEMORY
#define evm_safemalloc(ptr, size) if(!((ptr) = evm_malloc(size))) return EVM_ERR_OUT_OF_MEMORY

#define SAFECALL(f) { evm_result_t safecallerror; if(safecallerror = f) return safecallerror; }

#define SAFECALLCUSTOM(err, f) if(f) return err;

#define BYTEOFFSET(p, offs) ((void *)(((char *)(p))+(offs)))



evm_result_t evm_vmProfileInstructions(evm_program_t *program);

evm_result_t evm_vmGenericCall(evm_thread_t *t, evm_u32 address, evm_val32_t *parameters, evm_u32 numParameters);
evm_result_t evm_vmGenericEndCall(evm_thread_t *t, evm_u32 numParameters);
evm_result_t evm_vmGenericInitializeThread(evm_thread_t *t, evm_ocpstate_t *state, evm_program_t *program);

evm_result_t evm_vmJITCompileNative(evm_program_t *program);
evm_result_t evm_vmJITRun(evm_thread_t *t, evm_val32_t *rval);


evm_result_t evm_vmInterpretCompileNative(evm_program_t *program);
evm_result_t evm_vmInterpretRun(evm_thread_t *t, evm_val32_t *rval);



evm_result_t evm_vmProfileCode(evm_program_t *program);

#endif
