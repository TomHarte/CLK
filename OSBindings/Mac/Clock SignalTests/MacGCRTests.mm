//
//  MacGCRTests.mm
//  Clock SignalTests
//
//  Created by Thomas Harte on 15/06/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include "../../../Storage/Disk/Encodings/AppleGCR/Encoder.hpp"

@interface MacGCRTests : XCTestCase
@end

@implementation MacGCRTests {
}

- (void)testSector0Track0Side0 {
	const auto header = Storage::Encodings::AppleGCR::Macintosh::header(0x22, 0, 0, false);
	const std::vector<uint8_t> expected_mark = {
		0xd5, 0xaa, 0x96,
		0x96, 0x96, 0x96, 0xd9, 0xd9,
		0xde, 0xaa, 0xeb
	};
	const auto mark_segment = Storage::Disk::PCMSegment(expected_mark);

	XCTAssertEqual(mark_segment.data, header.data);
}

- (void)testSector9Track11Side1 {
	const auto header = Storage::Encodings::AppleGCR::Macintosh::header(0x22, 11, 9, true);
	const std::vector<uint8_t> expected_mark = {
		0xd5, 0xaa, 0x96,
		0xad, 0xab, 0xd6, 0xd9, 0x96,
		0xde, 0xaa, 0xeb
	};
	const auto mark_segment = Storage::Disk::PCMSegment(expected_mark);

	XCTAssertEqual(mark_segment.data, header.data);
}

@end
