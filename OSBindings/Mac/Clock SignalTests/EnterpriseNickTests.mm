//
//  EnterpriseNickTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 18/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Machines/Enterprise/Nick.hpp"
#include <memory>

@interface EnterpriseNickTests : XCTestCase
@end

@implementation EnterpriseNickTests {
	std::unique_ptr<Enterprise::Nick> _nick;
	uint8_t _ram[64*1024];
	int _totalLines;
}

- (void)setUp {
	[super setUp];

	// Create a Nick.
	_nick = std::make_unique<Enterprise::Nick>(_ram);

	// Add a basic line table of blocks proceeding in length: 1, 2, 3, 4, etc and toggling the interrupt bit.
	_totalLines = 0;
	int nextLength = 0;
	int pointer = 0;
	uint8_t interruptFlag = 0x80;
	while(nextLength < 256) {
		_ram[pointer] = 0x100 - nextLength;
		_ram[pointer+1] = interruptFlag;

		pointer += 16;
		++nextLength;
		interruptFlag ^= 0x80;
		_totalLines += nextLength;
	}

	// For now: assume Nick starts at address 0 from creation.
}

- (void)testInterruptPrediction {
	// Run for the number of cycles implied by the number of lines.
	int next_sequence_point = _nick->get_next_sequence_point().as<int>();
	bool last_interrupt_line = _nick->get_interrupt_line();

	for(int c = 0; c < _totalLines*912; c++) {
		// Check that interrupt line transitions happen only on declared sequence points.
		_nick->run_for(Cycles(1));
		--next_sequence_point;
		const bool interrupt_line = _nick->get_interrupt_line();

		if(interrupt_line != last_interrupt_line) {
			XCTAssertEqual(next_sequence_point, 0);
		}
		last_interrupt_line = interrupt_line;

		if(!next_sequence_point) {
			next_sequence_point = _nick->get_next_sequence_point().as<int>();
		} else {
			const int expected_next_sequence_point = _nick->get_next_sequence_point().as<int>();
			XCTAssertEqual(next_sequence_point, expected_next_sequence_point);
		}
	}
}

@end
