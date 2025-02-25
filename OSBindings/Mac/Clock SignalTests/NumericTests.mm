//
//  NumericTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 20/02/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "BitStream.hpp"

@interface NumericTests : XCTestCase
@end

@implementation NumericTests {
}

- (void)testBitStreamLSBFirst {
	constexpr uint8_t testData[] = {
		0xa3, 0xf4
	};

	auto it = std::begin(testData);
	Numeric::BitStream<2, true> stream([&]() -> uint8_t {
		if(it == std::end(testData)) return 0xff;
		return *it++;
	});

	XCTAssertEqual(stream.next<2>(), 0b11);
	XCTAssertEqual(stream.next<2>(), 0b00);
	XCTAssertEqual(stream.next<2>(), 0b01);
	XCTAssertEqual(stream.next<1>(), 0b0);
	XCTAssertEqual(stream.next<2>(), 0b10);
	XCTAssertEqual(stream.next<2>(), 0b01);
	XCTAssertEqual(stream.next<1>(), 0b0);
	XCTAssertEqual(stream.next<1>(), 0b1);
	XCTAssertEqual(stream.next<2>(), 0b11);
	XCTAssertEqual(stream.next<2>(), 0b11);
}

- (void)testBitStreamMSBFirst {
	constexpr uint8_t testData[] = {
		0xa3, 0xf4
	};

	auto it = std::begin(testData);
	Numeric::BitStream<2, false> stream([&]() -> uint8_t {
		if(it == std::end(testData)) return 0xff;
		return *it++;
	});

	XCTAssertEqual(stream.next<2>(), 0b10);
	XCTAssertEqual(stream.next<2>(), 0b10);
	XCTAssertEqual(stream.next<2>(), 0b00);
	XCTAssertEqual(stream.next<1>(), 0b1);
	XCTAssertEqual(stream.next<2>(), 0b11);
	XCTAssertEqual(stream.next<2>(), 0b11);
	XCTAssertEqual(stream.next<1>(), 0b1);
	XCTAssertEqual(stream.next<1>(), 0b0);
	XCTAssertEqual(stream.next<2>(), 0b10);
	XCTAssertEqual(stream.next<2>(), 0b01);
}

- (void)testBitStreamMultibyte {
	constexpr uint8_t testData[] = {
		0xa3, 0xf4, 0x11
	};

	auto it = std::begin(testData);
	Numeric::BitStream<11, false> stream([&]() -> uint8_t {
		if(it == std::end(testData)) return 0xff;
		return *it++;
	});

	XCTAssertEqual(stream.next<2>(), 0b10);
	XCTAssertEqual(stream.next<9>(), 0b100011111);
	XCTAssertEqual(stream.next<11>(), 0b10100000100);
	XCTAssertEqual(stream.next<3>(), 0b011);
}

@end
