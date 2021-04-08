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

static constexpr uint16_t initial_pc = 0;

struct CapturingZ80: public CPU::Z80::BusHandler {

	CapturingZ80(const std::initializer_list<uint8_t> &code) : z80_(*this) {
		// Take a copy of the code.
		std::copy(code.begin(), code.end(), ram_);

		// Skip the three cycles the Z80 spends on a reset, and
		// purge them from the record.
		run_for(3);
		bus_records_.clear();

		// Set the refresh address to the EE page.
		z80_.set_value_of_register(CPU::Z80::Register::I, 0xe0);
	}

	void run_for(int cycles) {
		z80_.run_for(HalfCycles(Cycles(cycles)));
	}

	/// A record of the state of the address bus, MREQ, IOREQ and RFSH lines,
	/// upon every clock transition.
	struct BusRecord {
		uint16_t address = 0xffff;
		bool mreq = false, ioreq = false, refresh = false;
	};

	HalfCycles perform_machine_cycle(const CPU::Z80::PartialMachineCycle &cycle) {
		// Log the activity.
		const uint8_t* const bus_state = cycle.bus_state();
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

		// Provide only reads.
		if(
			cycle.operation == CPU::Z80::PartialMachineCycle::Read ||
			cycle.operation == CPU::Z80::PartialMachineCycle::ReadOpcode
		) {
			*cycle.value = ram_[*cycle.address];
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

	private:
		CPU::Z80::Processor<CapturingZ80, false, false> z80_;
		uint8_t ram_[65536];

		std::vector<BusRecord> bus_records_;
};

}

@interface Z80ContentionTests : XCTestCase
@end

/*!
	Tests the Z80's MREQ, IOREQ and address outputs for correlation to those
	observed by ZX Spectrum users in the software-side documentation of
	contended memory timings.
*/
@implementation Z80ContentionTests {
}

struct ContentionCheck {
	uint16_t address;
	int length;
};

/*!
	Checks that the accumulated bus activity in @c z80 matches the expectations given in @c contentions if
	processed by a Sinclair 48k or 128k ULA.
*/
- (void)validate48Contention:(const std::initializer_list<ContentionCheck> &)contentions z80:(const CapturingZ80 &)z80 {
	// 48[/128]k contention logic: triggered on address alone, _unless_
	// MREQ is also active.
	//
	// I think the source I'm using also implicitly assumes that refresh
	// addresses are outside of the contended area, and doesn't check them.
	// So unlike the actual ULA I'm also ignoring any address while refresh
	// is asserted.
	int count = -1;
	uint16_t address = 0;
	auto contention = contentions.begin();

	const auto bus_records = z80.cycle_records();
	for(const auto &record: bus_records) {
		++count;

		if(
			!count ||								// i.e. is at start.
			(&record == &bus_records.back()) ||		// i.e. is at end.
			!(record.mreq || record.refresh)		// i.e. beginning of a new contention.
		) {
			if(count) {
				XCTAssertNotEqual(contention, contentions.end());
				XCTAssertEqual(contention->address, address);
				XCTAssertEqual(contention->length, count);
				++contention;
			}

			count = 1;
			address = record.address;
		}
	}

	XCTAssertEqual(contention, contentions.end());
}

/*!
	Checks that the accumulated bus activity in @c z80 matches the expectations given in @c contentions if
	processed by an Amstrad gate array.
*/
- (void)validatePlus3Contention:(const std::initializer_list<ContentionCheck> &)contentions z80:(const CapturingZ80 &)z80 {
	// +3 contention logic: triggered by the leading edge of MREQ, sans refresh.
	int count = -1;
	uint16_t address = 0;
	auto contention = contentions.begin();

	const auto bus_records = z80.bus_records();

	for(size_t c = 0; c < bus_records.size(); c += 2) {
		const bool is_leading_edge = !bus_records[c].mreq && bus_records[c+1].mreq && !bus_records[c].refresh;

		++count;
		if(
			!count ||								// i.e. is at start.
			(c == bus_records.size() - 2) ||		// i.e. is at end.
			is_leading_edge							// i.e. beginning of a new contention.
		) {
			if(count) {
				XCTAssertNotEqual(contention, contentions.end());
				XCTAssertEqual(contention->address, address);
				XCTAssertEqual(contention->length, count);
				++contention;
			}

			count = 1;
			address = bus_records[c].address;
		}
	}

	XCTAssertEqual(contention, contentions.end());
}

// MARK: - Opcode tests.

- (void)testSimpleSingleBytes {
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
		CapturingZ80 z80({opcode});
		z80.run_for(4);

		[self validate48Contention:{{initial_pc, 4}} z80:z80];
		[self validatePlus3Contention:{{initial_pc, 4}} z80:z80];
	}
}

@end
