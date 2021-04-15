//
//  Z80ContentionTests.cpp
//  Clock SignalTests
//
//  Created by Thomas Harte on 7/4/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Processors/Z80/Z80.hpp"

namespace {

static constexpr uint16_t initial_pc = 0x0000;
static constexpr uint16_t initial_ir = 0xe000;
static constexpr uint16_t initial_bc_de_hl = 0xabcd;
static constexpr uint16_t initial_ix_iy = 0x3412;
static constexpr uint16_t initial_sp = 0x6800;

struct ContentionCheck {
	uint16_t address;
	int length;
	bool is_io = false;
};

struct CapturingZ80: public CPU::Z80::BusHandler {

	template <typename Collection> CapturingZ80(const Collection &code) : z80_(*this) {
		// Take a copy of the code.
		std::copy(code.begin(), code.end(), ram_);
		code_length_ = uint16_t(code.size());

		// Skip the three cycles the Z80 spends on a reset, and
		// purge them from the record.
		run_for(3);
		bus_records_.clear();
		contentions_48k_.clear();

		// Set the flags so that if this is a conditional operation, it'll succeed.
		if((*code.begin())&0x8) {
			z80_.set_value_of_register(CPU::Z80::Register::Flags, 0xff);
		} else {
			z80_.set_value_of_register(CPU::Z80::Register::Flags, 0x00);
		}

		// Set the refresh address to the EE page and set A to 0x80.
		z80_.set_value_of_register(CPU::Z80::Register::I, 0xe0);
		z80_.set_value_of_register(CPU::Z80::Register::A, 0x80);

		// Set BC, DE and HL.
		z80_.set_value_of_register(CPU::Z80::Register::BC, initial_bc_de_hl);
		z80_.set_value_of_register(CPU::Z80::Register::DE, initial_bc_de_hl);
		z80_.set_value_of_register(CPU::Z80::Register::HL, initial_bc_de_hl);

		// Set IX and IY.
		z80_.set_value_of_register(CPU::Z80::Register::IX, initial_ix_iy);
		z80_.set_value_of_register(CPU::Z80::Register::IY, initial_ix_iy);

		// Set SP.
		z80_.set_value_of_register(CPU::Z80::Register::StackPointer, initial_sp);
	}

	void set_de(uint16_t value) {
		z80_.set_value_of_register(CPU::Z80::Register::DE, value);
	}

	void set_bc(uint16_t value) {
		z80_.set_value_of_register(CPU::Z80::Register::BC, value);
	}

	void run_for(int cycles) {
		z80_.run_for(HalfCycles(Cycles(cycles)));
		XCTAssertEqual(bus_records_.size(), cycles * 2);
	}

	/// A record of the state of the address bus, MREQ, IOREQ and RFSH lines,
	/// upon every clock transition.
	struct BusRecord {
		uint16_t address = 0xffff;
		bool mreq = false, ioreq = false, refresh = false;
	};

	HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
		//
		// Log the plain bus activity.
		//
		const uint8_t *const bus_state = cycle.bus_state();
		for(int c = 0; c < cycle.length.as<int>(); c++) {
			bus_records_.emplace_back();

			// TODO: I think everything tested here should have an address,
			// but am currently unsure whether the reset program puts the
			// address bus in high impedance, as bus req/ack does.
			if(cycle.address) {
				bus_records_.back().address = *cycle.address;
			}
			bus_records_.back().mreq = bus_state[c] & CPU::Z80::PartialMachineCycle::Line::MREQ;
			bus_records_.back().ioreq = bus_state[c] & CPU::Z80::PartialMachineCycle::Line::IOREQ;
			bus_records_.back().refresh = bus_state[c] & CPU::Z80::PartialMachineCycle::Line::RFSH;
		}

		//
		// Attempt to predict how the 48kb would contend this, based on machine cycle alone.
		//
		switch(cycle.operation) {
			default: assert(false);

			case CPU::Z80::PartialMachineCycle::ReadStart:
			case CPU::Z80::PartialMachineCycle::WriteStart:
				contentions_48k_.emplace_back();
				contentions_48k_.back().address = *cycle.address;
				contentions_48k_.back().length = 3;
			break;

			case CPU::Z80::PartialMachineCycle::ReadOpcodeStart:
				contentions_48k_.emplace_back();
				contentions_48k_.back().address = *cycle.address;
				contentions_48k_.back().length = 4;
			break;

			case CPU::Z80::PartialMachineCycle::InputStart:
			case CPU::Z80::PartialMachineCycle::OutputStart:
				contentions_48k_.emplace_back();
				contentions_48k_.back().address = *cycle.address;
				contentions_48k_.back().length = 4;
				contentions_48k_.back().is_io = true;
			break;

			case CPU::Z80::PartialMachineCycle::Refresh:
			case CPU::Z80::PartialMachineCycle::Read:
			case CPU::Z80::PartialMachineCycle::Write:
			case CPU::Z80::PartialMachineCycle::ReadOpcode:
			case CPU::Z80::PartialMachineCycle::Input:
			case CPU::Z80::PartialMachineCycle::Output:
			case CPU::Z80::PartialMachineCycle::InputWait:
			case CPU::Z80::PartialMachineCycle::OutputWait:
				// All already accounted for.
			break;

			case CPU::Z80::PartialMachineCycle::Internal:
				for(int c = 0; c < cycle.length.cycles().as<int>(); c++) {
					contentions_48k_.emplace_back();
					contentions_48k_.back().address = *cycle.address;
					contentions_48k_.back().length = 1;
				}
			break;
		}


		//
		// Do the actual action: albeit, only reads.
		//
		if(
			cycle.operation == CPU::Z80::PartialMachineCycle::Read ||
			cycle.operation == CPU::Z80::PartialMachineCycle::ReadOpcode
		) {
			*cycle.value = ram_[*cycle.address];

			XCTAssert(cycle.operation != CPU::Z80::PartialMachineCycle::ReadOpcode || *cycle.address < code_length_);
		}

		return HalfCycles(0);
	}

	const std::vector<BusRecord> &bus_records() const {
		return bus_records_;
	}

	std::vector<BusRecord> cycle_records() const {
		std::vector<BusRecord> cycle_records;
		for(size_t c = 0; c < bus_records_.size(); c += 2) {
			cycle_records.push_back(bus_records_[c]);
		}
		return cycle_records;
	}

	const std::vector<ContentionCheck> &contention_predictions_48k() const {
		return contentions_48k_;
	}

	private:
		CPU::Z80::Processor<CapturingZ80, false, false> z80_;
		uint8_t ram_[65536];
		uint16_t code_length_ = 0;

		std::vector<BusRecord> bus_records_;
		std::vector<ContentionCheck> contentions_48k_;
};

}

@interface Z80ContentionTests : XCTestCase
@end

/*!
	Tests the Z80's MREQ, IOREQ and address outputs for correlation to those
	observed by ZX Spectrum users in the software-side documentation of
	contended memory timings.
*/
@implementation Z80ContentionTests

template <typename LHSCollectionT, typename RHSCollectionT>
	void compareExpectedContentions(LHSCollectionT lhs, RHSCollectionT rhs, const char *label)
{
	XCTAssertEqual(lhs.size(), rhs.size(), "[%s] found %lu contentions but expected %zu", label, lhs.size(), rhs.size());

	auto contention = lhs.begin();
	auto found_contention = rhs.begin();

	while(contention != lhs.end() && found_contention != rhs.end()) {
		XCTAssertEqual(contention->address, found_contention->address, "[%s] mismatched address at step %zu; expected %04x but found %04x", label, contention - lhs.begin(), contention->address, found_contention->address);
		XCTAssertEqual(contention->length, found_contention->length, "[%s] mismatched length at step %zu; expected %d but found %d", label, contention - lhs.begin(), contention->length, found_contention->length);
		XCTAssertEqual(contention->is_io, found_contention->is_io, "[%s] mismatched IO flag at step %zu; expected %d but found %d", label, contention - lhs.begin(), contention->is_io, found_contention->is_io);

		if(contention->address != found_contention->address || contention->length != found_contention->length) {
			break;
		}

		++contention;
		++found_contention;
	}
}

/*!
	Checks that the accumulated bus activity in @c z80 matches the expectations given in @c contentions if
	processed by a Sinclair 48k or 128k ULA.
*/
- (void)validate48Contention:(const std::initializer_list<ContentionCheck> &)contentions z80:(const CapturingZ80 &)z80 {
	// 48[/128]k contention logic: triggered on address alone, _unless_
	// MREQ is also active.
	const auto bus_records = z80.cycle_records();
	std::vector<ContentionCheck> found_contentions;

	int count = 0;
	uint16_t address = bus_records.front().address;
	bool is_io = false;

	for(size_t c = 0; c < bus_records.size(); c++) {
		++count;

		if(
			c &&					// i.e. not at front.
			!bus_records[c].mreq &&	// i.e. beginning of a new contention.
			!bus_records[c].ioreq	// i.e. not during an IO cycle.
		) {
			found_contentions.emplace_back();
			found_contentions.back().address = address;
			found_contentions.back().length = count - 1;
			found_contentions.back().is_io = is_io;

			count = 1;
			address = bus_records[c].address;
		}

		is_io = bus_records[c].ioreq;
	}

	found_contentions.emplace_back();
	found_contentions.back().address = address;
	found_contentions.back().length = count;
	found_contentions.back().is_io = is_io;

	compareExpectedContentions(contentions, found_contentions, "48kb");
	compareExpectedContentions(z80.contention_predictions_48k(), found_contentions, "48kb prediction");
}

/*!
	Checks that the accumulated bus activity in @c z80 matches the expectations given in @c contentions if
	processed by an Amstrad gate array.
*/
- (void)validatePlus3Contention:(const std::initializer_list<ContentionCheck> &)contentions z80:(const CapturingZ80 &)z80 {
	// +3 contention logic: triggered by the leading edge of MREQ, sans refresh.
	const auto bus_records = z80.bus_records();
	std::vector<ContentionCheck> found_contentions;

	int count = 0;
	uint16_t address = bus_records.front().address;
	bool is_io = false;

	for(size_t c = 0; c < bus_records.size(); c += 2) {
		++count;

		// The IOREQ test below is a little inauthentic; it's included to match the published Spectrum
		// tables, even though the +3 doesn't contend IO.
		const bool is_mreq_leading_edge = !bus_records[c].mreq && bus_records[c+1].mreq && !bus_records[c].refresh;
		const bool is_ioreq_leading_edge = c < bus_records.size() - 2 && !bus_records[c].ioreq && bus_records[c+2].ioreq;
		if(
			c &&				// i.e. not at front.
			(is_mreq_leading_edge || is_ioreq_leading_edge)		// i.e. beginning of a new contention.
		) {
			found_contentions.emplace_back();
			found_contentions.back().address = address;
			found_contentions.back().length = count - 1;
			found_contentions.back().is_io = is_io;

			count = 1;
			address = bus_records[c].address;
		}

		is_io = bus_records[c].ioreq;
	}

	found_contentions.emplace_back();
	found_contentions.back().address = address;
	found_contentions.back().length = count;
	found_contentions.back().is_io = is_io;

	compareExpectedContentions(contentions, found_contentions, "+3");
}

// MARK: - Opcode tests.

- (void)testSimpleOneBytes {
	for(uint8_t opcode : {
		0x00,	// NOP

		// LD r, r'.
		0x40, 0x41, 0x42, 0x43, 0x44, 0x45, 0x47,
		0x48, 0x49, 0x4a, 0x4b, 0x4c, 0x4d, 0x4f,
		0x50, 0x51, 0x52, 0x53, 0x54, 0x55, 0x57,
		0x58, 0x59, 0x5a, 0x5b, 0x5c, 0x5d, 0x5f,
		0x60, 0x61, 0x62, 0x63, 0x64, 0x65, 0x67,
		0x68, 0x69, 0x6a, 0x6b, 0x6c, 0x6d, 0x6f,

		// ALO a, r
		0x80, 0x81, 0x82, 0x83, 0x84, 0x85, 0x87,
		0x88, 0x89, 0x8a, 0x8b, 0x8c, 0x8d, 0x8f,
		0x90, 0x91, 0x92, 0x93, 0x94, 0x95, 0x97,
		0x98, 0x99, 0x9a, 0x9b, 0x9c, 0x9d, 0x9f,
		0xa0, 0xa1, 0xa2, 0xa3, 0xa4, 0xa5, 0xa7,
		0xa8, 0xa9, 0xaa, 0xab, 0xac, 0xad, 0xaf,
		0xb0, 0xb1, 0xb2, 0xb3, 0xb4, 0xb5, 0xb7,
		0xb8, 0xb9, 0xba, 0xbb, 0xbc, 0xbd, 0xbf,

		// INC/DEC r
		0x04, 0x05, 0x0c, 0x0d,
		0x14, 0x15, 0x1c, 0x1d,
		0x24, 0x25, 0x2c, 0x2d,

		0xd9,	// EXX
		0x08,	// EX AF, AF'
		0xeb,	// EX DE, HL
		0x27,	// DAA
		0x2f,	// CPL
		0x3f,	// CCF
		0x37,	// SCF
		0xf3,	// DI
		0xfb,	// EI
		0x17,	// RLA
		0x1f,	// RRA
		0x07,	// RLCA
		0x0f,	// RRCA
		0xe9,	// JP (HL)
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(4);

		[self validate48Contention:{{initial_pc, 4}} z80:z80];
		[self validatePlus3Contention:{{initial_pc, 4}} z80:z80];
	}
}

- (void)testSimpleTwoBytes {
	// This group should apparently also include 'NOPD'. Neither I nor any other
	// page I could find seems to have heard of 'NOPD'.

	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// SRO d (i.e. RLC, RRC, RL, RR, SLA, SRA, SRL and SLL)
		{0xcb, 0x00},	{0xcb, 0x01},	{0xcb, 0x02},	{0xcb, 0x03},
		{0xcb, 0x04},	{0xcb, 0x05},	{0xcb, 0x07},
		{0xcb, 0x08},	{0xcb, 0x09},	{0xcb, 0x0a},	{0xcb, 0x0b},
		{0xcb, 0x0c},	{0xcb, 0x0d},	{0xcb, 0x0f},
		{0xcb, 0x10},	{0xcb, 0x11},	{0xcb, 0x12},	{0xcb, 0x13},
		{0xcb, 0x14},	{0xcb, 0x15},	{0xcb, 0x17},
		{0xcb, 0x18},	{0xcb, 0x19},	{0xcb, 0x1a},	{0xcb, 0x1b},
		{0xcb, 0x1c},	{0xcb, 0x1d},	{0xcb, 0x1f},
		{0xcb, 0x20},	{0xcb, 0x21},	{0xcb, 0x22},	{0xcb, 0x23},
		{0xcb, 0x24},	{0xcb, 0x25},	{0xcb, 0x27},
		{0xcb, 0x28},	{0xcb, 0x29},	{0xcb, 0x2a},	{0xcb, 0x2b},
		{0xcb, 0x2c},	{0xcb, 0x2d},	{0xcb, 0x2f},
		{0xcb, 0x30},	{0xcb, 0x31},	{0xcb, 0x32},	{0xcb, 0x33},
		{0xcb, 0x34},	{0xcb, 0x35},	{0xcb, 0x37},
		{0xcb, 0x38},	{0xcb, 0x39},	{0xcb, 0x3a},	{0xcb, 0x3b},
		{0xcb, 0x3c},	{0xcb, 0x3d},	{0xcb, 0x3f},

		// BIT b, r
		{0xcb, 0x40},	{0xcb, 0x41},	{0xcb, 0x42},	{0xcb, 0x43},
		{0xcb, 0x44},	{0xcb, 0x45},	{0xcb, 0x47},
		{0xcb, 0x48},	{0xcb, 0x49},	{0xcb, 0x4a},	{0xcb, 0x4b},
		{0xcb, 0x4c},	{0xcb, 0x4d},	{0xcb, 0x4f},
		{0xcb, 0x50},	{0xcb, 0x51},	{0xcb, 0x52},	{0xcb, 0x53},
		{0xcb, 0x54},	{0xcb, 0x55},	{0xcb, 0x57},
		{0xcb, 0x58},	{0xcb, 0x59},	{0xcb, 0x5a},	{0xcb, 0x5b},
		{0xcb, 0x5c},	{0xcb, 0x5d},	{0xcb, 0x5f},
		{0xcb, 0x60},	{0xcb, 0x61},	{0xcb, 0x62},	{0xcb, 0x63},
		{0xcb, 0x64},	{0xcb, 0x65},	{0xcb, 0x67},
		{0xcb, 0x68},	{0xcb, 0x69},	{0xcb, 0x6a},	{0xcb, 0x6b},
		{0xcb, 0x6c},	{0xcb, 0x6d},	{0xcb, 0x6f},
		{0xcb, 0x70},	{0xcb, 0x71},	{0xcb, 0x72},	{0xcb, 0x73},
		{0xcb, 0x74},	{0xcb, 0x75},	{0xcb, 0x77},
		{0xcb, 0x78},	{0xcb, 0x79},	{0xcb, 0x7a},	{0xcb, 0x7b},
		{0xcb, 0x7c},	{0xcb, 0x7d},	{0xcb, 0x7f},

		// RES b, r
		{0xcb, 0x80},	{0xcb, 0x81},	{0xcb, 0x82},	{0xcb, 0x83},
		{0xcb, 0x84},	{0xcb, 0x85},	{0xcb, 0x87},
		{0xcb, 0x88},	{0xcb, 0x89},	{0xcb, 0x8a},	{0xcb, 0x8b},
		{0xcb, 0x8c},	{0xcb, 0x8d},	{0xcb, 0x8f},
		{0xcb, 0x90},	{0xcb, 0x91},	{0xcb, 0x92},	{0xcb, 0x93},
		{0xcb, 0x94},	{0xcb, 0x95},	{0xcb, 0x97},
		{0xcb, 0x98},	{0xcb, 0x99},	{0xcb, 0x9a},	{0xcb, 0x9b},
		{0xcb, 0x9c},	{0xcb, 0x9d},	{0xcb, 0x9f},
		{0xcb, 0xa0},	{0xcb, 0xa1},	{0xcb, 0xa2},	{0xcb, 0xa3},
		{0xcb, 0xa4},	{0xcb, 0xa5},	{0xcb, 0xa7},
		{0xcb, 0xa8},	{0xcb, 0xa9},	{0xcb, 0xaa},	{0xcb, 0xab},
		{0xcb, 0xac},	{0xcb, 0xad},	{0xcb, 0xaf},
		{0xcb, 0xb0},	{0xcb, 0xb1},	{0xcb, 0xb2},	{0xcb, 0xb3},
		{0xcb, 0xb4},	{0xcb, 0xb5},	{0xcb, 0xb7},
		{0xcb, 0xb8},	{0xcb, 0xb9},	{0xcb, 0xba},	{0xcb, 0xbb},
		{0xcb, 0xbc},	{0xcb, 0xbd},	{0xcb, 0xbf},

		// SET b, r
		{0xcb, 0xc0},	{0xcb, 0xc1},	{0xcb, 0xc2},	{0xcb, 0xc3},
		{0xcb, 0xc4},	{0xcb, 0xc5},	{0xcb, 0xc7},
		{0xcb, 0xc8},	{0xcb, 0xc9},	{0xcb, 0xca},	{0xcb, 0xcb},
		{0xcb, 0xcc},	{0xcb, 0xcd},	{0xcb, 0xcf},
		{0xcb, 0xc0},	{0xcb, 0xd1},	{0xcb, 0xd2},	{0xcb, 0xd3},
		{0xcb, 0xd4},	{0xcb, 0xd5},	{0xcb, 0xd7},
		{0xcb, 0xd8},	{0xcb, 0xd9},	{0xcb, 0xda},	{0xcb, 0xdb},
		{0xcb, 0xdc},	{0xcb, 0xdd},	{0xcb, 0xdf},
		{0xcb, 0xe0},	{0xcb, 0xe1},	{0xcb, 0xe2},	{0xcb, 0xe3},
		{0xcb, 0xe4},	{0xcb, 0xe5},	{0xcb, 0xe7},
		{0xcb, 0xe8},	{0xcb, 0xe9},	{0xcb, 0xea},	{0xcb, 0xeb},
		{0xcb, 0xec},	{0xcb, 0xed},	{0xcb, 0xef},
		{0xcb, 0xf0},	{0xcb, 0xf1},	{0xcb, 0xf2},	{0xcb, 0xf3},
		{0xcb, 0xf4},	{0xcb, 0xf5},	{0xcb, 0xf7},
		{0xcb, 0xf8},	{0xcb, 0xf9},	{0xcb, 0xfa},	{0xcb, 0xfb},
		{0xcb, 0xfc},	{0xcb, 0xfd},	{0xcb, 0xff},

		// NEG
		{0xed, 0x44},	{0xed, 0x4c},	{0xed, 0x54},	{0xed, 0x5c},
		{0xed, 0x64},	{0xed, 0x6c},	{0xed, 0x74},	{0xed, 0x7c},

		// IM 0/1/2
		{0xed, 0x46},	{0xed, 0x4e},	{0xed, 0x56},	{0xed, 0x5e},
		{0xed, 0x66},	{0xed, 0x6e},	{0xed, 0x66},	{0xed, 0x6e},
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(8);

		[self validate48Contention:{{initial_pc, 4}, {initial_pc+1, 4}} z80:z80];
		[self validatePlus3Contention:{{initial_pc, 4}, {initial_pc+1, 4}} z80:z80];
	}
}

- (void)testAIR {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0x57},	// LD A, I
		{0xed, 0x5f},	// LD A, R
		{0xed, 0x47},	// LD I, A
		{0xed, 0x4f},	// LD R, A
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(9);

		[self validate48Contention:{{initial_pc, 4}, {initial_pc+1, 4}, {initial_ir+1, 1}} z80:z80];
		[self validatePlus3Contention:{{initial_pc, 4}, {initial_pc+1, 5}} z80:z80];
	}
}

- (void)testINCDEC16 {
	for(uint8_t opcode : {
		// INC dd
		0x03, 0x13, 0x23, 0x33,

		// DEC dd
		0x0b, 0x1b, 0x2b, 0x3b,

		// LD SP, HL
		0xf9,
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(6);

		[self validate48Contention:{{initial_pc, 4}, {initial_ir, 1}, {initial_ir, 1}} z80:z80];
		[self validatePlus3Contention:{{initial_pc, 6}} z80:z80];
	}
}

- (void)testADDHLdd {
	for(uint8_t opcode : {
		// ADD hl, dd
		0x09, 0x19, 0x29, 0x39,
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(11);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_ir, 1},
			{initial_ir, 1},
			{initial_ir, 1},
			{initial_ir, 1},
			{initial_ir, 1},
			{initial_ir, 1},
			{initial_ir, 1},
		} z80:z80];
		[self validatePlus3Contention:{{initial_pc, 11}} z80:z80];
	}
}

- (void)testADCSBCHLdd {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// ADC HL, dd
		{0xed, 0x4a},	{0xed, 0x5a},	{0xed, 0x6a},	{0xed, 0x7a},

		// SBC HL, dd
		{0xed, 0x42},	{0xed, 0x52},	{0xed, 0x62},	{0xed, 0x72},
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(15);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_ir+1, 1},
			{initial_ir+1, 1},
			{initial_ir+1, 1},
			{initial_ir+1, 1},
			{initial_ir+1, 1},
			{initial_ir+1, 1},
			{initial_ir+1, 1},
		} z80:z80];
		[self validatePlus3Contention:{{initial_pc, 4}, {initial_pc+1, 11}} z80:z80];
	}
}

- (void)testLDrnALOAn {
	for(uint8_t opcode : {
		// LD r, n
		0x06, 0x0e, 0x16, 0x1e, 0x26, 0x2e, 0x3e,

		0xc6,	// ADD A, n
		0xce,	// ADC A, n
		0xde,	// SBC A, n
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(7);

		[self validate48Contention:{{initial_pc, 4}, {initial_pc+1, 3}} z80:z80];
		[self validatePlus3Contention:{{initial_pc, 4}, {initial_pc+1, 3}} z80:z80];
	}
}

- (void)testLDrind {
	for(uint8_t opcode : {
		// LD r, (dd)
		0x0a, 0x1a,
		0x46, 0x4e, 0x56, 0x5e, 0x66, 0x6e, 0x7e, 0x86,

		// LD (ss), r
		0x02, 0x12,
		0x70, 0x71, 0x72, 0x73, 0x74, 0x75, 0x77,

		// ALO A, (HL)
		0x86, 0x8e, 0x96, 0x9e, 0xa6, 0xae, 0xb6, 0xbe
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(7);

		[self validate48Contention:{{initial_pc, 4}, {initial_bc_de_hl, 3}} z80:z80];
		[self validatePlus3Contention:{{initial_pc, 4}, {initial_bc_de_hl, 3}} z80:z80];
	}
}

- (void)testLDiiPlusn {
	constexpr uint8_t offset = 0x10;
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// LD r, (ii+n)
		{0xdd, 0x46, offset},	{0xdd, 0x4e, offset},
		{0xdd, 0x56, offset},	{0xdd, 0x5e, offset},
		{0xdd, 0x66, offset},	{0xdd, 0x6e, offset},

		{0xfd, 0x46, offset},	{0xfd, 0x4e, offset},
		{0xfd, 0x56, offset},	{0xfd, 0x5e, offset},
		{0xfd, 0x66, offset},	{0xfd, 0x6e, offset},

		// LD (ii+n), r
		{0xdd, 0x70, offset},	{0xdd, 0x71, offset},
		{0xdd, 0x72, offset},	{0xdd, 0x73, offset},
		{0xdd, 0x74, offset},	{0xdd, 0x75, offset},
		{0xdd, 0x77, offset},

		{0xfd, 0x70, offset},	{0xfd, 0x71, offset},
		{0xfd, 0x72, offset},	{0xfd, 0x73, offset},
		{0xfd, 0x74, offset},	{0xfd, 0x75, offset},
		{0xfd, 0x77, offset},

		// ALO A, (ii+n)
		{0xdd, 0x86, offset},	{0xdd, 0x8e, offset},
		{0xdd, 0x96, offset},	{0xdd, 0x9e, offset},
		{0xdd, 0xa6, offset},	{0xdd, 0xae, offset},
		{0xdd, 0xb6, offset},	{0xdd, 0xbe, offset},

		{0xfd, 0x86, offset},	{0xfd, 0x8e, offset},
		{0xfd, 0x96, offset},	{0xfd, 0x9e, offset},
		{0xfd, 0xa6, offset},	{0xfd, 0xae, offset},
		{0xfd, 0xb6, offset},	{0xfd, 0xbe, offset},
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(19);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 3},
			{initial_pc+2, 1},
			{initial_pc+2, 1},
			{initial_pc+2, 1},
			{initial_pc+2, 1},
			{initial_pc+2, 1},
			{initial_ix_iy + offset, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 8},
			{initial_ix_iy + offset, 3},
		} z80:z80];
	}
}

- (void)testBITbhl {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xcb, 0x46},	{0xcb, 0x4e},
		{0xcb, 0x56},	{0xcb, 0x5e},
		{0xcb, 0x66},	{0xcb, 0x6e},
		{0xcb, 0x76},	{0xcb, 0x7e},
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(12);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 3},
			{initial_bc_de_hl, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 4},
		} z80:z80];
	}
}

- (void)testBITbiin {
	constexpr uint8_t offset = 0x10;
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// BIT b, (ix+d)
		{0xdd, 0xcb, offset, 0x46},	{0xdd, 0xcb, offset, 0x4e},
		{0xdd, 0xcb, offset, 0x56},	{0xdd, 0xcb, offset, 0x5e},
		{0xdd, 0xcb, offset, 0x66},	{0xdd, 0xcb, offset, 0x6e},
		{0xdd, 0xcb, offset, 0x76},	{0xdd, 0xcb, offset, 0x7e},

		// BIT b, (iy+d)
		{0xfd, 0xcb, offset, 0x46},	{0xfd, 0xcb, offset, 0x4e},
		{0xfd, 0xcb, offset, 0x56},	{0xfd, 0xcb, offset, 0x5e},
		{0xfd, 0xcb, offset, 0x66},	{0xfd, 0xcb, offset, 0x6e},
		{0xfd, 0xcb, offset, 0x76},	{0xfd, 0xcb, offset, 0x7e},
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(20);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 3},
			{initial_pc+3, 3},
			{initial_pc+3, 1},
			{initial_pc+3, 1},
			{initial_ix_iy + offset, 3},
			{initial_ix_iy + offset, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 3},
			{initial_pc+3, 5},
			{initial_ix_iy + offset, 4},
		} z80:z80];
	}
}

- (void)testLDJPJRnn {
	for(uint8_t opcode : {
		// LD rr, dd
		0x01, 0x11, 0x21, 0x31,

		// JP nn, JP cc, nn
		0xc2, 0xc3, 0xd2, 0xe2, 0xf2,
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(10);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_pc+2, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_pc+2, 3},
		} z80:z80];
	}
}

- (void)testLDindHLn {
	for(uint8_t opcode : {
		// LD (HL), n
		0x36
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(10);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_bc_de_hl, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_bc_de_hl, 3},
		} z80:z80];
	}
}

- (void)testLDiiPlusnn {
	constexpr uint8_t offset = 0x10;
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// LD (ii+n), n
		{0xdd, 0x36, offset},	{0xfd, 0x36, offset},
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(19);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 3},
			{initial_pc+3, 3},
			{initial_pc+3, 1},
			{initial_pc+3, 1},
			{initial_ix_iy + offset, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 3},
			{initial_pc+3, 5},
			{initial_ix_iy + offset, 3},
		} z80:z80];
	}
}

- (void)testLDAind {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0x32, 0xcd, 0xab},	// LD (nn), a
		{0x3a, 0xcd, 0xab},	// LD a, (nn)
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(13);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_pc+2, 3},
			{0xabcd, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_pc+2, 3},
			{0xabcd, 3},
		} z80:z80];
	}
}

- (void)testLDHLind {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0x22, 0xcd, 0xab},	// LD (nn), HL
		{0x2a, 0xcd, 0xab},	// LD HL, (nn)
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(16);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_pc+2, 3},
			{0xabcd, 3},
			{0xabce, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_pc+2, 3},
			{0xabcd, 3},
			{0xabce, 3},
		} z80:z80];
	}
}

- (void)testLDrrind {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0x43, 0xcd, 0xab},	// LD (nn), BC
		{0xed, 0x53, 0xcd, 0xab},	// LD (nn), DE
		{0xed, 0x63, 0xcd, 0xab},	// LD (nn), HL
		{0xed, 0x73, 0xcd, 0xab},	// LD (nn), SP

		{0xed, 0x4b, 0xcd, 0xab},	// LD BC, (nn)
		{0xed, 0x5b, 0xcd, 0xab},	// LD DE, (nn)
		{0xed, 0x6b, 0xcd, 0xab},	// LD HL, (nn)
		{0xed, 0x7b, 0xcd, 0xab},	// LD SP, (nn)
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(20);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 3},
			{initial_pc+3, 3},
			{0xabcd, 3},
			{0xabce, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 3},
			{initial_pc+3, 3},
			{0xabcd, 3},
			{0xabce, 3},
		} z80:z80];
	}
}

- (void)testINCDECHL {
	for(uint8_t opcode : {
		0x34,	// INC (HL)
		0x35,	// DEC (HL)
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(11);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_bc_de_hl, 3},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_bc_de_hl, 4},
			{initial_bc_de_hl, 3},
		} z80:z80];
	}
}

- (void)testSETRESbHLind {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// SET b, (HL)
		{0xcb, 0xc6},	{0xcb, 0xce},	{0xcb, 0xd6},	{0xcb, 0xde},
		{0xcb, 0xe6},	{0xcb, 0xee},	{0xcb, 0xf6},	{0xcb, 0xfe},

		// RES b, (HL)
		{0xcb, 0x86},	{0xcb, 0x8e},	{0xcb, 0x96},	{0xcb, 0x9e},
		{0xcb, 0xa6},	{0xcb, 0xae},	{0xcb, 0xb6},	{0xcb, 0xbe},

		// SRO (HL)
		{0xcb, 0x06},	{0xcb, 0x0e},	{0xcb, 0x16},	{0xcb, 0x1e},
		{0xcb, 0x26},	{0xcb, 0x2e},	{0xcb, 0x36},	{0xcb, 0x3e},
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(15);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 3},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 4},
			{initial_bc_de_hl, 3},
		} z80:z80];
	}
}

- (void)testINCDECiin {
	constexpr uint8_t offset = 0x10;
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// INC (ii+n)
		{0xdd, 0x34, offset},	{0xfd, 0x34, offset},

		// DEC (ii+n)
		{0xdd, 0x35, offset},	{0xfd, 0x35, offset},
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(23);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 3},
			{initial_pc+2, 1},
			{initial_pc+2, 1},
			{initial_pc+2, 1},
			{initial_pc+2, 1},
			{initial_pc+2, 1},
			{initial_ix_iy + offset, 3},
			{initial_ix_iy + offset, 1},
			{initial_ix_iy + offset, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 8},
			{initial_ix_iy + offset, 4},
			{initial_ix_iy + offset, 3},
		} z80:z80];
	}
}

- (void)testSETRESiin {
	constexpr uint8_t offset = 0x10;
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// SET b, (ii+n)
		{0xdd, 0xcb, offset, 0xc6},	{0xdd, 0xcb, offset, 0xce},
		{0xdd, 0xcb, offset, 0xd6},	{0xdd, 0xcb, offset, 0xde},
		{0xdd, 0xcb, offset, 0xe6},	{0xdd, 0xcb, offset, 0xee},
		{0xdd, 0xcb, offset, 0xf6},	{0xdd, 0xcb, offset, 0xfe},

		{0xfd, 0xcb, offset, 0xc6},	{0xfd, 0xcb, offset, 0xce},
		{0xfd, 0xcb, offset, 0xd6},	{0xfd, 0xcb, offset, 0xde},
		{0xfd, 0xcb, offset, 0xe6},	{0xfd, 0xcb, offset, 0xee},
		{0xfd, 0xcb, offset, 0xf6},	{0xfd, 0xcb, offset, 0xfe},

		// RES b, (ii+n)
		{0xdd, 0xcb, offset, 0x86},	{0xdd, 0xcb, offset, 0x8e},
		{0xdd, 0xcb, offset, 0x96},	{0xdd, 0xcb, offset, 0x9e},
		{0xdd, 0xcb, offset, 0xa6},	{0xdd, 0xcb, offset, 0xae},
		{0xdd, 0xcb, offset, 0xb6},	{0xdd, 0xcb, offset, 0xbe},

		{0xfd, 0xcb, offset, 0x86},	{0xfd, 0xcb, offset, 0x8e},
		{0xfd, 0xcb, offset, 0x96},	{0xfd, 0xcb, offset, 0x9e},
		{0xfd, 0xcb, offset, 0xa6},	{0xfd, 0xcb, offset, 0xae},
		{0xfd, 0xcb, offset, 0xb6},	{0xfd, 0xcb, offset, 0xbe},

		// SRO (ii+n)
		{0xdd, 0xcb, offset, 0x06},	{0xdd, 0xcb, offset, 0x0e},
		{0xdd, 0xcb, offset, 0x16},	{0xdd, 0xcb, offset, 0x1e},
		{0xdd, 0xcb, offset, 0x26},	{0xdd, 0xcb, offset, 0x2e},
		{0xdd, 0xcb, offset, 0x36},	{0xdd, 0xcb, offset, 0x3e},

		{0xfd, 0xcb, offset, 0x06},	{0xfd, 0xcb, offset, 0x0e},
		{0xfd, 0xcb, offset, 0x16},	{0xfd, 0xcb, offset, 0x1e},
		{0xfd, 0xcb, offset, 0x26},	{0xfd, 0xcb, offset, 0x2e},
		{0xfd, 0xcb, offset, 0x36},	{0xfd, 0xcb, offset, 0x3e},
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(23);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 3},
			{initial_pc+3, 3},
			{initial_pc+3, 1},
			{initial_pc+3, 1},
			{initial_ix_iy + offset, 3},
			{initial_ix_iy + offset, 1},
			{initial_ix_iy + offset, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_pc+2, 3},
			{initial_pc+3, 5},
			{initial_ix_iy + offset, 4},
			{initial_ix_iy + offset, 3},
		} z80:z80];
	}
}

- (void)testPOPddRET {
	for(uint8_t opcode : {
		// POP dd
		0xc1, 0xd1, 0xe1, 0xf1,

		// RET
		0xc9,
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(10);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_sp, 3},
			{initial_sp+1, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_sp, 3},
			{initial_sp+1, 3},
		} z80:z80];
	}
}

- (void)testRETIN {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// RETN
		{0xed, 0x45},	{0xed, 0x55},	{0xed, 0x5d},
		{0xed, 0x65},	{0xed, 0x6d},	{0xed, 0x75},	{0xed, 0x7d},

		{0xed, 0x4d},	// RETI
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(14);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_sp, 3},
			{initial_sp+1, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_sp, 3},
			{initial_sp+1, 3},
		} z80:z80];
	}
}

- (void)testRETcc {
	for(uint8_t opcode : {
		0xc0,	0xc8,	0xd0,	0xd8,
		0xe0,	0xe8,	0xf0,	0xf8,
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(11);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_ir, 1},
			{initial_sp, 3},
			{initial_sp+1, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 5},
			{initial_sp, 3},
			{initial_sp+1, 3},
		} z80:z80];
	}
}

- (void)testPUSHRST {
	for(uint8_t opcode : {
		// PUSH dd
		0xc5,	0xd5,	0xe5,	0xf5,

		// RST x
		0xc7,	0xcf,	0xd7,	0xdf,	0xe7,	0xef,	0xf7,	0xff,
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(11);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_ir, 1},
			{initial_sp-1, 3},
			{initial_sp-2, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 5},
			{initial_sp-1, 3},
			{initial_sp-2, 3},
		} z80:z80];
	}
}

- (void)testCALL {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// CALL cc
		{0xc4, 0x00, 0x00},	{0xcc, 0x00, 0x00},
		{0xd4, 0x00, 0x00},	{0xdc, 0x00, 0x00},
		{0xe4, 0x00, 0x00},	{0xec, 0x00, 0x00},
		{0xf4, 0x00, 0x00},	{0xfc, 0x00, 0x00},

		{0xcd, 0x00, 0x00},	// CALL
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(17);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_pc+2, 3},
			{initial_pc+2, 1},
			{initial_sp-1, 3},
			{initial_sp-2, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_pc+2, 4},
			{initial_sp-1, 3},
			{initial_sp-2, 3},
		} z80:z80];
	}
}

- (void)testJR {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// JR cc
		{0x20, 0x00},	{0x28, 0x00},
		{0x30, 0x00},	{0x38, 0x00},

		{0x18, 0x00},	// JR
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(12);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{initial_pc+1, 1},
			{initial_pc+1, 1},
			{initial_pc+1, 1},
			{initial_pc+1, 1},
			{initial_pc+1, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 8},
		} z80:z80];
	}
}

- (void)testDJNZ {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0x10, 0x00}
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(13);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_ir, 1},
			{initial_pc+1, 3},
			{initial_pc+1, 1},
			{initial_pc+1, 1},
			{initial_pc+1, 1},
			{initial_pc+1, 1},
			{initial_pc+1, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 5},
			{initial_pc+1, 8},
		} z80:z80];
	}
}

- (void)testRLDRRD {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0x6f},	// RLD
		{0xed, 0x67},	// RRD
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(18);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 3},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 7},
			{initial_bc_de_hl, 3},
		} z80:z80];
	}
}

- (void)testINOUTn {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xdb, 0xef},	// IN A, (n)
		{0xd3, 0xef},	// OUT (n), A
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(11);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{0x80ef, 4, true},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 3},
			{0x80ef, 4, true},
		} z80:z80];
	}
}

- (void)testINOUTC {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		// IN r, (C)
		{0xed, 0x40},	{0xed, 0x48},	{0xed, 0x50},	{0xed, 0x58},
		{0xed, 0x60},	{0xed, 0x68},	{0xed, 0x70},	{0xed, 0x78},

		// OUT r, (C)
		{0xed, 0x41},	{0xed, 0x49},	{0xed, 0x51},	{0xed, 0x59},
		{0xed, 0x61},	{0xed, 0x69},	{0xed, 0x71},	{0xed, 0x79},
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(12);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 4, true},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 4, true},
		} z80:z80];
	}
}

- (void)testEXSPHL {
	for(uint8_t opcode : {
		0xe3,
	}) {
		const std::initializer_list<uint8_t> opcodes = {opcode};
		CapturingZ80 z80(opcodes);
		z80.run_for(19);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_sp, 3},
			{initial_sp+1, 3},
			{initial_sp+1, 1},
			{initial_sp+1, 3},
			{initial_sp, 3},
			{initial_sp, 1},
			{initial_sp, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_sp, 3},
			{initial_sp+1, 4},
			{initial_sp+1, 3},
			{initial_sp, 5},
		} z80:z80];
	}
}

- (void)testLDILDD {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0xa0},	// LDI
		{0xed, 0xa8},	// LDD
	}) {
		CapturingZ80 z80(sequence);

		// Establish a distinct value for DE.
		constexpr uint16_t de = 0x9876;
		z80.set_de(de);

		z80.run_for(16);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 3},
			{de, 3},
			{de, 1},
			{de, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 3},
			{de, 5},
		} z80:z80];
	}
}

- (void)testLDIRLDDR {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0xb0},	// LDIR
		{0xed, 0xb8},	// LDDR
	}) {
		CapturingZ80 z80(sequence);

		// Establish a distinct value for DE.
		constexpr uint16_t de = 0x9876;
		z80.set_de(de);

		z80.run_for(21);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 3},
			{de, 3},
			{de, 1},
			{de, 1},
			{de, 1},
			{de, 1},
			{de, 1},
			{de, 1},
			{de, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 3},
			{de, 10},
		} z80:z80];
	}
}

- (void)testCPICPD {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0xa1},	// CPI
		{0xed, 0xa9},	// CPD
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(16);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 3},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 8},
		} z80:z80];
	}
}

- (void)testCPIRCPDR {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0xb1},	// CPIR
		{0xed, 0xb9},	// CPDR
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(21);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 3},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_bc_de_hl, 13},
		} z80:z80];
	}
}

- (void)testINIIND {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0xa2},	// INI
		{0xed, 0xaa},	// IND
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(16);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_ir+1, 1},
			{initial_bc_de_hl, 4, true},
			{initial_bc_de_hl, 3},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 5},
			{initial_bc_de_hl, 4, true},
			{initial_bc_de_hl, 3},
		} z80:z80];
	}
}

- (void)testINIRINDR {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0xb2},	// INIR
		{0xed, 0xba},	// INDR
	}) {
		CapturingZ80 z80(sequence);
		z80.run_for(21);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_ir+1, 1},
			{initial_bc_de_hl, 4, true},
			{initial_bc_de_hl, 3},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
			{initial_bc_de_hl, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 5},
			{initial_bc_de_hl, 4, true},
			{initial_bc_de_hl, 8},
		} z80:z80];
	}
}

- (void)testOUTIOUTD {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0xa3},	// OUTI
		{0xed, 0xab},	// OUTD
	}) {
		CapturingZ80 z80(sequence);

		// Establish a distinct value for BC.
		constexpr uint16_t bc = 0x9876;
		z80.set_bc(bc);

		z80.run_for(16);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_ir+1, 1},
			{initial_bc_de_hl, 3},
			{bc - 256, 4, true},			// B is decremented before the output.
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 5},
			{initial_bc_de_hl, 3},
			{bc - 256, 4, true},
		} z80:z80];
	}
}

- (void)testOTIROTDR {
	for(const auto &sequence : std::vector<std::vector<uint8_t>>{
		{0xed, 0xb3},	// OTIR
		{0xed, 0xbb},	// OTDR
	}) {
		CapturingZ80 z80(sequence);

		// Establish a distinct value for BC.
		constexpr uint16_t bc = 0x9876;
		z80.set_bc(bc);

		z80.run_for(21);

		[self validate48Contention:{
			{initial_pc, 4},
			{initial_pc+1, 4},
			{initial_ir+1, 1},
			{initial_bc_de_hl, 3},
			{bc - 256, 4, true},			// B is decremented before the output.
			{bc - 256, 1},
			{bc - 256, 1},
			{bc - 256, 1},
			{bc - 256, 1},
			{bc - 256, 1},
		} z80:z80];
		[self validatePlus3Contention:{
			{initial_pc, 4},
			{initial_pc+1, 5},
			{initial_bc_de_hl, 3},
			{bc - 256, 9, false},			// Abuse of the is_io flag here for the purposes of testing;
											// the +3 doesn't contend output so this doesn't matter.
		} z80:z80];
	}
}

@end
