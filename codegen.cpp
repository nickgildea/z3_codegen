#include	<z3++.h>
#include	<vector>
#include	<chrono>
#include	<random>

#include	"isa.h"

// ====================================================================================================================
// ====================================================================================================================

std::mt19937 prng;
std::uniform_int_distribution<ValueType> rndDist(-INT_MAX - 1, INT_MAX);

// ====================================================================================================================
// ====================================================================================================================

ValueType TargetFunc(const ValueType x)
{
//	return x >= 0 ? x : 1 - x;
	return x >= 0 ? x : -x;
}

// ====================================================================================================================
// ====================================================================================================================

using expr_vector_array = std::vector<z3::expr_vector>;

class CodeGenContext
{
public:

	CodeGenContext(z3::context& _ctx, const int _numInputs, const int _numChains, const int _numSteps, const ISASubset& _isa)
		: ctx(_ctx)
		, solver(_ctx)
		, numInputs(_numInputs)
		, numChains(_numChains)
		, numInstr(_numSteps)
		, opCode(_ctx)
		, regX(_ctx)
		, regY(_ctx)
		, imm32(_ctx)
		, isa(_isa)
	{
	}

	z3::context&        ctx;
	z3::solver          solver;
	int                 numInputs = 0;
	int                 numChains = 0;
	int                 numInstr = 0;
	z3::expr_vector     opCode;
	z3::expr_vector     regX;
	z3::expr_vector     regY;
	z3::expr_vector     imm32;
	expr_vector_array   R;

	ISASubset           isa;
};

// ====================================================================================================================
// ====================================================================================================================

void CreateConstants(CodeGenContext& codeGen)
{
	for (int c = 0; c < codeGen.numChains; c++)
	{
		z3::expr_vector chainR(codeGen.ctx);
		for (int idx = 0; idx < codeGen.numInstr; idx++)
		{
			char name[16];
			sprintf(name, "R%d_c%d", idx, c);
			chainR.push_back(codeGen.ctx.bv_const(name, 32));
		}

		codeGen.R.push_back(chainR);
	}

	for (int idx = 0; idx < codeGen.numInstr; idx++)
	{
		char name[16];

		sprintf(name, "opCode_s%d", idx);
		codeGen.opCode.push_back(codeGen.ctx.int_const(name));

		sprintf(name, "regX_s%d", idx);
		codeGen.regX.push_back(codeGen.ctx.int_const(name));

		sprintf(name, "regY_s%d", idx);
		codeGen.regY.push_back(codeGen.ctx.int_const(name));

		sprintf(name, "imm32_s%d", idx);
		codeGen.imm32.push_back(codeGen.ctx.bv_const(name, 32));
	}
}

// ====================================================================================================================
// ====================================================================================================================

void AddConstraints(CodeGenContext& codeGen)
{
	codeGen.solver = z3::solver(codeGen.ctx);

	for (int idx = codeGen.numInputs; idx < codeGen.numInstr; idx++)
	{
		codeGen.solver.add(codeGen.opCode[idx] >= 0);
		codeGen.solver.add(codeGen.opCode[idx] < codeGen.isa.size());

		codeGen.solver.add(codeGen.regX[idx] >= 0);
		codeGen.solver.add(codeGen.regX[idx] < idx);

		codeGen.solver.add(codeGen.regY[idx] >= 0);
		codeGen.solver.add(codeGen.regY[idx] < idx);

		z3::expr_vector shiftConstraints(codeGen.ctx);
		shiftConstraints.push_back(codeGen.imm32[idx] > 0);
		shiftConstraints.push_back(codeGen.imm32[idx] <= 31);
		z3::expr andShiftConstriants = z3::mk_and(shiftConstraints);

		for (const int shiftOpCode: codeGen.isa.opCodesForKindMask(Instruction::Kind_Shift))
		{
			codeGen.solver.add(z3::to_expr(codeGen.ctx, z3::implies(codeGen.opCode[idx] == shiftOpCode, andShiftConstriants)));
		}
	}
}

// ====================================================================================================================
// ====================================================================================================================

z3::expr SelectOperand(CodeGenContext& codeGen, z3::expr_vector& R, const z3::expr& regIdx, const int instructionIdx)
{
	z3::expr cond = codeGen.ctx.bv_val(0, 32);
	for (int i = instructionIdx - 1; i >= 0; i--)
	{
		cond = z3::to_expr(codeGen.ctx, z3::ite(regIdx == i, R[i], cond));
	}

	return cond;
}

// ====================================================================================================================
// ====================================================================================================================

void AddPerChainConstraints(CodeGenContext& codeGen)
{
	for (int c = 0; c < codeGen.numChains; c++)
	{
		auto& chainR = codeGen.R[c];

		const ValueType x = rndDist(prng);
		const ValueType out = TargetFunc(x);

		codeGen.solver.add(chainR[0] == codeGen.ctx.bv_val(x, 32));
		codeGen.solver.add(chainR[codeGen.numInstr - 1] == codeGen.ctx.bv_val(out, 32));

		for (int idx = codeGen.numInputs; idx < codeGen.numInstr; idx++)
		{
			const auto& op = codeGen.opCode[idx];
			const auto& x = SelectOperand(codeGen, chainR, codeGen.regX[idx], idx);
			const auto& y = op != codeGen.isa.opCodeForName("set") ? 
				SelectOperand(codeGen, chainR, codeGen.regY[idx], idx) :
				codeGen.ctx.bv_val(0, 32);

			const auto& imm = codeGen.imm32[idx];
			auto opers = SimOperands(codeGen.ctx, x, y, imm);

			z3::expr cond = codeGen.ctx.bv_val(0, 32);
			for (int opcodeIdx = codeGen.isa.size() - 1; opcodeIdx >= 0; opcodeIdx--)
			{
				cond = z3::to_expr(codeGen.ctx, z3::ite(op == opcodeIdx, codeGen.isa.simulateOp(opcodeIdx, opers), cond));
			}

			codeGen.solver.add(chainR[idx] == cond);
		}
	}
}

// ====================================================================================================================
// ====================================================================================================================

EvalOperands CreateEvalOperands(CodeGenContext& codeGen, const int instructionIdx, int& opcodeIdx)
{
	const auto model = codeGen.solver.get_model();
	const auto op    = model.eval(codeGen.opCode[instructionIdx]).get_numeral_int();
	const auto x     = model.eval(codeGen.regX[instructionIdx]).get_numeral_int();
	const auto y     = model.eval(codeGen.regY[instructionIdx]).get_numeral_int();

	// imm32 is not always retreivable so need to check first
	const auto imm_  = model.eval(codeGen.imm32[instructionIdx]);
	const auto imm   = imm_.is_numeral() ? imm_.get_numeral_uint() : 0;

	opcodeIdx = op;
	return EvalOperands(codeGen.ctx, x, y, imm);
}

// ====================================================================================================================
// ====================================================================================================================

void PrintModel(CodeGenContext& codeGen, int numInputs)
{
	printf("Generated code:\n");

	for (int i = 0; i < numInputs; i++)
	{
		printf("  r%d = <input>\n", i);
	}

	for (int instrIdx = numInputs; instrIdx < codeGen.numInstr; instrIdx++)
	{
		int opcodeIdx = -1;
		const auto opers = CreateEvalOperands(codeGen, instrIdx, opcodeIdx);
		codeGen.isa.formatOp(opcodeIdx, instrIdx, opers);
	}

	printf("\n");
}

// ====================================================================================================================
// ====================================================================================================================

std::vector<ValueType> Evaluate(CodeGenContext& codeGen, const std::vector<ValueType>& initValues)
{
	std::vector<ValueType> evalState(codeGen.numInstr);
	for (int i = 0; i < initValues.size(); i++)
	{
		evalState[i] = initValues[i];
	}

	for (int instrIdx = codeGen.numInputs; instrIdx < codeGen.numInstr; instrIdx++)
	{
		int opcodeIdx = -1;
		auto opers = CreateEvalOperands(codeGen, instrIdx, opcodeIdx);

		// if we're evaluation we want x & y to have the values of the registers
		opers.x = evalState[opers.x];
		opers.y = evalState[opers.y];

		evalState[instrIdx] = codeGen.isa.evaluateOp(opcodeIdx, opers);
	}

	return evalState;
}

// ====================================================================================================================
// ====================================================================================================================

z3::check_result Solve(CodeGenContext& codeGen)
{
	const auto start = std::chrono::high_resolution_clock::now();
	const auto res = codeGen.solver.check();
	const auto end = std::chrono::high_resolution_clock::now();
	const auto delta = end - start;
	const auto delta_ms = std::chrono::duration_cast<std::chrono::milliseconds>(delta);
	printf("  solver.check() call completed: %d ms\n", delta_ms);

	return res;
}

// ====================================================================================================================
// ====================================================================================================================

static bool EvaluateModel(CodeGenContext& codeGen)
{
	int numPassed = 0;
	const ValueType x = rndDist(prng);
	const ValueType y = rndDist(prng);
	const ValueType expected = TargetFunc(x);

	const auto stateValues = Evaluate(codeGen, {x, y});
	const ValueType actual = stateValues.back();
	return  expected == actual;
}

// ====================================================================================================================
// ====================================================================================================================

bool FindSolution(const int numInstructions, const ISASubset& isa)
{
	z3::context ctx;

	const int numChains = 10;
	const int numInputs = 1;

	CodeGenContext codeGen(ctx, numInputs, numChains, numInstructions, isa);

	CreateConstants(codeGen);
	AddConstraints(codeGen);
	AddPerChainConstraints(codeGen);

	const auto res = Solve(codeGen);
	if (res != z3::sat)
	{
		printf("  unsatifiable\n");
		return false;
	}

	printf("  satisified!\n\n");

	PrintModel(codeGen, numInputs);

	const int NUM_TESTS = 10000;
	int numPassed = 0;
	printf("Testing with random values...\n");
	for (int i = 0; i < NUM_TESTS; i++)
	{
		if (!EvaluateModel(codeGen))
		{
			break;
		}

		numPassed++;
	}
	printf("  %d / %d passed\n\n", numPassed, NUM_TESTS);

	return true;
}

// ====================================================================================================================
// ====================================================================================================================

int main(int argc, char** argv)
{
	// Don't need to always use the full ISA
	ISASubset isa;
	isa.addOpcode(ISA_OpCodeForName("set"));
//	isa.addOpcode(ISA_OpCodeForName("add"));
	isa.addOpcode(ISA_OpCodeForName("sub"));
	isa.addOpcode(ISA_OpCodeForName("xor"));
//	isa.addOpcode(ISA_OpCodeForName("shr"));
	isa.addOpcode(ISA_OpCodeForName("gt"));

	const int minInstructions = 2;
	const int maxInstructions = 8;
	for (int i = 2; i < maxInstructions; i++)
	{
		try
		{
			printf("Try with %d instructions...\n", i);
			if (FindSolution(i, isa))
			{
				break;
			}
		}
		catch (z3::exception& e)
		{
			std::cout << e.msg() << std::endl;
		}
	}
}

// ====================================================================================================================
// ====================================================================================================================

