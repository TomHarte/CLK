//
//  68000Tests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 13/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <array>
#include <cassert>

#include "68000.hpp"

/*!
	Provides a 68000 with 64kb of RAM in its low address space;
	/RESET will put the supervisor stack pointer at 0xFFFF and
	begin execution at 0x0400.
*/
class RAM68000: public CPU::MC68000::BusHandler {
	public:
		RAM68000() : m68000_(*this) {
			ram_.resize(32768);

			// Setup the /RESET vector.
			ram_[0] = 0;
			ram_[1] = 0xffff;
			ram_[2] = 0;
			ram_[3] = 0x0400;
		}

		void set_program(const std::vector<uint16_t> &program) {
			memcpy(&ram_[512], program.data(), program.size() * sizeof(uint16_t));
		}

		void run_for(HalfCycles cycles) {
			m68000_.run_for(cycles);
		}

		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
			const uint32_t word_address = cycle.word_address();

			using Microcycle = CPU::MC68000::Microcycle;
			if(cycle.data_select_active()) {
				switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
					default: break;

					case Microcycle::SelectWord | Microcycle::Read:
						cycle.value->full = ram_[word_address];
					break;
					case Microcycle::SelectByte | Microcycle::Read:
						cycle.value->halves.low = ram_[word_address] >> cycle.byte_shift();
					break;
					case Microcycle::SelectWord:
						ram_[word_address] = cycle.value->full;
					break;
					case Microcycle::SelectByte:
						ram_[word_address] = (cycle.value->full & cycle.byte_mask()) | (ram_[word_address] & (0xffff ^ cycle.byte_mask()));
					break;
				}
			}

			return HalfCycles(0);
		}

		CPU::MC68000::Processor<RAM68000, true>::State get_processor_state() {
			return m68000_.get_state();
		}

		void set_processor_state(const CPU::MC68000::Processor<RAM68000, true>::State &state) {
			m68000_.set_state(state);
		}

	private:
		CPU::MC68000::Processor<RAM68000, true> m68000_;
		std::vector<uint16_t> ram_;
};

@interface M68000Tests : XCTestCase
@end

@implementation M68000Tests {
	std::unique_ptr<RAM68000> _machine;
}

- (void)setUp {
    _machine.reset(new RAM68000());
}

- (void)tearDown {
	_machine.reset();
}

- (void)testABCD {
	for(int d = 0; d < 100; ++d) {
		_machine.reset(new RAM68000());
		_machine->set_program({
			0xc100		// ABCD D0, D0
		});

		auto state = _machine->get_processor_state();
		const uint8_t bcd_d = ((d / 10) * 16) + (d % 10);
		state.data[0] = bcd_d;
		_machine->set_processor_state(state);

		_machine->run_for(Cycles(40 + 6));

		state = _machine->get_processor_state();
		const uint8_t double_d = (d * 2) % 100;
		const uint8_t bcd_double_d = ((double_d / 10) * 16) + (double_d % 10);
		XCTAssert(state.data[0] == bcd_double_d, "%02x + %02x = %02x; should equal %02x", bcd_d, bcd_d, state.data[0], bcd_double_d);
	}
}

- (void)testSBCD {
	_machine->set_program({
		0x8100		// SBCD D0, D1
	});
    _machine->run_for(HalfCycles(400));
}

- (void)testMOVE {
	_machine->set_program({
		0x303c, 0xfb2e,		// MOVE #fb2e, D0
		0x3200,				// MOVE D0, D1

		0x3040,				// MOVEA D0, A0
		0x3278, 0x0400,		// MOVEA.w (0x0400), A1

		0x387c, 0x0400,		// MOVE #$400, A4
		0x2414,				// MOVE.l (A4), D2
	});

	// Perform RESET.
	_machine->run_for(Cycles(38));
	auto state = _machine->get_processor_state();
	XCTAssert(state.data[0] == 0);

	// Perform MOVE #fb2e, D0
	_machine->run_for(Cycles(8));
	state = _machine->get_processor_state();
	XCTAssert(state.data[0] == 0xfb2e);

	// Perform MOVE D0, D1
	_machine->run_for(Cycles(4));
	state = _machine->get_processor_state();
	XCTAssert(state.data[1] == 0xfb2e);

	// Perform MOVEA D0, A0
	_machine->run_for(Cycles(4));
	state = _machine->get_processor_state();
	XCTAssert(state.address[0] == 0xfffffb2e, "A0 was %08x instead of 0xfffffb2e", state.address[0]);

	// Perform MOVEA.w (0x1000), A1
	_machine->run_for(Cycles(13));
	state = _machine->get_processor_state();
	XCTAssert(state.address[1] == 0x0000303c, "A1 was %08x instead of 0x0000303c", state.address[1]);

	// Perform MOVE #$400, A4, MOVE.l (A4), D2
	_machine->run_for(Cycles(20));
	state = _machine->get_processor_state();
	XCTAssert(state.address[4] == 0x0400, "A4 was %08x instead of 0x00000400", state.address[4]);
	XCTAssert(state.data[2] == 0x303cfb2e, "D2 was %08x instead of 0x303cfb2e", state.data[2]);
}

@end
