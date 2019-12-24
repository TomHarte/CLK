//
//  MacintoshVideoTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 09/07/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <memory>
#include "../../../Machines/Apple/Macintosh/Video.hpp"

@interface MacintoshVideoTests : XCTestCase
@end

@implementation MacintoshVideoTests {
	Apple::Macintosh::DeferredAudio _dummy_audio;
	Apple::Macintosh::DriveSpeedAccumulator _dummy_drive_speed_accumulator;
	std::unique_ptr<Apple::Macintosh::Video> _video;
	uint16_t _ram[64*1024];
}

- (void)setUp {
	// Put setup code here. This method is called before the invocation of each test method in the class.
	_video = std::make_unique<Apple::Macintosh::Video>(_dummy_audio, _dummy_drive_speed_accumulator);
	_video->set_ram(_ram, sizeof(_ram) - 1);
}

- (void)testPrediction {
	int c = 5;
	bool vsync = _video->vsync();
	while(c--) {
		auto remaining_time_in_state = _video->get_next_sequence_point().as_integral();
		NSLog(@"Vsync %@ expected for %@ half-cycles", vsync ? @"on" : @"off", @(remaining_time_in_state));
		while(remaining_time_in_state--) {
			XCTAssertEqual(vsync, _video->vsync());
			_video->run_for(HalfCycles(1));

			if(remaining_time_in_state)
				XCTAssertEqual(remaining_time_in_state, _video->get_next_sequence_point().as_integral());
		}
		vsync ^= true;
	}
}

@end
