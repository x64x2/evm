#include "evm_internal.h"

#include "evm_codegen_x86.h"

// Custom emitter macros
#define x86_test_ah_imm8(inst,imm) \
	do {\
		*(inst)++ = 0xf6;\
		*(inst)++ = 0xc4;\
		*(inst)++ = (imm);\
	} while(0)


#define x86_jump32_auto(inst, offset) \
	x86_jump32 ((inst), (offset)-5)




#define JIT_STACK_SPACE_NEEDED  0x54

#define   eax    X86_EAX
#define   ebx    X86_EBX
#define   ecx    X86_ECX
#define   edx    X86_EDX
#define   ebp    X86_EBP
#define   edi    X86_EDI
#define   esp    X86_ESP
#define   esi    X86_ESI


#define l_state     (-4)
#define l_vmMemory  (-8)
#define l_sp        (-12)
#define l_axshostmm (-16)
#define l_pointer   (-20)
#define l_aptr      (-24)
#define l_svoffbkup (-32)
#define l_regbkup   (-36)
#define l_codebase  (-40)
#define l_instbase  (-44)
#define l_blockid   (-48)
#define l_lfloat    (-52)
#define l_rfloat    (-56)
#define l_float     l_lfloat

#define l_fp64space (-64)
#define l_apifunc   (-68)

#define l_jumplimit (-72)

#define p_thread    8
#define p_return    12


#define byteoffset(t, m) (((char *) (&(((t *)0)->m))) - ((char *)0))


// CONVENTIONS:
//    ebx contains pointer to virtual stack frame
//    To jump using a VPC, store VPC in ecx then jump to label_jumpToVPC

// Stupid access-violation thing designed specifically to cause a crash
// so I can debug my assembly.
#define compile_access_violation(i) \
	x86_mov_reg_membase(i, eax, ebx, 0xcccccccc, 4)


#define compile_stopexec(i, returncode) \
	do\
	{\
		x86_mov_reg_membase(i, ebx, ebp, p_thread, 4);\
		x86_mov_membase_imm(i, ebx, byteoffset(evm_thread_t, pc), vPC, 4);\
		x86_mov_reg_membase(i, eax, ebp, l_sp, 4);\
		x86_mov_membase_reg(i, ebx, byteoffset(evm_thread_t, sp), eax, 4);\
		x86_mov_reg_imm(i, eax, returncode);\
		x86_jump_code(i, label_return);\
	} while(0)


// compile_get_stack0_pointer
//    sets eax to accessable pointer address

#define compile_get_stack0_pointer(i, stackoffset, alignment) \
	do\
	{\
		unsigned char *label_jneToNonNull;\
		unsigned char *label_jneToFinishAccess;\
		unsigned char *label_jltToLocalMemory;\
		unsigned char *label_jzToAligned;\
		unsigned char *label_jeqToAccessViolation;\
		/* Load EDX with the virtual address */\
		x86_mov_reg_membase(i, edx, ebx, stackoffset, 4);								\
\
		/* Verify the pointer as non-NULL */\
		x86_alu_reg_imm(i, X86_CMP, edx, 0);											\
		label_jneToNonNull = i;															\
		x86_branch_disp(i, X86_CC_NE, 1, 0);											\
\
		compile_stopexec(i, EVM_RTERR_NULL_ACCESS);										\
		x86_branch(label_jneToNonNull, X86_CC_NE, i, 0);								\
\
		if(alignment != 1)																\
		{																				\
			x86_mov_reg_reg(i, ecx, edx, 4);											\
			x86_alu_reg_imm(i, X86_AND, ecx, (alignment==2) ? 1 : 3);					\
			label_jzToAligned = i;														\
			x86_branch_disp(i, X86_CC_Z, 1, 0);											\
			compile_stopexec(i, EVM_RTERR_ALIGNMENT_ERROR);								\
			x86_branch(label_jzToAligned, X86_CC_Z, i, 0);								\
		}																				\
\
		/* Pointer is non-null, verify range */\
		x86_alu_reg_imm(i, X86_CMP, edx, state->vmMemoryConsumed);						\
		label_jltToLocalMemory = i;														\
		x86_branch_disp(i, X86_CC_LT, 1, 0);											\
\
		/* Out of range, get it from the host */\
		/* See if the host memory can be accessed */\
		x86_mov_reg_membase(i, eax, ebp, l_axshostmm, 4);								\
		x86_alu_reg_imm(i, X86_CMP, eax, 0);											\
		label_jeqToAccessViolation = i;													\
		x86_branch_disp(i, X86_CC_EQ, 1, 0);											\
		/* Host supplied a memory access function */\
		x86_push_imm(i, 0);																\
		x86_push_reg(i, edx);															\
		x86_call_reg(i, eax);															\
		x86_alu_reg_imm(i, X86_ADD, esp, 8);											\
\
		/* Did it return NULL? */\
		x86_alu_reg_imm(i, X86_CMP, eax, 0);											\
		label_jneToFinishAccess = i;													\
		x86_branch_disp(i, X86_CC_NE, 1, 0);											\
\
		/* Yes, it did */\
		x86_branch(label_jeqToAccessViolation, X86_CC_EQ, i, 0);						\
		compile_stopexec(i, EVM_RTERR_ACCESS_VIOLATION);								\
		/* Otherwise, this will skip straight to the end*/\
\
		/* Local memory, not host-based */\
		x86_branch(label_jltToLocalMemory, X86_CC_LT, i, 0);							\
		x86_mov_reg_reg(i, eax, edx, 4);												\
		x86_alu_reg_membase(i, X86_ADD, eax, ebp, l_vmMemory);							\
\
		x86_branch(label_jneToFinishAccess, X86_CC_NE, i, 0);							\
	} while(0)




#define compile_store(i, stackoffset, size) \
	do {\
		/* EAX: Data address */\
		compile_get_stack0_pointer(i, stackoffset, size);\
\
		/* ECX: Data to store */\
		x86_mov_reg_membase(i, ecx, ebx, stackoffset+4, size);\
\
		/* Copy */\
		x86_mov_membase_reg(i, eax, 0, ecx, size);\
	} while(0)


#define compile_load(i, stackoffset, size) \
	do {\
		/* EAX: Data address */\
		compile_get_stack0_pointer(i, stackoffset-4, size);\
\
		/* ECX: Loaded data */\
		x86_mov_reg_membase(i, ecx, eax, 0, size);\
\
		/* Copy to the stack*/\
		x86_mov_membase_reg(i, ebx, stackoffset-4, ecx, size);\
	} while(0)


#define compile_immediate(i, stackoffset, imm) \
	do\
	{\
		x86_mov_membase_imm(i, ebx, stackoffset-4, imm, 4);								\
	} while(0)


#define compile_addr_spr(i, stackoffset, offset) \
	do\
	{\
		x86_mov_reg_membase(i, eax, ebp, l_sp, 4);										\
		x86_alu_reg_imm(i, X86_ADD, eax, offset);										\
		x86_mov_membase_reg(i, ebx, stackoffset-4, eax, 4);								\
	} while(0)


#define compile_beginsub(i, framesize) \
	do\
	{\
		unsigned char *label_jgtToStackspaceOK;											\
		/* Make sure there's room.  If framesize >= SP, then overflow */\
		x86_mov_reg_membase(i, eax, ebp, l_sp, 4);										\
		/* Do SUB because it needs to be subtracted anyway, this spares a SUB later */\
		x86_alu_reg_imm(i, X86_SUB, eax, framesize);									\
		/* If SP <= framesize, then stack overflow */\
		label_jgtToStackspaceOK = i;													\
		x86_branch_disp(i, X86_CC_GT, 1, 1);											\
\
		/* No stack space*/\
		compile_stopexec(i, EVM_RTERR_STACK_OVERFLOW);									\
\
		x86_branch(label_jgtToStackspaceOK, X86_CC_GT, i, 1);							\
		/* Update SP */\
		x86_mov_membase_reg(i, ebp, l_sp, eax, 4);										\
		/* Update the actual base pointer */\
		x86_alu_reg_imm(i, X86_SUB, ebx, framesize);									\
	} while(0)


// The condition actually has to be reversed, since it requires 2 instructions to branch
#define compile_branch(i, stackoffset, condition, is_signed, branch_label) \
	do\
	{\
		unsigned char *label_branch;\
		x86_mov_reg_membase(i, ecx, ebx, stackoffset+4, 4);								\
		x86_mov_reg_membase(i, eax, ebx, stackoffset, 4);								\
		x86_alu_reg_reg(i, X86_CMP, eax, ecx);											\
		label_branch = i;\
		x86_branch_disp(i, (condition ^ 1), 1, is_signed);								\
		x86_mov_reg_imm(i, edx, branch_label);											\
		x86_mov_reg_imm(i, ecx, inst->blockID);											\
		x86_jump_code(i, label_jumpToVPCWithinBlock);									\
		x86_branch(label_branch, (condition ^ 1), i, is_signed);						\
	} while(0)


// Because of zone checking, the SP and increment are guaranteed to be valid, so this
// can be done safely without any checks
#define compile_endsub(i, framesize, stackoffset) \
	do\
	{\
		unsigned char *label_jneToEndsub;												\
		unsigned char *label_jzToSaveAligned;											\
		unsigned char *label_jltToSaveValid;											\
		/* Get the return value, store it in EDX */\
		x86_mov_reg_membase(i, edx, ebx, stackoffset, 4);								\
		/* Increment the stack base pointer */\
		x86_alu_reg_imm(i, X86_ADD, ebx, framesize);									\
		/* Get the VSP by subtracting the memory base from EBX */\
		x86_mov_reg_reg(i, eax, ebx, 4);												\
		x86_alu_reg_membase(i, X86_SUB, eax, ebp, l_vmMemory);							\
		/* Store the new VSP */\
		x86_mov_membase_reg(i, ebp, l_sp, eax, 4);										\
		/* Recover the old VPC */\
		x86_mov_reg_membase(i, ecx, ebx, 0, 4);											\
\
		/* Check the VPC.  If it's 0, assume the VM code has been exited */\
		x86_alu_reg_imm(i, X86_CMP, ecx, 0);											\
		label_jneToEndsub = i;															\
		x86_branch_disp(i, X86_CC_NE, 1, 0);											\
\
		/* Return */\
		/* Copy the return value */\
		x86_mov_reg_membase(i, ebx, ebp, p_return, 4);									\
		x86_mov_membase_reg(i, ebx, 0, edx, 4);											\
		compile_stopexec(i, EVM_THREAD_EXITED);											\
\
		x86_branch(label_jneToEndsub, X86_CC_NE, i, 0);									\
		/* End sub (still VM code) */\
		/* ECX contains VPC, EDX contains return value */\
		/* Verify the save offset alignment */\
		x86_mov_reg_membase(i, eax, ebx, 4, 4);											\
		x86_alu_reg_imm(i, X86_AND, eax, 3);											\
		label_jzToSaveAligned = i;														\
		x86_branch_disp(i, X86_CC_Z, 1, 0);											\
		/* Misaligned save offset (corrupt) */\
		compile_stopexec(i, EVM_RTERR_ALIGNMENT_ERROR);									\
		/* Correctly-aligned save offset, add SP and verify range */\
		x86_branch(label_jzToSaveAligned, X86_CC_Z, i, 0);							\
		x86_mov_reg_membase(i, eax, ebx, 4, 4);											\
		x86_alu_reg_membase(i, X86_ADD, eax, ebp, l_sp);								\
		x86_alu_reg_imm(i, X86_CMP, eax, state->vmMemoryConsumed);						\
		label_jltToSaveValid = i;														\
		x86_branch_disp(i, X86_CC_LT, 1, 0); 											\
		/* Out of range */\
		compile_stopexec(i, EVM_RTERR_ACCESS_VIOLATION);								\
		/* I'm not going to bother doing NULL checks, they're harmless in reality */\
		/* and the odds of them occurring are extremely low.  It requires that the */\
		/* safe offset get corrupted and wind up being -VSP. */\
		/* Valid pointer, store the return value */\
		x86_branch(label_jltToSaveValid, X86_CC_LT, i, 0);								\
		x86_mov_reg_membase(i, eax, ebx, 4, 4);											\
		x86_mov_memindex_reg(i, ebx, 0, eax, 0, edx, 4);								\
		/* Last but not least, jump to the old VPC */\
		x86_jump_code(i, label_jumpToVPC);\
	} while(0)


// compile_mathop is ONLY for use on order-independent stuff like ADD, XOR, etc
#define compile_mathop(i, operation, stackoffset) \
	do\
	{\
		x86_mov_reg_membase(i, eax, ebx, stackoffset, 4);								\
		x86_alu_membase_reg(i, operation, ebx, stackoffset-4, eax);						\
	} while(0)

#define compile_mathop_oneway(i, operation, stackoffset) \
	do\
	{\
		x86_mov_reg_membase(i, eax, ebx, stackoffset-4, 4);								\
		x86_alu_reg_membase(i, operation, eax, ebx, stackoffset);						\
		x86_mov_membase_reg(i, ebx, stackoffset-4, eax, 4);								\
	} while(0)


#define compile_multiply(i, stackoffset) \
	do\
	{\
		x86_mov_reg_membase(i, ecx, ebx, stackoffset, 4);								\
		x86_mov_reg_membase(i, eax, ebx, stackoffset-4, 4);								\
		x86_mul_reg(i, ecx, 0);															\
		x86_mov_membase_reg(i, ebx, stackoffset-4, eax, 4);								\
	} while(0)


#define compile_divide(i, stackoffset, getremainder, is_signed) \
	do\
	{\
		unsigned char *label_jneToNotDivByZero;\
		x86_mov_reg_membase(i, ecx, ebx, stackoffset, 4);								\
		x86_mov_reg_membase(i, eax, ebx, stackoffset-4, 4);								\
		/* Division by zero? */\
		x86_alu_reg_imm(i, X86_CMP, ecx, 0);											\
		label_jneToNotDivByZero = i;													\
		x86_branch_disp(i, X86_CC_NE, 1, 0);											\
		/* Yep, blow up */\
		compile_stopexec(i, EVM_RTERR_DIVISION_BY_ZERO);								\
		/* No, continue */\
		x86_branch(label_jneToNotDivByZero, X86_CC_NE, i, 0);							\
		if(is_signed)\
			x86_cdq(i);\
		else\
			x86_alu_reg_reg(i, X86_XOR, edx, edx);\
		x86_div_reg(i, ecx, 0);															\
		x86_mov_membase_reg(i, ebx, stackoffset-4, getremainder ? edx : eax, 4);		\
	} while(0)


#define FLOATCOMPARECODE_GE  0x01
#define FLOATCOMPARECODE_GT  0x41
#define FLOATCOMPARECODE_NE  0x40

#define FLOATCOMPARE_GE      (FLOATCOMPARECODE_GE | 0x80000000)
#define FLOATCOMPARE_GT      (FLOATCOMPARECODE_GT | 0x80000000)
#define FLOATCOMPARE_LE      (FLOATCOMPARECODE_GT | 0x00000000)
#define FLOATCOMPARE_LT      (FLOATCOMPARECODE_GE | 0x00000000)
#define FLOATCOMPARE_EQ      (FLOATCOMPARECODE_NE | 0x00000000)
#define FLOATCOMPARE_NE      (FLOATCOMPARECODE_NE | 0x80000000)


#define compile_float_branch(i, stackoffset, comparecode, branch_label) \
	do\
	{\
		unsigned char *label_branch;\
		x86_fld_membase(i, ebx, stackoffset, 0);										\
		x86_fp_op_membase(i, X86_FCOMP, ebx, stackoffset+4, 0);							\
		x86_fnstsw(i);																	\
		x86_test_ah_imm8(i, comparecode & 0x7FFFFFFF);									\
		label_branch = i;																\
		x86_branch_disp(i, X86_CC_NE, 1, 0);											\
		x86_mov_reg_imm(i, edx, branch_label);											\
		x86_mov_reg_imm(i, ecx, inst->blockID);											\
		x86_jump_code(i, label_jumpToVPCWithinBlock);									\
		if(comparecode & 0x80000000)													\
			x86_branch(label_branch, X86_CC_NE, i, 0);									\
		else																			\
			x86_branch(label_branch, X86_CC_EQ, i, 0);									\
	} while(0)


#define compile_float_mathop(i, stackoffset, operation) \
	do\
	{\
		x86_fld_membase(i, ebx, stackoffset-4, 0);										\
		x86_fp_op_membase(i, operation, ebx, stackoffset, 0);							\
		x86_fst_membase(i, ebx, stackoffset-4, 0, 1);									\
	} while(0)



#define compile_float_div(i, stackoffset) \
	do\
	{\
		unsigned char *label_jzToNotZero;\
		x86_mov_reg_membase(i, eax, ebx, stackoffset, 4);								\
		x86_alu_reg_imm(i, X86_AND, eax, 0x80000000);									\
		label_jzToNotZero = i;															\
		x86_branch_disp(i, X86_CC_Z, 1, 0);											\
		compile_stopexec(i, EVM_RTERR_DIVISION_BY_ZERO);								\
		x86_branch(label_jzToNotZero, X86_CC_Z, i, 0);								\
		x86_fld_membase(i, ebx, stackoffset-4, 0);										\
		x86_fp_op_membase(i, X86_FDIV, ebx, stackoffset, 0);							\
		x86_fst_membase(i, ebx, stackoffset-4, 0, 1);									\
	} while(0)


#define compile_float_ftol(i, stackoffset) \
	do\
	{\
		x86_fld_membase(i, ebx, stackoffset-4, 0);										\
		x86_fist_pop_membase(i, ebx, stackoffset-4, 1);									\
	} while(0)

#define compile_float_ltof(i, stackoffset) \
	do\
	{\
		/* ??? Why does the fild macro work on 32/64 ??? */\
		x86_fild_membase(i, ebx, stackoffset-4, 0);										\
		x86_fst_membase(i, ebx, stackoffset-4, 0, 1);									\
	} while(0)


#define compile_float_utof(i, stackoffset) \
	do\
	{\
		x86_mov_membase_imm(i, ebp, l_fp64space, 0, 4);									\
		x86_mov_reg_membase(i, eax, ebx, stackoffset-4, 4);								\
		x86_mov_membase_reg(i, ebp, l_fp64space+4, eax, 4);								\
\
		x86_fild_membase(i, ebp, l_fp64space, 1);										\
		x86_fst_membase(i, ebx, stackoffset-4, 0, 1);									\
	} while(0)


// ??? Could I just do an XOR with 0x8000000 ???
#define compile_float_neg(i, stackoffset) \
	do\
	{\
		x86_fld_membase(i, ebx, stackoffset-4, 0);\
		x86_fchs(i);\
		x86_fst_membase(i, ebx, stackoffset-4, 0, 1);\
	} while(0)


#define compile_copy(i, stackoffset, size) \
	do\
	{\
		unsigned char *label_AccessViolation;\
		unsigned char *label_NullPointer;\
		unsigned char *label_jmpPastAccessViolation;\
		x86_mov_reg_membase(i, eax, ebx, stackoffset+4, 4);				\
		x86_mov_reg_membase(i, edx, ebx, stackoffset, 4);				\
		label_jmpPastAccessViolation = i;								\
		x86_jump_disp(i, 1);											\
		label_AccessViolation = i;										\
		compile_stopexec(i, EVM_RTERR_ACCESS_VIOLATION);				\
		label_NullPointer = i;											\
		compile_stopexec(i, EVM_RTERR_NULL_ACCESS);						\
		x86_jump_code(label_jmpPastAccessViolation, i);					\
		/* Check first pointer */\
		x86_alu_reg_imm(i, X86_CMP, eax, 0);							\
		x86_branch(i, X86_CC_EQ, label_NullPointer, 0);					\
		x86_alu_reg_imm(i, X86_CMP, eax, state->vmMemoryConsumed);		\
		x86_branch(i, X86_CC_GE, label_AccessViolation, 0);				\
		x86_alu_reg_imm(i, X86_CMP, eax, state->vmMemoryConsumed-size);	\
		x86_branch(i, X86_CC_GT, label_AccessViolation, 0);				\
		/* Check second pointer */\
		x86_alu_reg_imm(i, X86_CMP, edx, 0);							\
		x86_branch(i, X86_CC_EQ, label_NullPointer, 0);					\
		x86_alu_reg_imm(i, X86_CMP, edx, state->vmMemoryConsumed);		\
		x86_branch(i, X86_CC_GE, label_AccessViolation, 0);				\
		x86_alu_reg_imm(i, X86_CMP, edx, state->vmMemoryConsumed-size);	\
		x86_branch(i, X86_CC_GT, label_AccessViolation, 0);				\
		/* Offset both */\
		x86_alu_reg_membase(i, X86_ADD, eax, ebx, l_vmMemory);			\
		x86_alu_reg_membase(i, X86_ADD, edx, ebx, l_vmMemory);			\
		/* Copy */\
		x86_mov_reg_reg(i, esi, eax, 4);								\
		x86_mov_reg_reg(i, edi, edx, 4);								\
		x86_mov_reg_imm(i, ecx, size/4);								\
		x86_cld(i);														\
		x86_prefix(i, X86_REP_PREFIX);									\
		x86_movsl(i);													\
		if(size & 3)													\
		{																\
			x86_mov_reg_imm(i, ecx, size&3);							\
			x86_prefix(i, X86_REP_PREFIX);								\
			x86_movsb(i);												\
		}																\
	} while(0)


#define compile_call(i, stackoffset) \
	do\
	{\
		unsigned char *label_jneToSameSP;\
		/* Store the PC and save offset */\
		x86_mov_membase_imm(i, ebx, 0, vPC, 4);							\
		x86_mov_membase_imm(i, ebx, 4, stackoffset-4, 4);				\
		/* Get the function address */\
		x86_mov_reg_membase(i, ecx, ebx, stackoffset-4, 4);				\
		x86_alu_reg_imm(i, X86_CMP, ecx, state->vmInstructionCount);	\
		x86_branch(i, X86_CC_LT, label_jumpToVPC, 0);					\
		/* Out of range, have the host deal with it */\
		/* Save the thread data */\
		x86_mov_reg_membase(i, eax, ebp, p_thread, 4);					\
		x86_mov_reg_imm(i, ecx, vPC);									\
		x86_mov_reg_membase(i, edx, ebp, l_sp, 4);						\
		x86_mov_membase_reg(i, eax, byteoffset(evm_thread_t, pc), ecx, 4);	\
		x86_mov_membase_reg(i, eax, byteoffset(evm_thread_t, sp), edx, 4);	\
		/* Call the API function */\
		/* numArgs */\
		x86_mov_reg_imm(i, eax, state->vmMemoryConsumed);				\
		x86_alu_reg_membase(i, X86_SUB, eax, ebp, l_sp);				\
		x86_shift_reg_imm(i, X86_SHR, eax, 2);							\
		x86_push_reg(i, eax);											\
		/* args */\
		x86_mov_reg_reg(i, eax, ebx, 4);								\
		x86_alu_reg_imm(i, X86_ADD, eax, 8);							\
		x86_push_reg(i, eax);											\
		/* returnValue */\
		if(stackoffset != 8)											\
			x86_alu_reg_imm(i, X86_ADD, eax, stackoffset-8);			\
		x86_push_reg(i, eax);											\
		/* pc */\
		x86_push_imm(i, vPC);											\
		/* thread */\
		x86_push_membase(i, ebp, p_thread);								\
		/* Call it */\
		x86_call_membase(i, ebp, l_apifunc);							\
		x86_alu_reg_imm(i, X86_ADD, esp, 40);							\
		/* If it didn't return OK, break out */\
		x86_alu_reg_imm(i, X86_CMP, eax, EVM_OK);						\
		x86_branch(i, X86_CC_NE, label_return, 0);						\
		/* Recover the SP */\
		x86_mov_reg_membase(i, eax, ebp, p_thread, 4);					\
		x86_mov_reg_membase(i, edx, eax, byteoffset(evm_thread_t, sp), 4);	\
		x86_alu_reg_membase(i, X86_CMP, eax, ebp, l_sp);				\
		label_jneToSameSP = i;											\
		x86_branch_disp(i, X86_CC_NE, 1, 0);							\
		/* SP changed */\
		x86_mov_membase_reg(i, ebp, l_sp, eax, 4);						\
		x86_mov_reg_reg(i, ebx, eax, 4);								\
		x86_alu_reg_membase(i, X86_ADD, ebx, ebp, l_vmMemory);			\
		x86_branch(label_jneToSameSP, X86_CC_NE, i, 0);					\
		/* PC is NOT recovered, since it may be modified by VM function */\
		/* calls performed by the API function */\
		/* Fall through and resume code */\
	} while(0)


#define compile_store_arg(i, stackoffset, offset)	\
	do {\
		unsigned char *label_jltToValid;\
		x86_mov_reg_membase(i, eax, ebp, l_sp, 4);								\
		x86_alu_reg_imm(i, X86_CMP, eax, state->vmMemoryConsumed - offset);		\
		label_jltToValid = i;													\
		x86_branch_disp(i, X86_CC_LT, 1, 0);									\
		compile_stopexec(i, EVM_RTERR_ACCESS_VIOLATION);						\
		x86_branch(label_jltToValid, X86_CC_LT, i, 0);							\
		x86_mov_reg_membase(i, eax, ebx, stackoffset, 4);						\
		x86_mov_membase_reg(i, ebx, offset, eax, 4);							\
	} while(0)

#define compile_jump(i, stackoffset)	\
	do {\
		x86_mov_reg_membase(i, edx, ebx, stackoffset, 4);						\
		x86_mov_reg_imm(i, ecx, inst->blockID);									\
		x86_jump_code(i, label_jumpToVPCWithinBlock);							\
	} while(0)


#define compile_bitshift(i, stackoffset, op)	\
	do {\
		x86_mov_reg_membase(i, ecx, ebx, stackoffset, 4);						\
		x86_shift_membase(i, op, ebx, stackOffset-4);						\
	} while(0)



evm_result_t evm_vmJITCompileNative(evm_program_t *state)
{
	unsigned char *i;
	unsigned char *outputCodeBase;

	evm_u32 outputSize = 0;

	unsigned char *outTemp;

	unsigned char *label_return;
	unsigned char *label_jmpToFirstInstruction;
	unsigned char *label_jumpToVPC;
	unsigned char *label_jumpToVPCWithinBlock;
	unsigned char *label_jumpToInstruction;

	unsigned char *label_jneToValidPC;
	unsigned char *label_invalidPC;
	unsigned char *label_jeqToValidPC;

	evm_u32 stackOffset;
	evm_u32 spillOffset;
	evm_u32 size;

	evm_u32 ci;
	evm_instruction_t *inst;

	evm_u32 vPC;

	evm_result_t r;

	unsigned char *label_skipnext = NULL;
	unsigned char *label_jnzToLimitNotExceeded;

	r = evm_vmProfileCode(state);
	if(r != EVM_OK)
		return r;

	if(state->vmCompiledCode)
		evm_free(state->vmCompiledCode);
	state->vmCompiledCode = NULL;

	outTemp = evm_malloc(state->vmInstructionCount*256 + 1000);
	if(!outTemp) return EVM_ERR_OUT_OF_MEMORY;

	i = outputCodeBase = outTemp;

	// Function init code
	x86_push_reg(i, ebp);
	x86_mov_reg_reg(i, ebp, esp, 4);
	x86_alu_reg_imm(i, X86_SUB, esp, JIT_STACK_SPACE_NEEDED);
	x86_push_reg(i, ebx);
	x86_push_reg(i, esi);
	x86_push_reg(i, edi);

	// Get thread
	x86_mov_reg_membase(i, ebx, ebp, p_thread, 4);
	// Load VPC into ECX
	x86_mov_reg_membase(i, ecx, ebx, byteoffset(evm_thread_t, pc), 4);

	// Cache stack pointer
	x86_mov_reg_membase(i, eax, ebx, byteoffset(evm_thread_t, sp), 4);
	x86_mov_membase_reg(i, ebp, l_sp, eax, 4);

	// Load state
	x86_mov_reg_membase(i, ebx, ebx, byteoffset(evm_thread_t, programState), 4);
	// Cache it
	x86_mov_membase_reg(i, ebp, l_state, ebx, 4);

	// Get codebase
	x86_mov_reg_membase(i, eax, ebx, byteoffset(evm_program_t, vmCompiledCode), 4);
	x86_mov_membase_reg(i, ebp, l_codebase, eax, 4);

	// Get host memory access function
	x86_mov_reg_membase(i, eax, ebx, byteoffset(evm_program_t, access_host_memory), 4);
	x86_mov_membase_reg(i, ebp, l_axshostmm, eax, 4);

	// Get host API function
	x86_mov_reg_membase(i, eax, ebx, byteoffset(evm_program_t, apifunction), 4);
	x86_mov_membase_reg(i, ebp, l_apifunc, eax, 4);

	// Get instruction base
	x86_mov_reg_membase(i, eax, ebx, byteoffset(evm_program_t, vmInstructions), 4);
	x86_mov_membase_reg(i, ebp, l_instbase, eax, 4);

	// Get jump counter
	x86_mov_reg_membase(i, eax, ebx, byteoffset(evm_program_t, jumpLimit), 4);
	x86_mov_membase_reg(i, ebp, l_jumplimit, eax, 4);

	// Get memory
	x86_mov_reg_membase(i, ebx, ebx, byteoffset(evm_program_t, vmMemory), 4);
	x86_mov_membase_reg(i, ebp, l_vmMemory, ebx, 4);
	// Localize ebx to the stack
	x86_alu_reg_membase(i, X86_ADD, ebx, ebp, l_sp);

	label_jmpToFirstInstruction = i;

	x86_jump_disp(i, 1);

	label_return = i;

	x86_pop_reg(i, edi);
	x86_pop_reg(i, esi);
	x86_pop_reg(i, ebx);
	x86_mov_reg_reg(i, esp, ebp, 4);
	x86_pop_reg(i, ebp);
	x86_ret(i);

	x86_jump_code(label_jmpToFirstInstruction, i);


	// *** VPC CODE DISPLACEMENT (a.k.a. main jump table) ***
	label_jumpToVPC = i;

	// Check jump counter
	x86_mov_reg_membase(i, eax, ebp, l_jumplimit, 4);
	x86_alu_reg_imm(i, X86_ADD, eax, 0xFFFFFFFF);
	label_jnzToLimitNotExceeded = i;
	x86_branch_disp(i, X86_CC_NZ, 1, 0);
	// Jump counter hit 0
	x86_mov_reg_imm(i, eax, EVM_RTERR_JUMP_LIMIT_EXCEEDED);
	x86_jump_code(i, label_return);
	x86_branch(label_jnzToLimitNotExceeded, X86_CC_NZ, i, 0);

	// ECX contains VPC, which has to get resolved into an actual code location
	x86_alu_reg_imm(i, X86_CMP, ecx, 0);
	label_jneToValidPC = i;
	x86_branch_disp(i, X86_CC_NE, 1, 0);

	// Invalid PC
	label_invalidPC = i;
	x86_mov_reg_imm(i, eax, EVM_RTERR_CODE_ACCESS_VIOLATION);
	x86_jump_code(i, label_return);

	x86_branch(label_jneToValidPC, X86_CC_NE, i, 0);
	// Range check the PC
	x86_alu_reg_imm(i, X86_CMP, ecx, state->vmInstructionCount);
	x86_branch(i, X86_CC_GE, label_invalidPC, 0);	// This is cheesy

	// Multiply ECX by the size of the instruction structure
	x86_mov_reg_imm(i, eax, sizeof(evm_instruction_t));
	x86_mul_reg(i, ecx, 0);
	// Offset by the instruction list address
	x86_alu_reg_membase(i, X86_ADD, eax, ebp, l_instbase);

	// Load the value
	x86_mov_reg_membase(i, eax, eax, byteoffset(evm_instruction_t, jitTranslation), 4);


	// Offset by the codebase pointer
	x86_alu_reg_membase(i, X86_ADD, eax, ebp, l_codebase);
	// Jump
	x86_jump_reg(i, eax);


	// *** VPC CODE DISPLACEMENT WITH BLOCK CHECK ***
	label_jumpToVPCWithinBlock = i;

	// Check jump counter
	x86_mov_reg_membase(i, eax, ebp, l_jumplimit, 4);
	x86_alu_reg_imm(i, X86_ADD, eax, 0xFFFFFFFF);
	label_jnzToLimitNotExceeded = i;
	x86_branch_disp(i, X86_CC_NZ, 1, 0);
	// Jump counter hit 0
	x86_mov_reg_imm(i, eax, EVM_RTERR_JUMP_LIMIT_EXCEEDED);
	x86_jump_code(i, label_return);
	x86_branch(label_jnzToLimitNotExceeded, X86_CC_NZ, i, 0);

	// EDX contains VPC, which has to get resolved into an actual code location
	// ECX contains the block ID jumped from, which must match with the new location
	// This is organized this way because EDX will get clobbered by a MUL
	x86_alu_reg_imm(i, X86_CMP, edx, 0);
	label_jneToValidPC = i;
	x86_branch_disp(i, X86_CC_NE, 1, 0);

	// Invalid PC
	label_invalidPC = i;
	x86_mov_reg_imm(i, eax, EVM_RTERR_CODE_ACCESS_VIOLATION);
	x86_jump_code(i, label_return);

	x86_branch(label_jneToValidPC, X86_CC_NE, i, 0);
	// Multiply EDX by the size of the instruction structure
	x86_mov_reg_imm(i, eax, sizeof(evm_instruction_t));
	x86_mul_reg(i, edx, 0);
	// Offset by the instruction list address
	x86_alu_reg_membase(i, X86_ADD, eax, ebp, l_instbase);

	// Make sure it's within the same block
	x86_alu_reg_membase(i, X86_CMP, ecx, eax, byteoffset(evm_instruction_t, blockID));
	label_jeqToValidPC = i;
	x86_branch_disp(i, X86_CC_EQ, 1, 0);
	x86_mov_reg_imm(i, eax, EVM_RTERR_SUBROUTINE_ZONE_VIOLATION);
	x86_jump_code(i, label_return);

	x86_branch(label_jeqToValidPC, X86_CC_EQ, i, 0);

	// Load the translation
	x86_mov_reg_membase(i, eax, eax, byteoffset(evm_instruction_t, jitTranslation), 4);
	// Offset by the codebase pointer
	x86_alu_reg_membase(i, X86_ADD, eax, ebp, l_codebase);
	// Jump
	x86_jump_reg(i, eax);

	x86_nop(i);
	x86_nop(i);
	x86_nop(i);
	x86_nop(i);
	x86_nop(i);

	// Compile the actual instructions
	inst = state->vmInstructions+1;

	for(ci=1;ci<state->vmInstructionCount;ci++)
	{
		vPC = ci+1;

		inst->jitTranslation = i - outputCodeBase;
		if(inst->opcode == EVM_OP_BEGINSUB)
			spillOffset = inst->argument.ul - (inst->maxStackLevel * 4);

		stackOffset = (inst->maxStackLevel * 4) + spillOffset;

		size = 0;


		switch(inst->opcode)
		{
		default:
			return EVM_RTERR_UNIMPLEMENTED_OPCODE;

			//*****************************************
		case EVM_OP_SEX8:
			x86_widen_membase(i, eax, ebx, stackOffset-4, 1, 0);
			x86_mov_membase_reg(i, ebx, stackOffset-4, eax, 4);
			break;
		case EVM_OP_SEX16:
			x86_widen_membase(i, eax, ebx, stackOffset-4, 1, 1);
			x86_mov_membase_reg(i, ebx, stackOffset-4, eax, 4);
			break;

		case EVM_OP_ZHI24:
			x86_alu_membase_imm(i, X86_AND, ebx, stackOffset-4, 0x000000FF);
			break;
		case EVM_OP_ZHI16:
			x86_alu_membase_imm(i, X86_AND, ebx, stackOffset-4, 0x0000FFFF);
			break;


		case EVM_OP_JUMP:
			compile_jump(i, stackOffset);
			break;

		case EVM_OP_BSL:
			compile_bitshift(i, stackOffset, X86_SHL);
			break;
		case EVM_OP_BSRI:
			compile_bitshift(i, stackOffset, X86_SAR);
			break;
		case EVM_OP_BSRU:
			compile_bitshift(i, stackOffset, X86_SHR);
			break;

		case EVM_OP_DISCARD:
			// Drop8 and Drop16 do nothing on little-endian architecture
		case EVM_OP_DROP8:
		case EVM_OP_DROP16:
			break;

		case EVM_OP_NEGF:
			compile_float_neg(i, stackOffset);
			break;

		case EVM_OP_CALL:
			compile_call(i, stackOffset);
			break;
		case EVM_OP_NEGL:
			x86_neg_membase(i, ebx, stackOffset-4);
			break;
		case EVM_OP_BNOT:
			x86_not_membase(i, ebx, stackOffset-4);
			break;

		case EVM_OP_SKIPNEXT:
			label_skipnext = i;
			x86_jump32_auto(i, 0);
			break;

		
		case EVM_OP_BEQ:
			compile_branch(i, stackOffset, X86_CC_EQ, 0, inst->argument.ul);
			break;
		case EVM_OP_BEQF:
			compile_float_branch(i, stackOffset, FLOATCOMPARE_EQ, inst->argument.ul);
			break;
		case EVM_OP_BNE:
			compile_branch(i, stackOffset, X86_CC_NE, 0, inst->argument.ul);
			break;
		case EVM_OP_BNEF:
			compile_float_branch(i, stackOffset, FLOATCOMPARE_NE, inst->argument.ul);
			break;

		case EVM_OP_BGEF:
			compile_float_branch(i, stackOffset, FLOATCOMPARE_GE, inst->argument.ul);
			break;
		case EVM_OP_BGEU:
			compile_branch(i, stackOffset, X86_CC_GE, 0, inst->argument.ul);
			break;
		case EVM_OP_BGEL:
			compile_branch(i, stackOffset, X86_CC_GE, 1, inst->argument.ul);
			break;

		case EVM_OP_BGTF:
			compile_float_branch(i, stackOffset, FLOATCOMPARE_GT, inst->argument.ul);
			break;
		case EVM_OP_BGTU:
			compile_branch(i, stackOffset, X86_CC_GT, 0, inst->argument.ul);
			break;
		case EVM_OP_BGTL:
			compile_branch(i, stackOffset, X86_CC_GT, 1, inst->argument.ul);
			break;

		case EVM_OP_BLEF:
			compile_float_branch(i, stackOffset, FLOATCOMPARE_LE, inst->argument.ul);
			break;
		case EVM_OP_BLEU:
			compile_branch(i, stackOffset, X86_CC_LE, 0, inst->argument.ul);
			break;
		case EVM_OP_BLEL:
			compile_branch(i, stackOffset, X86_CC_LE, 1, inst->argument.ul);
			break;

		case EVM_OP_BLTF:
			compile_float_branch(i, stackOffset, FLOATCOMPARE_LT, inst->argument.ul);
			break;
		case EVM_OP_BLTU:
			compile_branch(i, stackOffset, X86_CC_LT, 0, inst->argument.ul);
			break;
		case EVM_OP_BLTL:
			compile_branch(i, stackOffset, X86_CC_LT, 1, inst->argument.ul);
			break;

		
		case EVM_OP_STORE_SPR4:
			compile_store_arg(i, stackOffset, inst->argument.ul);
			break;

		case EVM_OP_LTOF:
			compile_float_ltof(i, stackOffset);
			break;
		case EVM_OP_UTOF:
			compile_float_utof(i, stackOffset);
			break;
		case EVM_OP_FTOL:
			compile_float_ftol(i, stackOffset);
			break;

		case EVM_OP_ADDF:
			compile_float_mathop(i, stackOffset, X86_FADD);
			break;
		case EVM_OP_SUBF:
			compile_float_mathop(i, stackOffset, X86_FSUB);
			break;
		case EVM_OP_MULF:
			compile_float_mathop(i, stackOffset, X86_FMUL);
			break;
		case EVM_OP_DIVF:
			compile_float_div(i, stackOffset);
			break;

		case EVM_OP_COPY:
			compile_copy(i, stackOffset, inst->argument.ul);
			break;

		case EVM_OP_DIVU:
			compile_divide(i, stackOffset, 0, 0);
			break;
		case EVM_OP_MODU:
			compile_divide(i, stackOffset, 1, 0);
			break;
		case EVM_OP_DIVL:
			compile_divide(i, stackOffset, 0, 1);
			break;
		case EVM_OP_MODL:
			compile_divide(i, stackOffset, 1, 1);
			break;

		case EVM_OP_MULU:
		case EVM_OP_MULL:
			// These are actually the same... No idea why, but it works out, so whatever
			compile_multiply(i, stackOffset);
			break;

		case EVM_OP_ADDL:
			compile_mathop(i, X86_ADD, stackOffset);
			break;
		case EVM_OP_SUBL:
			compile_mathop_oneway(i, X86_SUB, stackOffset);
			break;
		case EVM_OP_BAND:
			compile_mathop(i, X86_AND, stackOffset);
			break;
		case EVM_OP_BOR:
			compile_mathop(i, X86_OR, stackOffset);
			break;
		case EVM_OP_BXOR:
			compile_mathop(i, X86_XOR, stackOffset);
			break;

		case EVM_OP_ADDR_SPR:
		case EVM_OP_ADDR_ARG:
			compile_addr_spr(i, stackOffset, inst->argument.ul);
			break;


		case EVM_OP_STORE1:
			size = 1;
		case EVM_OP_STORE2:
			if(!size) size = 2;
		case EVM_OP_STORE4:
			if(!size) size = 4;
			compile_store(i, stackOffset, size);
			break;

		case EVM_OP_LOAD1:
			size = 1;
		case EVM_OP_LOAD2:
			if(!size) size = 2;
		case EVM_OP_LOAD4:
			if(!size) size = 4;
			compile_load(i, stackOffset, size);
			break;

		
		case EVM_OP_IMMEDIATE:
			compile_immediate(i, stackOffset, inst->argument.ul);
			break;
		case EVM_OP_BEGINSUB:
			compile_beginsub(i, inst->argument.ul);
			break;
		case EVM_OP_ENDSUB:
			compile_endsub(i, inst->argument.ul, stackOffset);
			break;
		}

#ifdef _DEBUG
		x86_nop(i);
		x86_mov_reg_imm(i, eax, vPC);
#endif

		// Patch in skipnext if needed
		if(label_skipnext && inst->opcode != EVM_OP_SKIPNEXT)
		{
			x86_jump32_auto(label_skipnext, i-label_skipnext);
			label_skipnext = NULL;
		}

		inst++;
	}

	outputSize = i - outputCodeBase;

	state->vmCompiledCode = evm_realloc(outputCodeBase, outputSize);

	if(!state->vmCompiledCode)
		return EVM_ERR_OUT_OF_MEMORY;

	state->vmCompiledCodeSize = outputSize;

	return EVM_OK;
}
