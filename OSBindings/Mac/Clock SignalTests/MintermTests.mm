//
//  MintermTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 20/09/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <cstdint>
#include "Minterms.h"

namespace {

/// Implements the Amiga minterm algorithm in a bit-by-bit form.
template <typename IntT> IntT slow_minterm(IntT a, IntT b, IntT c, uint8_t minterm) {
	constexpr int top_shift = sizeof(IntT) * 8 - 1;

	IntT result = 0;
	for(int i = 0; i < 8*sizeof(IntT); i++) {
		int index = ((c&1) << 0) | ((b&1) << 1) | ((a&1) << 2);

		result >>= 1;
		result |= ((minterm >> index) & 1) << top_shift;

		a >>= 1;
		b >>= 1;
		c >>= 1;
	}

	return result;
}

}

@interface MintermTests: XCTestCase
@end

@implementation MintermTests

- (void)testAll {
	// These three are selected just to ensure that all combinations of
	// bit pattern exist in the input.
	constexpr uint8_t a = 0xaa;
	constexpr uint8_t b = 0xcc;
	constexpr uint8_t c = 0xf0;

	for(int minterm = 0; minterm < 256; minterm++) {
		const uint8_t slow = slow_minterm(a, b, c, minterm);
		const uint8_t fast = Amiga::apply_minterm(a, b, c, minterm);

		XCTAssertEqual(slow, fast, "Mismatch found between naive and fast implementations for %02x", minterm);
	}
}

@end
