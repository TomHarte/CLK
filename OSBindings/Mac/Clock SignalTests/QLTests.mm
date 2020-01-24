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

#include <iostream>
#include <fstream>

#include <zlib.h>

#include "68000.hpp"
#include "Comparative68000.hpp"
#include "CSROMFetcher.hpp"

class QL: public ComparativeBusHandler {
	public:
		QL(const std::vector<uint8_t> &rom, const char *trace_name) : ComparativeBusHandler(trace_name), m68000_(*this) {
			assert(!(rom.size() & 1));
			rom_.resize(rom.size() / 2);

			for(size_t c = 0; c < rom_.size(); ++c) {
				rom_[c] = (rom[c << 1] << 8) | rom[(c << 1) + 1];
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

			// QL memory map: ROM is in the lowest area; RAM is from 0x20000.
			const bool is_rom = word_address < rom_.size();
			const bool is_ram = word_address >= 0x10000 && word_address < 0x10000+ram_.size();
			const bool is_peripheral = !is_ram && !is_rom;

			uint16_t *const base = is_rom ? rom_.data() : ram_.data();
			if(is_rom) {
				word_address %= rom_.size();
			}
			if(is_ram) {
				word_address %= ram_.size();
			}

			using Microcycle = CPU::MC68000::Microcycle;
			if(cycle.data_select_active()) {
				uint16_t peripheral_result = 0xffff;

				switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
					default: break;

					case Microcycle::SelectWord | Microcycle::Read:
						cycle.value->full = is_peripheral ? peripheral_result : base[word_address];
					break;
					case Microcycle::SelectByte | Microcycle::Read:
						cycle.value->halves.low = (is_peripheral ? peripheral_result : base[word_address]) >> cycle.byte_shift();
					break;
					case Microcycle::SelectWord:
						assert(!(is_rom && !is_peripheral));
						if(!is_peripheral) base[word_address] = cycle.value->full;
					break;
					case Microcycle::SelectByte:
						assert(!(is_rom && !is_peripheral));
						if(!is_peripheral) base[word_address] = (cycle.value->halves.low << cycle.byte_shift()) | (base[word_address] & (0xffff ^ cycle.byte_mask()));
					break;
				}
			}

			return HalfCycles(0);
		}

	private:
		CPU::MC68000::Processor<QL, true, true> m68000_;

		std::vector<uint16_t> rom_;
		std::array<uint16_t, 64*1024> ram_;
};

@interface QLTests : XCTestCase
@end

@implementation QLTests {
	std::unique_ptr<QL> _machine;
}

/*!
	Tests the progression of Clock Signal's 68000 through the Sinclair QL's ROM against a known-good trace.
*/
- (void)testStartup {
	const std::vector<ROMMachine::ROM> rom_names = {{"SinclairQL", "", "js.rom", 0, 0 }};
	const auto roms = CSROMFetcher()(rom_names);
	NSString *const traceLocation = [[NSBundle bundleForClass:[self class]] pathForResource:@"qltrace" ofType:@".txt.gz"];
	_machine = std::make_unique<QL>(*roms[0], traceLocation.UTF8String);

	// This is how many cycles it takes to exhaust the supplied trace file.
	_machine->run_for(HalfCycles(23923180));
}

@end
