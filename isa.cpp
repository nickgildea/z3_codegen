#include	"isa.h"

using namespace z3;

// ====================================================================================================================
// ====================================================================================================================

void fmt_reg_reg(const int opcodeIdx, const int instrIdx, const EvalOperands& opers)
{
	printf("  r%d = %s r%d r%d\n", instrIdx, ISA_OpName(opcodeIdx), opers.x, opers.y);
}

void fmt_reg_imm(const int opcodeIdx, const int instrIdx, const EvalOperands& opers)
{
	printf("  r%d = %s r%d 0x%x\n", instrIdx, ISA_OpName(opcodeIdx), opers.x, opers.imm32);
}

void fmt_imm(const int opcodeIdx, const int instrIdx, const EvalOperands& opers)
{
	printf("  r%d = %s 0x%x\n", instrIdx, ISA_OpName(opcodeIdx), opers.imm32);
}

// ====================================================================================================================
// ====================================================================================================================

expr sim_set(SimOperands& opers) { return opers.imm32; }

expr sim_add(SimOperands& opers) { return opers.x + opers.y; }
expr sim_sub(SimOperands& opers) { return opers.x - opers.y; }
expr sim_mul(SimOperands& opers) { return opers.x * opers.y; }

expr sim_xor(SimOperands& opers) { return opers.x ^ opers.y; }
expr sim_and(SimOperands& opers) { return opers.x & opers.y; }
expr sim_or (SimOperands& opers) { return opers.x | opers.y; }

expr sim_xor_not(SimOperands& opers) { return opers.x ^ (~opers.y); }
expr sim_and_not(SimOperands& opers) { return opers.x & ~opers.y; }
expr sim_or_not (SimOperands& opers) { return opers.x | ~opers.y; }

expr sim_shl(SimOperands& opers) { return to_expr(opers.ctx, Z3_mk_bvshl(opers.ctx, opers.x, opers.imm32)); }
expr sim_shr(SimOperands& opers) { return to_expr(opers.ctx, Z3_mk_bvashr(opers.ctx, opers.x, opers.imm32)); }

expr sim_gt(SimOperands& opers) 
{ 
	// a more complicated/powerful instruction: return either ~0 or 0 as the result of a comparison
	// "ite" is if-then-else: if the comparison is true return ~0 else 0
	return ite(opers.x > opers.y, opers.ctx.bv_val(0xffffffff, 32), opers.ctx.bv_val(0, 32));
}

// ====================================================================================================================
// ====================================================================================================================

ValueType eval_set(const EvalOperands& opers) { return opers.imm32; }

ValueType eval_add(const EvalOperands& opers) { return opers.x + opers.y; }
ValueType eval_sub(const EvalOperands& opers) { return opers.x - opers.y; }
ValueType eval_mul(const EvalOperands& opers) { return opers.x * opers.y; }

ValueType eval_xor(const EvalOperands& opers) { return opers.x ^ opers.y; }
ValueType eval_and(const EvalOperands& opers) { return opers.x & opers.y; }
ValueType eval_or (const EvalOperands& opers) { return opers.x | opers.y; }

ValueType eval_xor_not(const EvalOperands& opers) { return opers.x ^ (~opers.y); }
ValueType eval_and_not(const EvalOperands& opers) { return opers.x & ~opers.y; }
ValueType eval_or_not (const EvalOperands& opers) { return opers.x | ~opers.y; }

ValueType eval_shl(const EvalOperands& opers) { return opers.x << opers.imm32; }
ValueType eval_shr(const EvalOperands& opers) { return opers.x >> opers.imm32; }

// the C version is much simpler!
ValueType eval_gt(const EvalOperands& opers) { return opers.x > opers.y ? 0xffffffff : 0; }

// ====================================================================================================================
// ====================================================================================================================

static const Instruction ISA[] =
{
	// rather than having multiple versions of each opcode with different operands
	// a single opcode is implemented to allow an immediate value to be introduced
	// into the instruction stream (this signficantly reduces the search space)
	Instruction( "set", fmt_imm, sim_set, eval_set ),

	Instruction( "add", fmt_reg_reg, sim_add, eval_add ),
	Instruction( "sub", fmt_reg_reg, sim_sub, eval_sub ),
	Instruction( "mul", fmt_reg_reg, sim_mul, eval_mul ),

	Instruction( "xor", fmt_reg_reg, sim_xor, eval_xor ),
	Instruction( "and", fmt_reg_reg, sim_and, eval_and ),
	Instruction( "or" , fmt_reg_reg, sim_or,  eval_or ),

	Instruction( "xor_not", fmt_reg_reg, sim_xor_not, eval_xor_not ),
	Instruction( "and_not", fmt_reg_reg, sim_and_not, eval_and_not ),
	Instruction( "or_not" , fmt_reg_reg, sim_or_not,  eval_or_not ),

	Instruction( "shl", fmt_reg_imm, sim_shl, eval_shl, Instruction::Kind_Shift ),
	Instruction( "shr", fmt_reg_imm, sim_shr, eval_shr, Instruction::Kind_Shift ),

	Instruction( "gt", fmt_reg_reg, sim_gt, eval_gt ),
};

// ====================================================================================================================
// ====================================================================================================================

int ISA_NumOpCodes()
{
	return sizeof(ISA) / sizeof(ISA[0]);
}

// ====================================================================================================================
// ====================================================================================================================

const char* ISA_OpName(const int opCode)
{
	return ISA[opCode].name_;
}

// ====================================================================================================================
// ====================================================================================================================

int ISA_OpCodeForName(const char* name)
{
	for (int i = 0; i < ISA_NumOpCodes(); i++)
	{
		if (strcmp(ISA[i].name_, name) == 0)
		{
			return i;
		}
	}

	printf("Unknown opcode name: %s\n", name);
	exit(1);

	return -1;
}

// ====================================================================================================================
// ====================================================================================================================

void ISA_FormatOp(const int opcodeIdx, const int instrIdx, const EvalOperands& operands)
{
	ISA[opcodeIdx].fmt_(opcodeIdx, instrIdx, operands);
}

// ====================================================================================================================
// ====================================================================================================================

z3::expr ISA_SimulateOp(const int opcodeIdx, SimOperands& operands)
{
	return ISA[opcodeIdx].sim_(operands);
}

// ====================================================================================================================
// ====================================================================================================================

ValueType ISA_EvaluateOp(const int opcodeIdx, const EvalOperands& operands)
{
	return ISA[opcodeIdx].eval_(operands);
}

// ====================================================================================================================
// ====================================================================================================================

std::vector<int> ISA_OpCodesForKindMask(const int kindMask)
{
	std::vector<int> opCodes;
	for (int idx = 0; idx < ISA_NumOpCodes(); idx++)
	{
		if (ISA[idx].kindMask & kindMask)
		{
			opCodes.push_back(idx);
		}
	}

	return opCodes;
}

// ====================================================================================================================
// ====================================================================================================================

