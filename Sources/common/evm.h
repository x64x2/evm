#ifndef __D3_COMMON_H__
#define __D3_COMMON_H__

#include "evm_types.h"
#include "evm_api.h"

#include <stdlib.h>

// Stop certain errors
#pragma warning(disable:4706)	// assignment within conditional expression
#pragma warning(disable:4115)	// named type definition in parentheses

// Exposed file structures
typedef struct
{
	char identifier[4];		// D3OB

	evm_u32 symbolTableSize;
	evm_u32 symbolCount;

	evm_u32 relocationCount;

	evm_u32 dataTableSize;
	evm_u32 dataTableSizeDecoded;

	evm_u32 litTableSize;

	evm_u32 codeTableSize;
	evm_u32 codeTableNumSymbols;

	evm_u32 bssSize;
} evm_header_t;


typedef struct
{
	unsigned char table;
	unsigned char unused[3];

	evm_u32 tableOffset;
	evm_u32 symbolIndex;

	evm_i32 valueIncrement;
} evm_relocation_t;


typedef struct
{
	char name[255];
	unsigned char linkFlags;

	evm_u32 value;
} evm_symbol_t;


// Symbol flags
#define EVM_LINKFLAG_MASK			(3<<2)
#define EVM_EXPORTED_MASK			(2<<2)
#define EVM_SEG_MASK				3

#define EVM_LINKFLAG_LOCAL			(0<<2)
#define EVM_LINKFLAG_IMPORT			(1<<2)
#define EVM_LINKFLAG_EXPORT			(2<<2)
#define EVM_LINKFLAG_ENTRYPOINT		(3<<2)

#define EVM_LINKFLAG_RESOLVED		(1<<4)

// Actual layout: BSS, DATA, LIT
#define EVM_SEG_DATA				0
#define EVM_SEG_BSS					1
#define EVM_SEG_LIT					2
#define EVM_SEG_CODE				3



// VM interfaces
typedef evm_u32 evm_pointer_t;

typedef union
{
	evm_u32 ul;
	evm_i32 l;
#ifndef EVM_NO_FLOATING_POINT
	float f;
#endif
	evm_pointer_t ptr;
	evm_u32 function;
	unsigned char bytes[4];
} evm_val32_t;


struct evm_symbol
{
	evm_symbol(const evm_symbol &) = default;
	evm_symbol(evm_symbol &&) = default;
	evm_symbol &operator=(const evm_symbol &) = default;
	evm_symbol &operator=(evm_symbol &&) = default;
	unsigned char opcode;
	evm_val32_t argument;
	
	evm_u32 maxStackLevel;
	evm_u32 jitTranslation;
	
	evm_u32 blockID; 
};


// Return values
typedef enum
{
	EVM_OK=0,

	EVM_THREAD_EXITED = -10,
	EVM_THREAD_SUSPENDED = -11,

	EVM_ERR_NOT_AN_OBJECT_FILE = -20,
	EVM_ERR_INVALID_OBJECT_FILE = -21,
	EVM_ERR_NO_SUCH_EXECUTION_DRIVER = -22,

	EVM_ERR_CORRUPT_CODE_SEGMENT = -30,
	EVM_ERR_CORRUPT_DATA_SEGMENT = -31,
	EVM_ERR_CORRUPT_SYMBOL_TABLE = -32,
	EVM_ERR_CORRUPT_RELOCATION_TABLE = -33,
	EVM_ERR_UNEXPECTED_EOF = -34,
	EVM_ERR_BAD_RELOCATION = -35,

	EVM_ERR_SYMBOL_CONFLICT = -40,
	EVM_ERR_SYMBOL_NOT_FOUND = -41,

	EVM_ERR_BAD_RESOLVE_FLAGS = -50,

	EVM_ERR_COULD_NOT_RESOLVE_IMPORT = -60,

	EVM_ERR_RELOCATIONS_NOT_RESOLVED = -70,

	EVM_ERR_OUT_OF_MEMORY = -80,

	EVM_ERR_PROFILE_FAILED = -90,
	EVM_ERR_COMPILE_FAILED = -91,


	EVM_RTERR_ACCESS_VIOLATION = -100,
	EVM_RTERR_NULL_ACCESS = -101,

	EVM_RTERR_CODE_ACCESS_VIOLATION = -102,
	EVM_RTERR_UNIMPLEMENTED_OPCODE = -103,

	EVM_RTERR_OPSTACK_UNDERFLOW = -104,
	EVM_RTERR_OPSTACK_OVERFLOW = -105,

	EVM_RTERR_ALIGNMENT_ERROR = -106,

	EVM_RTERR_DIVISION_BY_ZERO = -107,

	EVM_RTERR_STACK_OVERFLOW = -108,
	EVM_RTERR_STACK_UNDERFLOW = -109,

	EVM_RTERR_SUBROUTINE_ZONE_VIOLATION = -110,
	EVM_RTERR_CALL_NOT_AT_SUB_START = -111,

	EVM_RTERR_JUMP_LIMIT_EXCEEDED = -120,

	EVM_ERR_NO_FLOATING_POINT = -130,
} evm_result_t;

#define EVM_RELOCATIONS_ALLOWED 0x00000001


typedef struct evm_memorymanager_s
{
	void *opaque;
	void *(*malloc) (size_t size, void *opaque);
	void *(*realloc) (void *ptr, size_t size, void *opaque);
	void (*free) (void *ptr, void *opaque);
} evm_memorymanager_t;


typedef struct evm_thread_s evm_thread_t;

// VM state
typedef struct evm_ocpstate_s
{
	// Memory management
	evm_memorymanager_t mm;

	// Host callbacks
	evm_result_t (*resolve_symbol) (struct evm_ocpstate_s *state, const char *name, evm_u32 *resolution);

	// Linker state
	void *dataseg;
	evm_u32 dataSegSize;
	evm_u32 dataSegSizeDecoded;

	evm_u32 bssSegSize;

	void *litseg;
	evm_u32 litSegSize;

	void *codeseg;
	evm_u32 codeSegSize;
	evm_u32 codeSegNumSymbols;

	// Symbol stuff
	evm_u32 symbolHashChains[256];	// Indexes into symbols+1

	struct evm_symbolhash_s *symbols;
	evm_u32 numSymbols;

	evm_relocation_t *relocations;
	evm_u32 numRelocations;
} evm_ocpstate_t;


// Program state
typedef struct
{
	evm_memorymanager_t mm;

	// Host callbacks
	evm_result_t (*apifunction) (evm_thread_t *t, evm_u32 pc, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs);

	void *(*access_host_memory) (evm_pointer_t ptr, evm_u32 size);
	evm_u32 (*host_memory_capacity) (evm_pointer_t ptr);

	// Vars
	evm_u32 vmMemoryConsumed;

	evm_symbol_t *vmSymbols;
	evm_u32 vmSymbolCount;

	evm_u32 jumpLimit;

	void *vmMemory;

	void *vmCompiledCode;
	evm_u32 vmCompiledCodeSize;
} evm_program_t;


// Thread
struct evm_thread_s
{
	evm_program_t *programState;

	evm_pointer_t sp;
	evm_u32 pc;

	void *(*evm_vmGetPointer) (evm_thread_t *t, evm_pointer_t ptr);
	void *(*evm_vmGetSpanPointer) (evm_thread_t *t, evm_pointer_t ptr, evm_u32 size);
	evm_u32 (*evm_vmGetPointerCapacity) (evm_thread_t *t, evm_pointer_t ptr);
};

typedef evm_result_t (*evm_apifunc_t) (struct evm_thread_s *t, evm_val32_t *returnValue, evm_val32_t *args, evm_u32 numArgs);

typedef struct
{
	evm_apifunc_t func;
	const char *name;
} evm_apientry_t;

typedef struct
{
	evm_apientry_t *apientries;
	evm_u32 numEntries;
	evm_u32 pcBase;
} evm_apiblock_t;

#define EVM_CREATE_APIBLOCK(l, n) evm_apiblock_t n = { l, sizeof(l) / sizeof(l[0]) }


#define EVM_RESOLVE_IMPORTS			1
#define EVM_RESOLVE_NONIMPORTS		2
#define EVM_RESOLVE_MASK			3

typedef void (*evm_dumpfunc_t) (const void *buf, evm_u32 size, void *handle);


// Execution driver
#define EVM_DRIVERNAME_INTERPRETER  "interpreter"
#define EVM_DRIVERNAME_NATIVE       "native"

typedef struct
{
	evm_result_t (*initializeThread) (evm_thread_t *thread, evm_ocpstate_t *state, evm_program_t *program);
	evm_result_t (*compile) (evm_program_t *program);
	evm_result_t (*startCall) (evm_thread_t *t, evm_u32 compileress, evm_val32_t *parameters, evm_u32 numParameters);
	evm_result_t (*run) (evm_thread_t *t, evm_val32_t *returnValue);
	evm_result_t (*endCall) (evm_thread_t *t, evm_u32 numParameters);
} evm_executiondriver_t;


EVM_EXPORT evm_result_t evm_LoadObjectFile(evm_ocpstate_t *state, const void *buffer, evm_u32 size);
EVM_EXPORT evm_result_t evm_MarkAPIPoint(evm_symbol_t *sym);
EVM_EXPORT evm_result_t evm_FindExport(evm_ocpstate_t *state, const char *name, evm_symbol_t **sym);
EVM_EXPORT evm_result_t evm_Dump(evm_ocpstate_t *state, evm_dumpfunc_t dumpfunc, void *f);
EVM_EXPORT evm_result_t evm_CalculateObjectFileSize(evm_header_t *header, evm_u32 *size);
EVM_EXPORT evm_result_t evm_Localize(evm_ocpstate_t *state);
EVM_EXPORT evm_result_t evm_Link(evm_ocpstate_t *state, const char **conflict);
EVM_EXPORT evm_result_t evm_FreeOCPState(evm_ocpstate_t *state);

// VM functions
EVM_EXPORT evm_result_t evm_CompileObjects(evm_ocpstate_t *state, evm_program_t *program, evm_u32 flags);
EVM_EXPORT evm_result_t evm_Resolve(evm_ocpstate_t *state, evm_program_t *program, evm_u32 resolveFlags);
EVM_EXPORT evm_result_t evm_FreeProgram(evm_program_t *state);


// Utility functions
EVM_EXPORT evm_result_t evm_uResolveUsingAPIBlock(evm_apiblock_t *block, const char *name, evm_u32 *resolution);
EVM_EXPORT evm_apifunc_t evm_uGetAPIFunction(evm_apiblock_t *block, evm_u32 pc);


// Execution drivers
EVM_EXPORT evm_result_t evm_GetExecutionDriver(const char *driverName, evm_executiondriver_t *driver);

#endif
