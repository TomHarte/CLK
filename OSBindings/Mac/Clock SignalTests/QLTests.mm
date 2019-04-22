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

#include "68000.hpp"
#include "CSROMFetcher.hpp"

class QL: public CPU::MC68000::BusHandler {
	public:
		QL(const std::vector<uint8_t> &rom) : m68000_(*this) {
			assert(!(rom.size() & 1));
			rom_.resize(rom.size() / 2);

			for(size_t c = 0; c < rom_.size(); ++c) {
				rom_[c] = (rom[c << 1] << 8) | rom[(c << 1) + 1];
			}
		}

		void run_for(HalfCycles cycles) {
			m68000_.run_for(cycles);
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
/*				if(is_peripheral) {
					switch(address & 0x7ff) {
						// A hard-coded value for TIMER B.
						case (0xa21 >> 1):
							peripheral_result = 0x00000001;
						break;
					}
					printf("Peripheral: %c %08x", (cycle.operation & Microcycle::Read) ? 'r' : 'w', *cycle.address);
					if(!(cycle.operation & Microcycle::Read)) {
						if(cycle.operation & Microcycle::SelectByte)
							printf(" %02x", cycle.value->halves.low);
						else
							printf(" %04x", cycle.value->full);
					}
					printf("\n");
				}*/

				switch(cycle.operation & (Microcycle::SelectWord | Microcycle::SelectByte | Microcycle::Read)) {
					default: break;

					case Microcycle::SelectWord | Microcycle::Read:
						cycle.value->full = is_peripheral ? peripheral_result : base[word_address];
//						if(!(cycle.operation & Microcycle::IsProgram)) printf("[%08x -> %04x] ", *cycle.address, cycle.value->full);
					break;
					case Microcycle::SelectByte | Microcycle::Read:
						cycle.value->halves.low = (is_peripheral ? peripheral_result : base[word_address]) >> cycle.byte_shift();
//						if(!(cycle.operation & Microcycle::IsProgram)) printf("[%08x -> %02x] ", *cycle.address, cycle.value->halves.low);
					break;
					case Microcycle::SelectWord:
						assert(!(is_rom && !is_peripheral));
//						if(!(cycle.operation & Microcycle::IsProgram)) printf("{%04x -> %08x} ", cycle.value->full, *cycle.address);
						if(!is_peripheral) base[word_address] = cycle.value->full;
					break;
					case Microcycle::SelectByte:
						assert(!(is_rom && !is_peripheral));
//						if(!(cycle.operation & Microcycle::IsProgram)) printf("{%02x -> %08x} ", cycle.value->halves.low, *cycle.address);
						if(!is_peripheral) base[word_address] = (cycle.value->halves.low << cycle.byte_shift()) | (base[word_address] & (0xffff ^ cycle.byte_mask()));
					break;
				}
			}

			return HalfCycles(0);
		}

	private:
		CPU::MC68000::Processor<QL, true> m68000_;

		std::vector<uint16_t> rom_;
		std::array<uint16_t, 64*1024> ram_;
};

@interface QLTests : XCTestCase
@end

@implementation QLTests {
	std::unique_ptr<QL> _machine;
}

std::streambuf *coutbuf;

- (void)setUp {
    const auto roms = CSROMFetcher()("SinclairQL", {"js.rom"});
    _machine.reset(new QL(*roms[0]));
}

- (void)testStartup {
    // This is an example of a functional test case.
    // Use XCTAssert and related functions to verify your tests produce the correct results.
    _machine->run_for(HalfCycles(40000000));
}

@end

