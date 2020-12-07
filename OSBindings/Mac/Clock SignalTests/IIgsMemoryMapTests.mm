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

- (void)write:(uint8_t)value address:(uint32_t)address {
	const auto &region = MemoryMapRegion(_memoryMap, address);
	MemoryMapWrite(_memoryMap, region, address, &value);
}

- (uint8_t)readAddress:(uint32_t)address {
	const auto &region = MemoryMapRegion(_memoryMap, address);
	uint8_t value;
	MemoryMapRead(region, address, &value);
	return value;
}

- (void)testHigherRAM {
	// Fill memory via the map.
	for(int address = 0x02'0000; address < 0x80'0000; ++address) {
		const uint8_t value = uint8_t(address ^ (address >> 8));
		[self write:value address:address];
	}

	// Test by direct access.
	for(int address = 0x02'0000; address < 0x80'0000; ++address) {
		const uint8_t value = uint8_t(address ^ (address >> 8));
		XCTAssertEqual([self readAddress:address], value);
	}
}

- (void)testROMIsReadonly {
	_rom[0] = 0xc0;

	// Test that ROM can be read in the correct location.
	XCTAssertEqual([self readAddress:0xfc'0000], 0xc0);

	// Try writing to it, and check that nothing happened.
	[self write:0xfc address:0xfc'0000];
	XCTAssertEqual(_rom[0], 0xc0);
}

/// Tests that the same portion of ROM is visible in banks $00, $01, $e0 and $e1.
- (void)testROMVisibility {
	_rom.back() = 0xa8;
	auto test_bank = [self](uint32_t bank) {
		const uint32_t address = bank | 0xffff;
		XCTAssertEqual([self readAddress:address], 0xa8);
	};

	test_bank(0x00'0000);
	test_bank(0x01'0000);
	test_bank(0xe0'0000);
	test_bank(0xe1'0000);
}

/// Tests that writes to $00:$0400 and to $01:$0400 are subsequently visible at $e0:$0400 and $e1:$0400.
- (void)testShadowing {
	[self write:0xab address:0x00'0400];
	[self write:0xcd address:0x01'0400];
	XCTAssertEqual([self readAddress:0xe0'0400], 0xab);
	XCTAssertEqual([self readAddress:0xe1'0400], 0xcd);
}

/// Tests that a write to bank $00 which via the auxiliary switches is redirected to bank $01 is then
/// mirrored to $e1.
- (void)testAuxiliaryShadowing {
	// Select the alternate text page 1.
	_memoryMap.access(0xc001, false);	// Set 80STORE.
	_memoryMap.access(0xc055, false);	// Set PAGE2.
										// These two things together should enable auxiliary memory for text page 1.
										// No, really.

	// Enable shadowing of text page 1.
	_memoryMap.set_shadow_register(0x00);

	// Establish a different value in bank $e1, then write
	// to bank $00 and check bank $e1.
	[self write: 0xcb address:0xe1'0400];
	[self write: 0xde address:0x00'0400];

	XCTAssertEqual([self readAddress:0xe1'0400], 0xde);
}

@end
