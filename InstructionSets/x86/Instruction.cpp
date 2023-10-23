//
//  Instruction.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#include "Instruction.hpp"

#include "../../Numeric/Carry.hpp"

#include <cassert>
#include <iomanip>
#include <sstream>

using namespace InstructionSet::x86;

bool InstructionSet::x86::has_displacement(Operation operation) {
	switch(operation) {
		default: return false;

		case Operation::JO:			case Operation::JNO:
		case Operation::JB:			case Operation::JNB:
		case Operation::JZ:			case Operation::JNZ:
		case Operation::JBE:		case Operation::JNBE:
		case Operation::JS:			case Operation::JNS:
		case Operation::JP:			case Operation::JNP:
		case Operation::JL:			case Operation::JNL:
		case Operation::JLE:		case Operation::JNLE:
		case Operation::LOOPNE:		case Operation::LOOPE:
		case Operation::LOOP:		case Operation::JCXZ:
		case Operation::CALLrel:	case Operation::JMPrel:
			return true;
	}
}

int InstructionSet::x86::max_displayed_operands(Operation operation) {
	switch(operation) {
		default:	return 2;

		case Operation::INC:	case Operation::DEC:
		case Operation::POP:	case Operation::PUSH:
		case Operation::MUL:	case Operation::IMUL_1:
		case Operation::IDIV:	case Operation::DIV:
		case Operation::ESC:
		case Operation::AAM:	case Operation::AAD:
		case Operation::INT:
		case Operation::JMPabs:	case Operation::JMPfar:
		case Operation::CALLabs:	case Operation::CALLfar:
		case Operation::NEG:	case Operation::NOT:
		case Operation::RETnear:
		case Operation::RETfar:
			return 1;

		// Pedantically, these have an displacement rather than an operand.
		case Operation::JO:		case Operation::JNO:
		case Operation::JB:		case Operation::JNB:
		case Operation::JZ:		case Operation::JNZ:
		case Operation::JBE:	case Operation::JNBE:
		case Operation::JS:		case Operation::JNS:
		case Operation::JP:		case Operation::JNP:
		case Operation::JL:		case Operation::JNL:
		case Operation::JLE:	case Operation::JNLE:
		case Operation::LOOPNE:		case Operation::LOOPE:
		case Operation::LOOP:		case Operation::JCXZ:
		case Operation::CALLrel:	case Operation::JMPrel:
		// Genuine zero-operand instructions:
		case Operation::CMPS:	case Operation::LODS:
		case Operation::MOVS:	case Operation::SCAS:
		case Operation::STOS:
		case Operation::CLC:	case Operation::CLD:
		case Operation::CLI:
		case Operation::STC:	case Operation::STD:
		case Operation::STI:
		case Operation::CMC:
		case Operation::LAHF:	case Operation::SAHF:
		case Operation::AAA:	case Operation::AAS:
		case Operation::DAA:	case Operation::DAS:
		case Operation::CBW:	case Operation::CWD:
		case Operation::INTO:
		case Operation::PUSHF:	case Operation::POPF:
		case Operation::IRET:
		case Operation::NOP:
		case Operation::XLAT:
		case Operation::SALC:
		case Operation::Invalid:
			return 0;
	}
}

std::string InstructionSet::x86::to_string(Operation operation, DataSize size, Model model) {
	switch(operation) {
		case Operation::AAA:	return "aaa";
		case Operation::AAD:	return "aad";
		case Operation::AAM:	return "aam";
		case Operation::AAS:	return "aas";
		case Operation::DAA:	return "daa";
		case Operation::DAS:	return "das";

		case Operation::CBW:	return "cbw";
		case Operation::CWD:	return "cwd";
		case Operation::ESC:	return "esc";

		case Operation::HLT:	return "hlt";
		case Operation::WAIT:	return "wait";

		case Operation::ADC:	return "adc";
		case Operation::ADD:	return "add";
		case Operation::SBB:	return "sbb";
		case Operation::SUB:	return "sub";
		case Operation::MUL:	return "mul";
		case Operation::IMUL_1:	return "imul";
		case Operation::DIV:	return "div";
		case Operation::IDIV:	return "idiv";

		case Operation::INC:	return "inc";
		case Operation::DEC:	return "dec";

		case Operation::IN:		return "in";
		case Operation::OUT:	return "out";

		case Operation::JO:		return "jo";
		case Operation::JNO:	return "jno";
		case Operation::JB:		return "jb";
		case Operation::JNB:	return "jnb";
		case Operation::JZ:		return "jz";
		case Operation::JNZ:	return "jnz";
		case Operation::JBE:	return "jbe";
		case Operation::JNBE:	return "jnbe";
		case Operation::JS:		return "js";
		case Operation::JNS:	return "jns";
		case Operation::JP:		return "jp";
		case Operation::JNP:	return "jnp";
		case Operation::JL:		return "jl";
		case Operation::JNL:	return "jnl";
		case Operation::JLE:	return "jle";
		case Operation::JNLE:	return "jnle";

		case Operation::CALLabs:	return "call";
		case Operation::CALLrel:	return "call";
		case Operation::CALLfar:	return "callf";
		case Operation::IRET:		return "iret";
		case Operation::RETfar:		return "retf";
		case Operation::RETnear:	return "retn";
		case Operation::JMPabs:		return "jmp";
		case Operation::JMPrel:		return "jmp";
		case Operation::JMPfar:		return "jmpf";
		case Operation::JCXZ:		return "jcxz";
		case Operation::INT:		return "int";
		case Operation::INTO:		return "into";

		case Operation::LAHF:	return "lahf";
		case Operation::SAHF:	return "sahf";
		case Operation::LDS:	return "lds";
		case Operation::LES:	return "les";
		case Operation::LEA:	return "lea";

		case Operation::CMPS: {
			constexpr char sizes[][6] = { "cmpsb", "cmpsw", "cmpsd", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Operation::LODS: {
			constexpr char sizes[][6] = { "lodsb", "lodsw", "lodsd", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Operation::MOVS: {
			constexpr char sizes[][6] = { "movsb", "movsw", "movsd", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Operation::SCAS: {
			constexpr char sizes[][6] = { "scasb", "scasw", "scasd", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Operation::STOS: {
			constexpr char sizes[][6] = { "stosb", "stosw", "stosd", "?" };
			return sizes[static_cast<int>(size)];
		}

		case Operation::LOOP:	return "loop";
		case Operation::LOOPE:	return "loope";
		case Operation::LOOPNE:	return "loopne";

		case Operation::MOV:	return "mov";
		case Operation::NEG:	return "neg";
		case Operation::NOT:	return "not";
		case Operation::AND:	return "and";
		case Operation::OR:		return "or";
		case Operation::XOR:	return "xor";
		case Operation::NOP:	return "nop";
		case Operation::POP:	return "pop";
		case Operation::POPF:	return "popf";
		case Operation::PUSH:	return "push";
		case Operation::PUSHF:	return "pushf";
		case Operation::RCL:	return "rcl";
		case Operation::RCR:	return "rcr";
		case Operation::ROL:	return "rol";
		case Operation::ROR:	return "ror";
		case Operation::SAL:	return "sal";
		case Operation::SAR:	return "sar";
		case Operation::SHR:	return "shr";

		case Operation::CLC:	return "clc";
		case Operation::CLD:	return "cld";
		case Operation::CLI:	return "cli";
		case Operation::STC:	return "stc";
		case Operation::STD:	return "std";
		case Operation::STI:	return "sti";
		case Operation::CMC:	return "cmc";

		case Operation::CMP:	return "cmp";
		case Operation::TEST:	return "test";

		case Operation::XCHG:	return "xchg";
		case Operation::XLAT:	return "xlat";
		case Operation::SALC:	return "salc";

		case Operation::SETMO:
			if(model == Model::i8086) {
				return "setmo";
			} else {
				return  "enter";
			}

		case Operation::SETMOC:
			if(model == Model::i8086) {
				return "setmoc";
			} else {
				return "bound";
			}

		case Operation::Invalid:	return "invalid";

		default:
			assert(false);
			return "";
	}
}

bool InstructionSet::x86::mnemonic_implies_data_size(Operation operation) {
	switch(operation) {
		default:	return false;

		case Operation::CMPS:
		case Operation::LODS:
		case Operation::MOVS:
		case Operation::SCAS:
		case Operation::STOS:
		case Operation::JMPrel:
		case Operation::LEA:
			return true;
	}
}

std::string InstructionSet::x86::to_string(DataSize size) {
	constexpr char sizes[][6] = { "byte", "word", "dword", "?" };
	return sizes[static_cast<int>(size)];
}

std::string InstructionSet::x86::to_string(Source source, DataSize size) {
	switch(source) {
		case Source::eAX: {
			constexpr char sizes[][4] = { "al", "ax", "eax", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eCX: {
			constexpr char sizes[][4] = { "cl", "cx", "ecx", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eDX: {
			constexpr char sizes[][4] = { "dl", "dx", "edx", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eBX: {
			constexpr char sizes[][4] = { "bl", "bx", "ebx", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eSPorAH: {
			constexpr char sizes[][4] = { "ah", "sp", "esp", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eBPorCH: {
			constexpr char sizes[][4] = { "ch", "bp", "ebp", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eSIorDH: {
			constexpr char sizes[][4] = { "dh", "si", "esi", "?" };
			return sizes[static_cast<int>(size)];
		}
		case Source::eDIorBH: {
			constexpr char sizes[][4] = { "bh", "di", "edi", "?" };
			return sizes[static_cast<int>(size)];
		}

		case Source::ES:	return "es";
		case Source::CS:	return "cs";
		case Source::SS:	return "ss";
		case Source::DS:	return "ds";
		case Source::FS:	return "fd";
		case Source::GS:	return "gs";

		case Source::None:				return "0";
		case Source::DirectAddress:		return "DirectAccess";
		case Source::Immediate:			return "Immediate";
		case Source::Indirect:			return "Indirect";
		case Source::IndirectNoBase:	return "IndirectNoBase";

		default: return "???";
	}
}

namespace {

std::string to_hex(int value, int digits, bool with_suffix = true) {
	auto stream = std::stringstream();
	stream << std::setfill('0') << std::uppercase << std::hex << std::setw(digits);
	switch(digits) {
		case 2: stream << +uint8_t(value);	break;
		case 4: stream << +uint16_t(value);	break;
		default: stream << value;	break;
	}
	if (with_suffix) stream << 'h';
	return stream.str();
};

}

template <bool is_32bit>
std::string InstructionSet::x86::to_string(
	DataPointer pointer,
	Instruction<is_32bit> instruction,
	int offset_length,
	int immediate_length,
	DataSize operation_size
) {
	if(operation_size == InstructionSet::x86::DataSize::None) operation_size = instruction.operation_size();

	std::string operand;

	auto append = [](std::stringstream &stream, auto value, int length) {
		switch(length) {
			case 0:
				if(!value) {
					return;
				}
				[[fallthrough]];

			case 2:
				value &= 0xff;
			break;
		}

		stream << std::uppercase << std::hex << value << 'h';
	};

	auto append_signed = [](std::stringstream &stream, auto value, int length) {
		if(!value && !length) {
			return;
		}

		const bool is_negative = Numeric::top_bit<decltype(value)>() & value;
		const uint64_t abs_value = std::abs(int16_t(value));	// TODO: don't assume 16-bit.

		stream << (is_negative ? '-' : '+') << std::uppercase << std::hex << abs_value << 'h';
	};
	using Source = InstructionSet::x86::Source;
	const Source source = pointer.source<false>();
	switch(source) {
		// to_string handles all direct register names correctly.
		default:	return InstructionSet::x86::to_string(source, operation_size);

		case Source::Immediate: {
			std::stringstream stream;
			append(stream, instruction.operand(), immediate_length);
			return stream.str();
		}

		case Source::DirectAddress:
		case Source::Indirect:
		case Source::IndirectNoBase: {
			std::stringstream stream;

			if(!InstructionSet::x86::mnemonic_implies_data_size(instruction.operation)) {
				stream << InstructionSet::x86::to_string(operation_size) << ' ';
			}

			stream << '[';
			Source segment = instruction.segment_override();
			if(segment == Source::None) {
				segment = pointer.default_segment();
				if(segment == Source::None) {
					segment = Source::DS;
				}
			}
			stream << InstructionSet::x86::to_string(segment, InstructionSet::x86::DataSize::None) << ':';

			bool addOffset = false;
			switch(source) {
				default: break;
				case Source::Indirect:
					stream << InstructionSet::x86::to_string(pointer.base(), data_size(instruction.address_size()));
					if(pointer.index() != Source::None) {
						stream << '+' << InstructionSet::x86::to_string(pointer.index(), data_size(instruction.address_size()));
					}
					addOffset = true;
				break;
				case Source::IndirectNoBase:
					stream << InstructionSet::x86::to_string(pointer.index(), data_size(instruction.address_size()));
					addOffset = true;
				break;
				case Source::DirectAddress:
					stream << std::uppercase << std::hex << instruction.offset() << 'h';
				break;
			}
			if(addOffset) {
				append_signed(stream, instruction.offset(), offset_length);
			}
			stream << ']';
			return stream.str();
		}
	}

	return operand;
};

template<bool is_32bit>
std::string InstructionSet::x86::to_string(
	Instruction<is_32bit> instruction,
	Model model,
	int offset_length,
	int immediate_length
) {
	std::string operation;

	// Add a repetition prefix; it'll be one of 'rep', 'repe' or 'repne'.
	switch(instruction.repetition()) {
		case Repetition::None: break;
		case Repetition::RepE:
			switch(instruction.operation) {
				default:
					operation += "repe ";
				break;

				case Operation::MOVS:
				case Operation::STOS:
				case Operation::LODS:
					operation += "rep ";
				break;
			}
		break;
		case Repetition::RepNE:
			operation += "repne ";
		break;
	}

	// Add operation itself.
	operation += to_string(instruction.operation, instruction.operation_size(), model);
	operation += " ";

	// Deal with a few special cases up front.
	switch(instruction.operation) {
		default: {
			const int operands = max_displayed_operands(instruction.operation);
			const bool displacement = has_displacement(instruction.operation);
			const bool print_first = operands > 1 && instruction.destination().source() != Source::None;
			if(print_first) {
				operation += to_string(instruction.destination(), instruction, offset_length, immediate_length);
			}
			if(operands > 0 && instruction.source().source() != Source::None) {
				if(print_first) operation += ", ";
				operation += to_string(instruction.source(), instruction, offset_length, immediate_length);
			}
			if(displacement) {
				operation += to_hex(instruction.displacement(), offset_length);
			}
		} break;

		case Operation::CALLfar:
		case Operation::JMPfar: {
			switch(instruction.destination().source()) {
				case Source::Immediate:
					operation += "far 0x";
					operation += to_hex(instruction.segment(), 4, false);
					operation += ":0x";
					operation += to_hex(instruction.offset(), 4, false);
				break;
				default:
					operation += to_string(instruction.destination(), instruction, offset_length, immediate_length);
				break;
			}
		} break;

		case Operation::LDS:
		case Operation::LES:	// The test set labels the pointer type as dword, which I guess is technically accurate.
								// A full 32 bits will be loaded from that address in 16-bit mode.
			operation += to_string(instruction.destination(), instruction, offset_length, immediate_length);
			operation += ", ";
			operation += to_string(instruction.source(), instruction, offset_length, immediate_length, InstructionSet::x86::DataSize::DWord);
		break;

		case Operation::IN:
			operation += to_string(instruction.destination(), instruction, offset_length, immediate_length);
			operation += ", ";
			switch(instruction.source().source()) {
				case Source::DirectAddress:
					operation += to_hex(instruction.offset(), 2, true);
				break;
				default:
					operation += to_string(instruction.source(), instruction, offset_length, immediate_length, InstructionSet::x86::DataSize::Word);
				break;
			}
		break;

		case Operation::OUT:
			switch(instruction.destination().source()) {
				case Source::DirectAddress:
					operation += to_hex(instruction.offset(), 2, true);
				break;
				default:
					operation += to_string(instruction.destination(), instruction, offset_length, immediate_length, InstructionSet::x86::DataSize::Word);
				break;
			}
			operation += ", ";
			operation += to_string(instruction.source(), instruction, offset_length, immediate_length);
		break;

		// Rolls and shifts list eCX as a source on the understanding that everyone knows that rolls and shifts
		// use CL even when they're shifting or rolling a word-sized quantity.
		case Operation::RCL:	case Operation::RCR:
		case Operation::ROL:	case Operation::ROR:
		case Operation::SAL:	case Operation::SAR:
		case Operation::SHR:
		case Operation::SETMO:	case Operation::SETMOC:
			operation += to_string(instruction.destination(), instruction, offset_length, immediate_length);
			switch(instruction.source().source()) {
				case Source::None:	break;
				case Source::eCX:	operation += ", cl"; break;
				case Source::Immediate:
					// Providing an immediate operand of 1 is a little future-proofing by the decoder; the '1'
					// is actually implicit on a real 8088. So omit it.
					if(instruction.operand() == 1) break;
					[[fallthrough]];
				default:
					operation += ", ";
					operation += to_string(instruction.source(), instruction, offset_length, immediate_length);
				break;
			}
		break;
	}

	return operation;
}

// Although advertised, 32-bit printing is incomplete.
//
//template std::string InstructionSet::x86::to_string(
//	Instruction<true> instruction,
//	Model model,
//	int offset_length,
//	int immediate_length
//);

template std::string InstructionSet::x86::to_string(
	Instruction<false> instruction,
	Model model,
	int offset_length,
	int immediate_length
);
