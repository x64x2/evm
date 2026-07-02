typedef struct
{
	unsigned char opcode;
	evm_i32 stackmod;
} evm_stackoffset_t;

static evm_stackoffset_t evm_stackoffsets[] =
{
	{EVM_OP_STORE1, -2},
	{EVM_OP_STORE2, -2},
	{EVM_OP_STORE4, -2},

	{EVM_OP_LOAD1, 0},
	{EVM_OP_LOAD2, 0},
	{EVM_OP_LOAD4, 0},

	{EVM_OP_LTOF, 0},
	{EVM_OP_UTOF, 0},
	{EVM_OP_FTOL, 0},

	{EVM_OP_SEX8, 0},
	{EVM_OP_SEX16, 0},

	{EVM_OP_ZHI24, 0},
	{EVM_OP_ZHI16, 0},

	{EVM_OP_NEGL, 0},
	{EVM_OP_NEGF, 0},

	{EVM_OP_CALL, 0},

	{EVM_OP_BSL, -1},
	{EVM_OP_BSRI, -1},
	{EVM_OP_BSRU, -1},

	{EVM_OP_BNOT, 0},

	{EVM_OP_BAND, -1},
	{EVM_OP_BOR, -1},
	{EVM_OP_BXOR, -1},

	{EVM_OP_ADDF, -1},
	{EVM_OP_SUBF, -1},
	{EVM_OP_MULF, -1},
	{EVM_OP_DIVF, -1},

	{EVM_OP_MULU, -1},
	{EVM_OP_DIVU, -1},
	{EVM_OP_MODU, -1},

	{EVM_OP_ADDL, -1},
	{EVM_OP_SUBL, -1},
	{EVM_OP_MULL, -1},
	{EVM_OP_DIVL, -1},
	{EVM_OP_MODL, -1},

	{EVM_OP_JUMP, -1},

	{EVM_OP_DISCARD, -1},

	{EVM_OP_ADDR_SPR, 1},
	{EVM_OP_ADDR_ARG, 1},

	{EVM_OP_BEGINSUB, 0},
	{EVM_OP_ENDSUB, -1},

	{EVM_OP_COPY, -2},

	{EVM_OP_BEQ, -2},
	{EVM_OP_BEQF, -2},
	{EVM_OP_BNE, -2},
	{EVM_OP_BNEF, -2},

	{EVM_OP_BGEF, -2},
	{EVM_OP_BGEU, -2},
	{EVM_OP_BGEL, -2},

	{EVM_OP_BGTF, -2},
	{EVM_OP_BGTU, -2},
	{EVM_OP_BGTL, -2},

	{EVM_OP_BLEF, -2},
	{EVM_OP_BLEU, -2},
	{EVM_OP_BLEL, -2},

	{EVM_OP_BLTF, -2},
	{EVM_OP_BLTU, -2},
	{EVM_OP_BLTL, -2},

	{EVM_OP_BLTL, 1},

	{EVM_OP_SKIPNEXT, -1},

	{EVM_OP_IMMEDIATE, 1},

	{EVM_OP_STORE_SPR4, -1},

	{EVM_OP_DROP8, 0},
	{EVM_OP_DROP16, 0},
};

#define NUM_STACK_OFFSETS (sizeof(evm_stackoffsets) / sizeof(evm_stackoffsets[0]))
