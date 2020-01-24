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

//#define LOG_TRACE

#include "68000.hpp"
#include "Comparative68000.hpp"
#include "CSROMFetcher.hpp"

class EmuTOS: public ComparativeBusHandler {
	public:
		EmuTOS(const std::vector<uint8_t> &emuTOS, const char *trace_name) : ComparativeBusHandler(trace_name), m68000_(*this) {
			assert(!(emuTOS.size() & 1));
			emuTOS_.resize(emuTOS.size() / 2);

			for(size_t c = 0; c < emuTOS_.size(); ++c) {
				emuTOS_[c] = (emuTOS[c << 1] << 8) | emuTOS[(c << 1) + 1];
			}
		}

		void run_for(HalfCycles cycles) {
			m68000_.run_for(cycles);
		}

		CPU::MC68000::ProcessorState get_state() final {
			return m68000_.get_state();
		}

		HalfCycles perform_bus_operation(const CPU::MC68000::Microcycle &cycle, int is_supervisor) {
			const uint32_t address = cycle.word_address();
			uint32_t word_address = address;

			// As much about the Atari ST's memory map as is relevant here: the ROM begins
			// at 0xfc0000, and the first eight bytes are mirrored to the first four memory
			// addresses in order for /RESET to work properly. RAM otherwise fills the first
			// 512kb of the address space. Trying to write to ROM raises a bus error.

			const bool is_rom = (word_address >= (0xfc0000 >> 1) && word_address < (0xff0000 >> 1)) || word_address < 4;
			const bool is_ram = word_address < ram_.size();
			const bool is_peripheral = !is_rom && !is_ram;

			uint16_t *const base = is_rom ? emuTOS_.data() : ram_.data();
			if(is_rom) {
				word_address %= emuTOS_.size();
			} else {
				word_address %= ram_.size();
			}

			using Microcycle = CPU::MC68000::Microcycle;
			if(cycle.data_select_active()) {
				uint16_t peripheral_result = 0xffff;
				if(is_peripheral) {
					switch(address & 0x7ff) {
						// A hard-coded value for TIMER B.
						case (0xa21 >> 1):
							peripheral_result = 0x00000001;
						break;
					}
				}

				switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
					default: break;

					case Microcycle::SelectWord | Microcycle::Read:
						cycle.value->full = is_peripheral ? peripheral_result : base[word_address];
					break;
					case Microcycle::SelectByte | Microcycle::Read:
						cycle.value->halves.low = (is_peripheral ? peripheral_result : base[word_address]) >> cycle.byte_shift();
					break;
					case Microcycle::SelectWord:
						base[word_address] = cycle.value->full;
					break;
					case Microcycle::SelectByte:
						base[word_address] = (cycle.value->halves.low << cycle.byte_shift()) | (base[word_address] & (0xffff ^ cycle.byte_mask()));
					break;
				}
			}

			return HalfCycles(0);
		}

	private:
		CPU::MC68000::Processor<EmuTOS, true, true> m68000_;

		std::vector<uint16_t> emuTOS_;
		std::array<uint16_t, 256*1024> ram_;
};

@interface EmuTOSTests : XCTestCase
@end

@implementation EmuTOSTests {
	std::unique_ptr<EmuTOS> _machine;
}

- (void)testImage:(NSString *)image trace:(NSString *)trace length:(int)length {
	const std::vector<ROMMachine::ROM> rom_names = {{"AtariST", "", image.UTF8String, 0, 0 }};
    const auto roms = CSROMFetcher()(rom_names);
	NSString *const traceLocation = [[NSBundle bundleForClass:[self class]] pathForResource:trace ofType:@"trace.txt.gz"];
    _machine = std::make_unique<EmuTOS>(*roms[0], traceLocation.UTF8String);
    _machine->run_for(HalfCycles(length));
}

- (void)testEmuTOSStartup {
	[self testImage:@"etos192uk.img" trace:@"etos192uk" length:313490];
    // TODO: assert that machine is now STOPped.
}

- (void)testTOSStartup {
	[self testImage:@"tos100.img" trace:@"tos100" length:54011091];
}

@end
