//
//  DispatcherTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 12/06/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "RangeDispatcher.hpp"

#include <array>
#include <cassert>
#include <vector>

@interface DispatcherTests : XCTestCase
@end

@implementation DispatcherTests {
}

- (void)setUp {
}

- (void)tearDown {
}

struct DoStep {
	static constexpr int max = 100;
	template <int n> void perform(int, int) {
		assert(n < max);
		performed[n] = true;
	}
	std::array<bool, max> performed{};
};

- (void)testPoints {
	DoStep stepper;

	Reflection::RangeDispatcher<DoStep>::dispatch(stepper, 0, 10);
	for(size_t c = 0; c < stepper.performed.size(); c++) {
		XCTAssert(stepper.performed[c] == (c < 10));
	}

	Reflection::RangeDispatcher<DoStep>::dispatch(stepper, 29, 100000);
	for(size_t c = 0; c < stepper.performed.size(); c++) {
		XCTAssert(stepper.performed[c] == (c < 10) || (c >= 29));
	}
}

enum class RangeType {
	Sync, Border
};

struct RangeClassifier {
	static constexpr int max = 50;

	static constexpr RangeType region(int x) {
		return x >= 10 && x < 20 ? RangeType::Sync : RangeType::Border;
	}
};

struct RangeTarget {
	struct Event {
		enum class Type {
			Begin, End, Advance
		};
		Type event_type;
		RangeType range_type;
		int length = 0;

		Event(Type event_type, RangeType range_type) : event_type(event_type), range_type(range_type) {}
		Event(Type event_type, RangeType range_type, int length) : event_type(event_type), range_type(range_type), length(length) {}
	};
	std::vector<Event> events;

	template <RangeType type> void begin(int) {
		events.emplace_back(Event::Type::Begin, type);
	}
	template <RangeType type> void end(int) {
		events.emplace_back(Event::Type::End, type);
	}
	template <RangeType type> void advance(int length) {
		events.emplace_back(Event::Type::Advance, type, length);
	}
};

- (void)testRanges {
	using Dispatcher = Reflection::SubrangeDispatcher<RangeClassifier, RangeTarget>;
	Dispatcher dispatcher;
	Reflection::RangeDispatcher<Dispatcher>::dispatch(dispatcher, 0, 10);

	printf("");
}

@end
