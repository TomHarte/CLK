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

			case Operation::lwzx: {
				XCTAssertEqualObjects(operation, @"lwzx");
				NSString *const rA = instruction.rA() ? [NSString stringWithFormat:@"r%d", instruction.rA()] : @"0";
				NSString *const rB = [NSString stringWithFormat:@"r%d", instruction.rB()];
				NSString *const rD = [NSString stringWithFormat:@"r%d", instruction.rD()];
				XCTAssertEqualObjects(rD, columns[3]);
				XCTAssertEqualObjects(rA, columns[4]);
				XCTAssertEqualObjects(rB, columns[5]);
			} break;

			case Operation::bcx:
			case Operation::bclrx:
			case Operation::bcctrx: {
				NSString *baseOperation = nil;
				BOOL addConditionToOperand = NO;

				switch(instruction.branch_options()) {
					case BranchOption::Always:			baseOperation = @"b";		break;
					case BranchOption::Dec_Zero:		baseOperation = @"bdz";		break;
					case BranchOption::Dec_NotZero:		baseOperation = @"bdnz";	break;

					case BranchOption::Clear:
						switch(Condition(instruction.bi() & 3)) {
							default: break;
							case Condition::Negative:	baseOperation = @"bge";	break;
							case Condition::Positive:	baseOperation = @"ble";	break;
							case Condition::Zero:		baseOperation = @"bne";	break;
							case Condition::SummaryOverflow:
								baseOperation = @"bns";
							break;
						}
					break;
					case BranchOption::Dec_ZeroAndClear:
						baseOperation = @"bdzf";
						addConditionToOperand = YES;
					break;
					case BranchOption::Dec_NotZeroAndClear:
						baseOperation = @"bdnzf";
						addConditionToOperand = YES;
					break;

					case BranchOption::Set:
						switch(Condition(instruction.bi() & 3)) {
							default: break;
							case Condition::Negative:	baseOperation = @"blt";	break;
							case Condition::Positive:	baseOperation = @"bgt";	break;
							case Condition::Zero:		baseOperation = @"beq";	break;
							case Condition::SummaryOverflow:
								baseOperation = @"bso";
							break;
						}
					break;
					case BranchOption::Dec_ZeroAndSet:
						baseOperation = @"bdzt";
						addConditionToOperand = YES;
					break;
					case BranchOption::Dec_NotZeroAndSet:
						baseOperation = @"bdnzt";
						addConditionToOperand = YES;
					break;

					default: break;
				}

				switch(instruction.operation) {
					case Operation::bcctrx:
						baseOperation = [baseOperation stringByAppendingString:@"ctr"];
					break;
					case Operation::bclrx:
						baseOperation = [baseOperation stringByAppendingString:@"lr"];
					break;

					case Operation::bcx: {
						uint32_t decoded_destination;
						if(instruction.aa()) {
							decoded_destination = instruction.bd();
						} else {
							decoded_destination = instruction.bd() + address;
						}

						const uint32_t destination = uint32_t(std::strtol([[columns lastObject] UTF8String], 0, 16));
						XCTAssertEqual(decoded_destination, destination);
					} break;

					default: break;
				}

				if(!baseOperation) {
					NSAssert(FALSE, @"Didn't handle bi %d for bo %d, %@", instruction.bi() & 3, instruction.bo(), line);
				} else {
					if(instruction.lk()) {
						baseOperation = [baseOperation stringByAppendingString:@"l"];
					}
					if(instruction.aa()) {
						baseOperation = [baseOperation stringByAppendingString:@"a"];
					}
					if(instruction.branch_prediction_hint()) {
						baseOperation = [baseOperation stringByAppendingString:@"+"];
					}
					XCTAssertEqualObjects(operation, baseOperation);
				}

				if(instruction.bi() & ~3) {
					NSString *expectedCR;

					if(addConditionToOperand) {
						NSString *suffix;
						switch(Condition(instruction.bi() & 3)) {
							default: break;
							case Condition::Negative:			suffix = @"lt";	break;
							case Condition::Positive:			suffix = @"gt";	break;
							case Condition::Zero:				suffix = @"eq";	break;
							case Condition::SummaryOverflow:	suffix = @"so"; break;
						}

						expectedCR = [NSString stringWithFormat:@"4*cr%d+%@", instruction.bi() >> 2, suffix];
					} else {
						expectedCR = [NSString stringWithFormat:@"cr%d", instruction.bi() >> 2];
					}
					XCTAssertEqualObjects(columns[3], expectedCR);
				}
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
