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
	_machine->set_program({
		0xc100		// ABCD D0, D0
	});
    _machine->run_for(HalfCycles(400));
}

- (void)testSBCD {
	_machine->set_program({
		0x8100		// SBCD D0, D1
	});
    _machine->run_for(HalfCycles(400));
}

@end
