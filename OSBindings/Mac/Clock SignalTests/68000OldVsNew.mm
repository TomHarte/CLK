//
//  68000ArithmeticTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 28/06/2019.
//
//  Largely ported from the tests of the Portable 68k Emulator.
//

#import <XCTest/XCTest.h>

#include "TestRunner68000.hpp"
#include "68000.hpp"
#include "68000Mk2.hpp"

namespace {

struct BusHandler {

};

using OldProcessor = CPU::MC68000::Processor<BusHandler, true>;
using NewProcessor = CPU::MC68000Mk2::Processor<BusHandler, true, true>;

template <typename M68000> struct Tester {
	Tester() : processor_(bus_handler_) {}

	BusHandler bus_handler_;
	M68000 processor_;
};

}

@interface M68000OldVsNewTests : XCTestCase
@end

@implementation M68000OldVsNewTests {
}

- (void)testOldVsNew {
	Tester<OldProcessor> oldTester;
	Tester<NewProcessor> newTester;
}

@end
