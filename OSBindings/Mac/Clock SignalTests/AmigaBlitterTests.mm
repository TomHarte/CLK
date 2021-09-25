//
//  AmigaBlitterTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 25/09/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "Blitter.hpp"

namespace Amiga {
/// An empty stub to satisfy Amiga::Blitter's inheritance from Amiga::DMADevice;
struct Chipset {};
};

@interface AmigaBlitterTests: XCTestCase
@end

@implementation AmigaBlitterTests

- (void)testWorkbench13BootLogo {
	uint16_t ram[512 * 1024]{};
	Amiga::Chipset nonChipset;
	Amiga::Blitter blitter(nonChipset, ram, 256 * 1024);
}

@end
