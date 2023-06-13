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
	template <int n> void perform() {
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
	static constexpr int max = 200;

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

		Event(Type event_type, RangeType range_type, int length) : event_type(event_type), range_type(range_type), length(length) {}

		bool operator ==(const Event &rhs) const {
			if(rhs.event_type != event_type) return false;
			if(rhs.range_type != range_type) return false;
			return rhs.length == length;
		}
	};
	std::vector<Event> events;

	template <RangeType type> void begin(int position) {
		events.emplace_back(Event::Type::Begin, type, position);
	}
	template <RangeType type> void end(int position) {
		events.emplace_back(Event::Type::End, type, position);
	}
	template <RangeType type> void advance(int length) {
		events.emplace_back(Event::Type::Advance, type, length);
	}
};

- (void)testRanges {
	RangeTarget target;
	using Dispatcher = Reflection::SubrangeDispatcher<RangeClassifier, RangeTarget>;
	Dispatcher::dispatch(target, 0, 11);

	XCTAssertEqual(target.events.size(), 5);
	XCTAssert(target.events[0] == RangeTarget::Event(RangeTarget::Event::Type::Begin, RangeType::Border, 0));
	XCTAssert(target.events[1] == RangeTarget::Event(RangeTarget::Event::Type::Advance, RangeType::Border, 10));
	XCTAssert(target.events[2] == RangeTarget::Event(RangeTarget::Event::Type::End, RangeType::Border, 10));
	XCTAssert(target.events[3] == RangeTarget::Event(RangeTarget::Event::Type::Begin, RangeType::Sync, 10));
	XCTAssert(target.events[4] == RangeTarget::Event(RangeTarget::Event::Type::Advance, RangeType::Sync, 1));

	Dispatcher::dispatch(target, 11, 75);
	Dispatcher::dispatch(target, 75, 100);
	Dispatcher::dispatch(target, 100, 199);
	Dispatcher::dispatch(target, 199, 200);
	Dispatcher::dispatch(target, 200, 400);	// Out of range.

	XCTAssertEqual(target.events.size(), 13);
	XCTAssert(target.events[5] == RangeTarget::Event(RangeTarget::Event::Type::Advance, RangeType::Sync, 9));
	XCTAssert(target.events[6] == RangeTarget::Event(RangeTarget::Event::Type::End, RangeType::Sync, 20));
	XCTAssert(target.events[7] == RangeTarget::Event(RangeTarget::Event::Type::Begin, RangeType::Border, 20));
	XCTAssert(target.events[8] == RangeTarget::Event(RangeTarget::Event::Type::Advance, RangeType::Border, 55));
	XCTAssert(target.events[9] == RangeTarget::Event(RangeTarget::Event::Type::Advance, RangeType::Border, 25));
	XCTAssert(target.events[10] == RangeTarget::Event(RangeTarget::Event::Type::Advance, RangeType::Border, 99));
	XCTAssert(target.events[11] == RangeTarget::Event(RangeTarget::Event::Type::Advance, RangeType::Border, 1));
	XCTAssert(target.events[12] == RangeTarget::Event(RangeTarget::Event::Type::End, RangeType::Border, 200));
}

@end
