//
//  Instruction.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 15/01/21.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef InstructionSets_M50740_Instruction_h
#define InstructionSets_M50740_Instruction_h

#include <cstdint>
#include <iomanip>
#include <string>
#include <sstream>
#include "../AccessType.hpp"

namespace InstructionSet {
namespace M50740 {

enum class AddressingMode {
	Implied,				Accumulator,			Immediate,
	Absolute,				AbsoluteX,				AbsoluteY,
	ZeroPage,				ZeroPageX,				ZeroPageY,
	XIndirect,				IndirectY,
	Relative,
	AbsoluteIndirect,		ZeroPageIndirect,
	SpecialPage,
	ImmediateZeroPage,
	AccumulatorRelative,	ZeroPageRelative
};

static constexpr auto MaxAddressingMode = int(AddressingMode::ZeroPageRelative);
static constexpr auto MinAddressingMode = int(AddressingMode::Implied);

constexpr int size(AddressingMode mode) {
	// This is coupled to the AddressingMode list above; be careful!
	constexpr int sizes[] = {
		0, 0, 1,
		2, 2, 2,
		1, 1, 1,
		1, 1,
		1,
		2, 1,
		1,
		2,
		1,	2
	};
	static_assert(sizeof(sizes)/sizeof(*sizes) == int(MaxAddressingMode) + 1);
	return sizes[int(mode)];
}

enum class Operation: uint8_t {
	Invalid,

	// Operations that don't access memory.
	BBC0,	BBC1,	BBC2,	BBC3,	BBC4,	BBC5,	BBC6,	BBC7,
	BBS0,	BBS1,	BBS2,	BBS3,	BBS4,	BBS5,	BBS6,	BBS7,
	BCC,	BCS,
	BEQ,	BMI,	BNE,	BPL,
	BVC,	BVS,	BRA,	BRK,
	JMP,	JSR,
	RTI,	RTS,
	CLC,	CLD,	CLI,	CLT,	CLV,
	SEC,	SED,	SEI,	SET,
	INX,	INY,	DEX,	DEY,
	FST,	SLW,
	NOP,
	PHA, 	PHP, 	PLA,	PLP,
	STP,
	TAX,	TAY,	TSX,	TXA,
	TXS,	TYA,

	// Read operations.
	ADC,	SBC,
	AND,	ORA,	EOR,	BIT,
	CMP,	CPX,	CPY,
	LDA,	LDX,	LDY,
	TST,

	// Read-modify-write operations.
	ASL,	LSR,
	CLB0,	CLB1,	CLB2,	CLB3,	CLB4,	CLB5,	CLB6,	CLB7,
	SEB0,	SEB1,	SEB2,	SEB3,	SEB4,	SEB5,	SEB6,	SEB7,
	COM,
	DEC,	INC,
	ROL,	ROR,	RRF,

	// Write operations.
	LDM,
	STA,	STX,	STY,
};

static constexpr auto MaxOperation = int(Operation::STY);
static constexpr auto MinOperation = int(Operation::BBC0);

constexpr AccessType access_type(Operation operation) {
	if(operation < Operation::ADC)	return AccessType::None;
	if(operation < Operation::ASL)	return AccessType::Read;
	if(operation < Operation::LDM)	return AccessType::ReadModifyWrite;
	return AccessType::Write;
}

constexpr bool uses_index_mode(Operation operation) {
	return
		operation == Operation::ADC || operation == Operation::AND ||
		operation == Operation::CMP || operation == Operation::EOR ||
		operation == Operation::LDA || operation == Operation::ORA ||
		operation == Operation::SBC;
}

/*!
	@returns The name of @c operation.
*/
inline constexpr const char *operation_name(Operation operation) {
#define MAP(x)	case Operation::x: return #x;
	switch(operation) {
		default: break;
		MAP(BBC0);	MAP(BBC1);	MAP(BBC2);	MAP(BBC3);	MAP(BBC4);	MAP(BBC5);	MAP(BBC6);	MAP(BBC7);
		MAP(BBS0);	MAP(BBS1);	MAP(BBS2);	MAP(BBS3);	MAP(BBS4);	MAP(BBS5);	MAP(BBS6);	MAP(BBS7);
		MAP(BCC);	MAP(BCS);	MAP(BEQ);	MAP(BMI);	MAP(BNE);	MAP(BPL);	MAP(BVC);	MAP(BVS);
		MAP(BRA);	MAP(BRK);	MAP(JMP);	MAP(JSR);	MAP(RTI);	MAP(RTS);	MAP(CLC);	MAP(CLD);
		MAP(CLI);	MAP(CLT);	MAP(CLV);	MAP(SEC);	MAP(SED);	MAP(SEI);	MAP(SET);	MAP(INX);
		MAP(INY);	MAP(DEX);	MAP(DEY);	MAP(FST);	MAP(SLW);	MAP(NOP);	MAP(PHA); 	MAP(PHP);
		MAP(PLA);	MAP(PLP);	MAP(STP);	MAP(TAX);	MAP(TAY);	MAP(TSX);	MAP(TXA);	MAP(TXS);
		MAP(TYA);	MAP(ADC);	MAP(SBC);	MAP(AND);	MAP(ORA);	MAP(EOR);	MAP(BIT);	MAP(CMP);
		MAP(CPX);	MAP(CPY);	MAP(LDA);	MAP(LDX);	MAP(LDY);	MAP(TST);	MAP(ASL);	MAP(LSR);
		MAP(CLB0);	MAP(CLB1);	MAP(CLB2);	MAP(CLB3);	MAP(CLB4);	MAP(CLB5);	MAP(CLB6);	MAP(CLB7);
		MAP(SEB0);	MAP(SEB1);	MAP(SEB2);	MAP(SEB3);	MAP(SEB4);	MAP(SEB5);	MAP(SEB6);	MAP(SEB7);
		MAP(COM);	MAP(DEC);	MAP(INC);	MAP(ROL);	MAP(ROR);	MAP(RRF);	MAP(LDM);	MAP(STA);
		MAP(STX);	MAP(STY);
	}
#undef MAP

	return "???";
}

/*!
	@returns The name of @c addressing_mode.
*/
inline constexpr const char *addressing_mode_name(AddressingMode addressing_mode) {
	switch(addressing_mode) {
		default: break;
		case AddressingMode::Implied:				return "";
		case AddressingMode::Accumulator:			return "A";
		case AddressingMode::Immediate:				return "#";
		case AddressingMode::Absolute:				return "abs";
		case AddressingMode::AbsoluteX:				return "abs, x";
		case AddressingMode::AbsoluteY:				return "abs, y";
		case AddressingMode::ZeroPage:				return "zp";
		case AddressingMode::ZeroPageX:				return "zp, x";
		case AddressingMode::ZeroPageY:				return "zp, y";
		case AddressingMode::XIndirect:				return "((zp, x))";
		case AddressingMode::IndirectY:				return "((zp), y)";
		case AddressingMode::Relative:				return "rel";
		case AddressingMode::AbsoluteIndirect:		return "(abs)";
		case AddressingMode::ZeroPageIndirect:		return "(zp)";
		case AddressingMode::SpecialPage:			return "\\sp";
		case AddressingMode::ImmediateZeroPage:		return "#, zp";
		case AddressingMode::AccumulatorRelative:	return "A, rel";
		case AddressingMode::ZeroPageRelative:		return "zp, rel";
	}

	return "???";
}

/*!
	@returns The way that the address for an operation with @c addressing_mode and encoded starting from @c operation
		would appear in an assembler. E.g. '$5a' for that zero page address, or '$5a, x' for zero-page indexed from $5a. This function
		may access up to three bytes from @c operation onwards.
*/
inline std::string address(AddressingMode addressing_mode, const uint8_t *operation, uint16_t program_counter) {
	std::stringstream output;
	output << std::hex;

#define NUM(x) std::setfill('0') << std::setw(2) << int(x)
#define NUM4(x) std::setfill('0') << std::setw(4) << int(x)
	switch(addressing_mode) {
		default: 									return "???";
		case AddressingMode::Implied:				return "";
		case AddressingMode::Accumulator:			return "A ";
		case AddressingMode::Immediate:				output << "#$" << NUM(operation[1]);									break;
		case AddressingMode::Absolute:				output << "$" << NUM(operation[2]) << NUM(operation[1]);				break;
		case AddressingMode::AbsoluteX:				output << "$" << NUM(operation[2]) << NUM(operation[1]) << ", x";		break;
		case AddressingMode::AbsoluteY:				output << "$" << NUM(operation[2]) << NUM(operation[1]) << ", y";		break;
		case AddressingMode::ZeroPage:				output << "$" << NUM(operation[1]);										break;
		case AddressingMode::ZeroPageX:				output << "$" << NUM(operation[1]) << ", x";							break;
		case AddressingMode::ZeroPageY:				output << "$" << NUM(operation[1]) << ", y";							break;
		case AddressingMode::XIndirect:				output << "(($" << NUM(operation[1]) << ", x))";						break;
		case AddressingMode::IndirectY:				output << "(($" << NUM(operation[1]) << "), y)";						break;
		case AddressingMode::Relative:				output << "#$" << NUM4(2 + program_counter + int8_t(operation[1]));		break;
		case AddressingMode::AbsoluteIndirect:		output << "($" << NUM(operation[2]) << NUM(operation[1]) << ") ";		break;
		case AddressingMode::ZeroPageIndirect:		output << "($" << NUM(operation[1]) << ")";								break;
		case AddressingMode::SpecialPage:			output << "$1f" << NUM(operation[1]);									break;
		case AddressingMode::ImmediateZeroPage:		output << "#$" << NUM(operation[1]) << ", $"  << NUM(operation[2]);		break;
		case AddressingMode::AccumulatorRelative:	output << "A, $"  << NUM4(2 + program_counter + int8_t(operation[1]));	break;
		case AddressingMode::ZeroPageRelative:
			output << "$" << NUM(operation[1]) << ", $"  << NUM4(2 + program_counter + int8_t(operation[2]));
		break;
	}
#undef NUM4
#undef NUM

	return output.str();
}

/*!
	Models a complete M50740-style instruction, including its operation, addressing mode and opcode.
*/
struct Instruction {
	Operation operation = Operation::Invalid;
	AddressingMode addressing_mode = AddressingMode::Implied;
	uint8_t opcode = 0;

	Instruction(Operation operation, AddressingMode addressing_mode, uint8_t opcode) : operation(operation), addressing_mode(addressing_mode), opcode(opcode) {}
	Instruction(uint8_t opcode) : opcode(opcode) {}
	Instruction() {}
};

/*!
	Outputs a description of @c instruction to @c stream.
*/
inline std::ostream &operator <<(std::ostream &stream, const Instruction &instruction) {
	stream << operation_name(instruction.operation) << " " << addressing_mode_name(instruction.addressing_mode);
	return stream;
}

}
}


#endif /* InstructionSets_M50740_Instruction_h */
