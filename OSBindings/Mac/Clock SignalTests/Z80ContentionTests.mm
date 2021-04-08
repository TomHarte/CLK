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

- (void)testNOP {
	CapturingZ80 z80({0x00});
	z80.run_for(4);

	[self validate48Contention:{{initial_pc, 4}} z80:z80];
	[self validatePlus3Contention:{{initial_pc, 4}} z80:z80];
}

@end
