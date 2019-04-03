//
//  EmuTOSTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 10/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <array>
#include <cassert>

#include "68000.hpp"
#include "CSROMFetcher.hpp"

class EmuTOS: public CPU::MC68000::BusHandler {
	public:
		EmuTOS(const std::vector<uint8_t> &emuTOS) : m68000_(*this) {
			assert(!(emuTOS.size() & 1));
			emuTOS_.resize(emuTOS.size() / 2);

			for(size_t c = 0; c < emuTOS_.size(); ++c) {
				emuTOS_[c] = (emuTOS[c << 1] << 8) | emuTOS[(c << 1) + 1];
			}
		}

		void run_for(HalfCycles cycles) {
			m68000_.run_for(cycles);
		}

		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
			uint32_t word_address = cycle.word_address();

			// As much about the Atari ST's memory map as is relevant here: the ROM begins
			// at 0xfc0000, and the first eight bytes are mirrored to the first four memory
			// addresses in order for /RESET to work properly. RAM otherwise fills the first
			// 512kb of the address space. Trying to write to ROM raises a bus error.

			const bool is_rom = word_address > (0xfc0000 >> 1) || word_address < 4;
			uint16_t *const base = is_rom ? emuTOS_.data() : ram_.data();
			if(is_rom) {
				word_address &= 0xffff;
			} else {
				word_address &= 0x3ffff;
			}

			using Microcycle = CPU::MC68000::Microcycle;
			if(cycle.data_select_active()) {
				switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
					default: break;

					case Microcycle::SelectWord | Microcycle::Read:
						cycle.value->full = base[word_address];
					break;
					case Microcycle::SelectByte | Microcycle::Read:
						cycle.value->halves.low = base[word_address] >> cycle.byte_shift();
					break;
					case Microcycle::SelectWord:
						base[word_address] = cycle.value->full;
					break;
					case Microcycle::SelectByte:
						base[word_address] = (cycle.value->full & cycle.byte_mask()) | (base[word_address] & (0xffff ^ cycle.byte_mask()));
					break;
				}
			}

			return HalfCycles(0);
		}

	private:
		CPU::MC68000::Processor<EmuTOS, true> m68000_;

		std::vector<uint16_t> emuTOS_;
		std::array<uint16_t, 256*1024> ram_;
};

@interface EmuTOSTests : XCTestCase
@end

@implementation EmuTOSTests {
	std::unique_ptr<EmuTOS> _machine;
}

- (void)setUp {
    // Put setup code here. This method is called before the invocation of each test method in the class.
    const auto roms = CSROMFetcher()("AtariST", {"tos100.img"});
    _machine.reset(new EmuTOS(*roms[0]));
}

- (void)tearDown {
    // Put teardown code here. This method is called after the invocation of each test method in the class.
}

- (void)testStartup {
    // This is an example of a functional test case.
    // Use XCTAssert and related functions to verify your tests produce the correct results.
    _machine->run_for(HalfCycles(4000));
}

@end
