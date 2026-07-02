#ifndef __EVM_OPCODES_H__
#define __EVM_OPCODES_H__

// Opcode table
enum
{
	// 0
	EVM_OP_STORE1,		// copy 1 byte top to *second, pop x2
	EVM_OP_STORE2,		// copy 2 byte top to *second, pop x2
	EVM_OP_STORE4,		// copy 4 byte top to *second, pop x2

	// 3
	EVM_OP_LOAD1,		// push 1 byte from *top, pop
	EVM_OP_LOAD2,		// push 2 byte from *top, pop
	EVM_OP_LOAD4,		// push 4 byte from *top, pop

	// 6
	EVM_OP_LTOF,		// convert top from integer to float
	EVM_OP_UTOF,		// convert top from unsigned integer to float
	EVM_OP_FTOL,		// convert top from float to integer

	// 9
	EVM_OP_SEX8,		// convert low byte of top to 32-bit top
	EVM_OP_SEX16,		// convert low 2 bytes of top to 32-bit top

	// 11
	EVM_OP_ZHI24,		// zero high 24 bits of top
	EVM_OP_ZHI16,		// zero high 16 bits of top

	// 13
	EVM_OP_NEGL,		// negate top as long
	EVM_OP_NEGF,		// negate top as float

	// 15
	EVM_OP_CALL,		// Copy PC to *SP, load PC from top, pop

	// 16
	EVM_OP_BSL,			// result = second << top
	EVM_OP_BSRI,		// result = second >> top, signed
	EVM_OP_BSRU,		// result = second >> top, unsigned

	// 19
	EVM_OP_BNOT,		// inverts bits of top

	// 20
	EVM_OP_BAND,		// result = second & top
	EVM_OP_BOR,			// result = second | top
	EVM_OP_BXOR,		// result = second ^ top

	// 23
	EVM_OP_ADDF,		// result = second + top (float)
	EVM_OP_SUBF,		// result = second - top (float)
	EVM_OP_MULF,		// result = second * top (float)
	EVM_OP_DIVF,		// result = second / top (float)

	// 27
	EVM_OP_MULU,		// result = second * top (unsigned)
	EVM_OP_DIVU,		// result = second / top (unsigned)
	EVM_OP_MODU,		// result = second % top (unsigned)

	// 30
	EVM_OP_ADDL,		// result = second + top
	EVM_OP_SUBL,		// result = second - top
	EVM_OP_MULL,		// result = second * top
	EVM_OP_DIVL,		// result = second / top
	EVM_OP_MODL,		// result = second % top

	// 35
	EVM_OP_JUMP,		// PC = top, pop

	// 36
	EVM_OP_DISCARD,		// pop opstack

	// 37
	EVM_OP_SKIPNEXT,	// PC = PC+1

	//============================================================
	EVM_OP_ARGOPS_START,
	EVM_OP_ARGOPS_AUTOSKIP = EVM_OP_ARGOPS_START-1,
	//============================================================

	// 39
	EVM_OP_ADDR_SPR,	// push SP+arg
	EVM_OP_ADDR_ARG,	// push SP+arg+spill

	// 40
	EVM_OP_BEGINSUB,	// SP -= arg
	EVM_OP_ENDSUB,		// SP += arg, load PC from *SP

	// 42
	EVM_OP_COPY,		// copy arg byte from *top to *second

	// 43

	//============================================================
	EVM_OP_BRANCHOPS_START,
	EVM_OP_BRANCHOPS_AUTOSKIP = EVM_OP_BRANCHOPS_START - 1,
	//============================================================
	EVM_OP_BEQ,			// if second == top, branch to arg pop x2
	EVM_OP_BEQF,		// if second == top, branch to arg (float) pop x2
	EVM_OP_BNE,			// if second != top, branch to arg pop x2
	EVM_OP_BNEF,		// if second != top, branch to arg (float) pop x2

	// 47
	EVM_OP_BGEF,		// if second >= top, branch to arg (float) pop x2
	EVM_OP_BGEU,		// if second >= top, branch to arg (unsigned) pop x2
	EVM_OP_BGEL,		// if second >= top, branch to arg pop x2

	// 50
	EVM_OP_BGTF,		// if second > top, branch to arg (float) pop x2
	EVM_OP_BGTU,		// if second > top, branch to arg (unsigned) pop x2
	EVM_OP_BGTL,		// if second > top, branch to arg pop x2

	// 53
	EVM_OP_BLEF,		// if second <= top, branch to arg (float) pop x2
	EVM_OP_BLEU,		// if second <= top, branch to arg (unsigned) pop x2
	EVM_OP_BLEL,		// if second <= top, branch to arg pop x2

	// 56
	EVM_OP_BLTF,		// if second < top, branch to arg (float) pop x2
	EVM_OP_BLTU,		// if second < top, branch to arg (unsigned) pop x2
	EVM_OP_BLTL,		// if second < top, branch to arg pop x2
	//============================================================
	EVM_OP_BRANCHOPS_END,
	EVM_OP_BRANCHOPS_END_AUTOSKIP = EVM_OP_BRANCHOPS_END - 1,
	//============================================================

	// 59
	EVM_OP_IMMEDIATE,	// push arg

	// 60
	EVM_OP_STORE_SPR4,	// copy top to SP+arg

	// 61
	EVM_OP_DROP8,		// if big endian, copy 1 byte from SP+arg+3 to SP+arg
	EVM_OP_DROP16,		// if big endian, copy 2 bytes from SP+arg+2 to SP+arg


	EVM_OP_NUM_OPCODES,
};

#endif
