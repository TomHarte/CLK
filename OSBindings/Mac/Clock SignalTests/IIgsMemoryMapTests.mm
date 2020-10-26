//
//  IIgsMemoryMapTests.mm
//  Clock SignalTests
//
//  Created by Thomas Harte on 25/10/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Machines/Apple/AppleIIgs/MemoryMap.hpp"

namespace {
	using MemoryMap = Apple::IIgs::MemoryMap;
}

@interface IIgsMemoryMapTests : XCTestCase
@end

@implementation IIgsMemoryMapTests {
	MemoryMap _memoryMap;
	std::vector<uint8_t> _ram;
	std::vector<uint8_t> _rom;
}

- (void)setUp {
	_ram.resize((128 + 8 * 1024) * 1024);
	_rom.resize(256 * 1024);
	_memoryMap.set_storage(_ram, _rom);
}

- (void)testHigherRAM {
	// Fill memory via the map.
	for(int address = 0x020000; address < 0x800000; ++address) {
		const auto &region = MemoryMapRegion(_memoryMap, address);
		const uint8_t value = uint8_t(address ^ (address >> 8));
		MemoryMapWrite(_memoryMap, region, address, &value);
	}

	// Test by direct access.
	for(int address = 0x020000; address < 0x800000; ++address) {
		const uint8_t value = uint8_t(address ^ (address >> 8));
		XCTAssertEqual(_ram[address], value);
	}
}

- (void)testROMReadonly {
	_rom[0] = 0xc0;

	// Test that ROM can be read in the correct location.
	const uint32_t address = 0xfc0000;
	const auto &region = MemoryMapRegion(_memoryMap, address);
	uint8_t value;
	MemoryMapRead(region, address, &value);

	XCTAssertEqual(value, 0xc0);

	// Try writing to it, and check that nothing happened.
	value = 0xfc;
	MemoryMapWrite(_memoryMap, region, address, &value);

	XCTAssertEqual(_rom[0], 0xc0);
}

@end
