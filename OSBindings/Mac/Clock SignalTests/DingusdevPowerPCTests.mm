//
//  DingusdevPowerPCTests.mm
//  Clock Signal
//
//  Created by Thomas Harte on 18/03/2022.
//  Copyright 2022 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <cstdlib>

#include "../../../InstructionSets/PowerPC/Decoder.hpp"

using namespace InstructionSet::PowerPC;

@interface DingusdevPowerPCTests : XCTestCase
@end

@implementation DingusdevPowerPCTests

- (void)testDecoding {
	NSData *const testData =
		[NSData dataWithContentsOfURL:
			[[NSBundle bundleForClass:[self class]]
				URLForResource:@"ppcdisasmtest"
				withExtension:@"csv"
				subdirectory:@"dingusdev PowerPC tests"]];

	NSString *const wholeFile = [[NSString alloc] initWithData:testData encoding:NSUTF8StringEncoding];
	NSArray<NSString *> *const lines = [wholeFile componentsSeparatedByString:@"\n"];

	InstructionSet::PowerPC::Decoder decoder(InstructionSet::PowerPC::Model::MPC601);
	for(NSString *const line in lines) {
		// Ignore empty lines and comments.
		if([line length] == 0) {
			continue;
		}
		if([line characterAtIndex:0] == '#') {
			continue;
		}

		NSArray<NSString *> *const columns = [line componentsSeparatedByString:@","];

		// Columns are 1: address; 2: opcode; 3–: specific to the instruction.
		const uint32_t address = uint32_t(std::strtol([columns[0] UTF8String], 0, 16));
		const uint32_t opcode = uint32_t(std::strtol([columns[1] UTF8String], 0, 16));
		NSString *const operation = columns[2];
		const auto instruction = decoder.decode(opcode);

		switch(instruction.operation) {
			default:
				NSAssert(FALSE, @"Didn't handle %@", line);
			break;

			case Operation::bcctrx: {
				NSString *baseOperation = nil;
				switch(instruction.branch_options()) {
					case BranchOptions::Always:			baseOperation = @"bctr";	break;
					case BranchOptions::Clear:
						switch(Condition(instruction.bi() & 3)) {
							default: break;
							case Condition::Negative:	baseOperation = @"bgectr";	break;
							case Condition::Positive:	baseOperation = @"blectr";	break;
							case Condition::Zero:		baseOperation = @"bnectr";	break;
							case Condition::SummaryOverflow:
								baseOperation = @"bsoctr";
							break;
						}
					break;
					case BranchOptions::Set:
						switch(Condition(instruction.bi() & 3)) {
							default: break;
							case Condition::Negative:	baseOperation = @"bltctr";	break;
							case Condition::Positive:	baseOperation = @"bgtctr";	break;
							case Condition::Zero:		baseOperation = @"beqctr";	break;
							case Condition::SummaryOverflow:
								baseOperation = @"bnsctr";
							break;
						}
					break;
					default: break;
				}

				if(!baseOperation) {
					NSAssert(FALSE, @"Didn't handle bi %d for bo %d, %@", instruction.bi() & 3, instruction.bo(), line);
				} else {
					if(instruction.lk()) {
						baseOperation = [baseOperation stringByAppendingString:@"l"];
					}
					if(instruction.branch_prediction_hint()) {
						baseOperation = [baseOperation stringByAppendingString:@"+"];
					}
					XCTAssertEqualObjects(operation, baseOperation);
				}

				if(instruction.bi() & ~3) {
					NSString *const expectedCR = [NSString stringWithFormat:@"cr%d", instruction.bi() >> 2];
					XCTAssertEqualObjects(columns[3], expectedCR);
				}
			} break;

			case Operation::bcx: {
				switch(instruction.branch_options()) {
					case BranchOptions::Always:
						XCTAssertEqualObjects(operation, @"b");
					break;
					default:
						NSLog(@"No opcode tested for bcx with bo %02x", instruction.bo());
					break;
				}

				const uint32_t destination = uint32_t(std::strtol([columns[3] UTF8String], 0, 16));
				const uint32_t decoded_destination = instruction.bd() + address;
				XCTAssertEqual(decoded_destination, destination);
			} break;

			case Operation::bx: {
				switch((instruction.aa() ? 2 : 0) | (instruction.lk() ? 1 : 0)) {
					case 0:	XCTAssertEqualObjects(operation, @"b");		break;
					case 1:	XCTAssertEqualObjects(operation, @"bl");	break;
					case 2:	XCTAssertEqualObjects(operation, @"ba");	break;
					case 3:	XCTAssertEqualObjects(operation, @"bla");	break;
				}

				const uint32_t destination = uint32_t(std::strtol([columns[3] UTF8String], 0, 16));
				const uint32_t decoded_destination =
					instruction.li() + (instruction.aa() ? 0 : address);
				XCTAssertEqual(decoded_destination, destination);
			} break;
		}
	}
}

@end
