#ifndef		ISA_H_HAS_BEEN_INCLUDED
#define		ISA_H_HAS_BEEN_INCLUDED

#include	<vector>
#include	<z3++.h>

// ====================================================================================================================
// ====================================================================================================================

// ====================================================================================================================
// ====================================================================================================================

using ValueType = int32_t;
//using ValueType = uint32_t;

template <typename OperandT>
struct Operands
{
	Operands(
		z3::context& _ctx, 
		const OperandT& _x, 
		const OperandT& _y, 
		const OperandT& _imm32)
		: ctx(_ctx)
		, x(_x)
		, y(_y)
		, imm32(_imm32)
	{
	}

	z3::context& ctx;
	OperandT x;
	OperandT y;
	OperandT imm32;
};

using SimOperands = Operands<z3::expr>;
using EvalOperands = Operands<ValueType>;

// ====================================================================================================================
// ====================================================================================================================

#include <functional>
using FmtFn = std::function<void(const int, const int, const EvalOperands&)>;
using EvalFn = std::function<ValueType(const EvalOperands&)>;
using SimFn = std::function<z3::expr(SimOperands&)>;

struct Instruction
{
	const static int Kind_None          = 0;
	const static int Kind_Shift         = 1 << 0;

	Instruction(const char* name, FmtFn fmt, SimFn sim, EvalFn eval, const int _kindMask = Kind_None)
		: name_(name)
		, kindMask(_kindMask)
		, fmt_(fmt)
		, sim_(sim)
		, eval_(eval)
	{
	}

	const char* name_ = nullptr;
	int kindMask = Kind_None;
	FmtFn fmt_ = nullptr;
	SimFn sim_ = nullptr;
	EvalFn eval_ = nullptr;
};

// ====================================================================================================================
// ====================================================================================================================

int ISA_NumOpCodes();
const char* ISA_OpName(const int opCode);
int ISA_OpCodeForName(const char* name);
void ISA_FormatOp(const int opcodeIdx, const int instrIdx, const EvalOperands& operands); 
z3::expr ISA_SimulateOp(const int opcodeIdx, SimOperands& operands);
ValueType ISA_EvaluateOp(const int opcodeIdx, const EvalOperands& operands);
std::vector<int> ISA_OpCodesForKindMask(const int kindMask);

// ====================================================================================================================
// ====================================================================================================================

// Wrapper around the whole ISA to allow only selecting a subset, maps 0 -> (n-1) to the ISA[] opcodes
class ISASubset
{
public:

	void addOpcode(const int opcode)
	{
		instOpcodes_.push_back(opcode);
	}

	const char* opName(const int localID)
	{
		return ISA_OpName(instOpcodes_[localID]);
	}

	int opCodeForName(const char* name)
	{
		for (int i = 0; i < instOpcodes_.size(); i++)
		{
			if (strcmp(ISA_OpName(instOpcodes_[i]), name) == 0)
			{
				return i;
			}
		}

		return -1;
	}

	void formatOp(const int localID, const int instrIdx, const EvalOperands& operands)
	{
		return ISA_FormatOp(instOpcodes_[localID], instrIdx, operands);
	}

	z3::expr simulateOp(const int localID, SimOperands& operands)
	{
		return ISA_SimulateOp(instOpcodes_[localID], operands);
	}

	ValueType evaluateOp(const int localID, const EvalOperands& operands)
	{
		return ISA_EvaluateOp(instOpcodes_[localID], operands);
	}

	std::vector<int> opCodesForKindMask(const int kindMask)
	{
		std::vector<int> localIDs;
		const std::vector<int> opcodes = ISA_OpCodesForKindMask(kindMask);
		for (const int opcode: opcodes)
		{
			for (int i = 0; i < instOpcodes_.size(); i++)
			{
				if (opcode == instOpcodes_[i])
				{
					localIDs.push_back(i);
					break;
				}
			}
		}

		return localIDs;
	}

	int size() const 
	{ 
		return static_cast<int>(instOpcodes_.size());
	}

private:

	std::vector<int> instOpcodes_;
};

// ====================================================================================================================
// ====================================================================================================================

#endif //  ISA_H_HAS_BEEN_INCLUDED
