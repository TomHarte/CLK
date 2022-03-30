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

namespace {

void AssertEqualOperationName(NSString *lhs, NSString *rhs) {
	NSString *const lhsMapped = [lhs stringByReplacingOccurrencesOfString:@"_" withString:@"."];
	NSString *const rhsMapped = [rhs stringByReplacingOccurrencesOfString:@"_" withString:@"."];
	XCTAssertEqualObjects(lhsMapped, rhsMapped);
}

void AssertEqualOperationNameOE(NSString *lhs, Instruction instruction, NSString *rhs) {
	XCTAssert([lhs characterAtIndex:lhs.length - 1] == 'x');
	lhs = [lhs substringToIndex:lhs.length - 1];
	if(instruction.oe()) lhs = [lhs stringByAppendingString:@"o"];
	if(instruction.rc()) lhs = [lhs stringByAppendingString:@"."];
	XCTAssertEqualObjects(lhs, rhs);
}

void AssertEqualR(NSString *name, uint32_t reg) {
	NSString *const regName = [NSString stringWithFormat:@"r%d", reg];
	XCTAssertEqualObjects(name, regName);
}

NSString *condition(uint32_t code) {
	NSString *suffix;
	switch(Condition(code & 3)) {
		default: break;
		case Condition::Negative:			suffix = @"lt";	break;
		case Condition::Positive:			suffix = @"gt";	break;
		case Condition::Zero:				suffix = @"eq";	break;
		case Condition::SummaryOverflow:	suffix = @"so"; break;
	}

	if(code & ~3) {
		return [NSString stringWithFormat:@"4*cr%d+%@", code >> 2, suffix];
	} else {
		return suffix;
	}
}

}

@implementation DingusdevPowerPCTests

- (void)testABDInstruction:(Instruction)instruction columns:(NSArray<NSString *> *)columns testZero:(BOOL)testZero {
	NSString *const rA = (instruction.rA() || !testZero) ? [NSString stringWithFormat:@"r%d", instruction.rA()] : @"0";
	NSString *const rD = [NSString stringWithFormat:@"r%d", instruction.rD()];
	XCTAssertEqualObjects(rD, columns[3]);
	XCTAssertEqualObjects(rA, columns[4]);

	if([columns count] > 5) {
		NSString *const rB = [NSString stringWithFormat:@"r%d", instruction.rB()];
		XCTAssertEqualObjects(rB, columns[5]);
	}
}

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

		NSLog(@"%@", line);
		switch(instruction.operation) {
			default:
				NSAssert(FALSE, @"Didn't handle %@", line);
			break;

			case Operation::rlwinmx: {
				// This maps the opposite way from most of the other tests
				// owing to the simplified names being a shade harder to
				// detect motivationally.
				XCTAssertEqual((instruction.rc() != 0), ([operation characterAtIndex:operation.length - 1] == '.'));
				AssertEqualR(columns[3], instruction.rA());
				AssertEqualR(columns[4], instruction.rS());

				const auto n = [columns[5] intValue];
				const auto b = columns.count > 6 ? [columns[6] intValue] : 0;

				if([operation isEqualToString:@"extlwi"] || [operation isEqualToString:@"extlwi."]) {
					XCTAssertEqual(instruction.sh(), b);
					XCTAssertEqual(instruction.mb(), 0);
					XCTAssertEqual(instruction.me(), n - 1);
					break;
				}

				if([operation isEqualToString:@"extrwi"] || [operation isEqualToString:@"extrwi."]) {
					XCTAssertEqual(instruction.sh(), b + n);
					XCTAssertEqual(instruction.mb(), 32 - n);
					XCTAssertEqual(instruction.me(), 31);
					break;
				}

				if([operation isEqualToString:@"inslwi"] || [operation isEqualToString:@"inslwi."]) {
					XCTAssertEqual(instruction.sh(), 32 - b);
					XCTAssertEqual(instruction.mb(), b);
					XCTAssertEqual(instruction.me(), b + n - 1);
					break;
				}

				if([operation isEqualToString:@"insrwi"] || [operation isEqualToString:@"insrwi."]) {
					XCTAssertEqual(instruction.sh(), 32 - (b + n));
					XCTAssertEqual(instruction.mb(), b);
					XCTAssertEqual(instruction.me(), b + n - 1);
					break;
				}

				if([operation isEqualToString:@"rotlwi"] || [operation isEqualToString:@"rotlwi."]) {
					XCTAssertEqual(instruction.sh(), n);
					XCTAssertEqual(instruction.mb(), 0);
					XCTAssertEqual(instruction.me(), 31);
					break;
				}

				if([operation isEqualToString:@"rotrwi"] || [operation isEqualToString:@"rotrwi."]) {
					XCTAssertEqual(instruction.sh(), 32 - n);
					XCTAssertEqual(instruction.mb(), 0);
					XCTAssertEqual(instruction.me(), 31);
					break;
				}

				if([operation isEqualToString:@"rotlw"] || [operation isEqualToString:@"rotlw."]) {
					XCTAssertEqual(instruction.rB(), n);
					XCTAssertEqual(instruction.mb(), 0);
					XCTAssertEqual(instruction.me(), 31);
					break;
				}

				if([operation isEqualToString:@"slwi"] || [operation isEqualToString:@"slwi."]) {
					XCTAssertEqual(instruction.sh(), n);
					XCTAssertEqual(instruction.mb(), 0);
					XCTAssertEqual(instruction.me(), 31 - n);
					break;
				}

				if([operation isEqualToString:@"srwi"] || [operation isEqualToString:@"srwi."]) {
					XCTAssertEqual(instruction.sh(), 32 - n);
					XCTAssertEqual(instruction.mb(), n);
					XCTAssertEqual(instruction.me(), 31);
					break;
				}

				if([operation isEqualToString:@"clrlwi"] || [operation isEqualToString:@"clrlwi."]) {
					XCTAssertEqual(instruction.sh(), 0);
					XCTAssertEqual(instruction.mb(), n);
					XCTAssertEqual(instruction.me(), 31);
					break;
				}

				if([operation isEqualToString:@"clrrwi"] || [operation isEqualToString:@"clrrwi."]) {
					XCTAssertEqual(instruction.sh(), 0);
					XCTAssertEqual(instruction.mb(), 0);
					XCTAssertEqual(instruction.me(), 31 - n);
					break;
				}

				if([operation isEqualToString:@"clrlslwi"] || [operation isEqualToString:@"clrlslwi."]) {
					XCTAssertEqual(instruction.sh(), n);
					XCTAssertEqual(instruction.mb(), b - n);
					XCTAssertEqual(instruction.me(), 31 - n);
					break;
				}

				NSAssert(FALSE, @"Didn't handle %@", line);
			} break;

#define CRMod(x) \
			case Operation::x:	\
				AssertEqualOperationName(operation, @#x);	\
				XCTAssertEqualObjects(columns[3], condition(instruction.crbD()));	\
				XCTAssertEqualObjects(columns[4], condition(instruction.crbA()));	\
				XCTAssertEqualObjects(columns[5], condition(instruction.crbB()));	\
			break;

			CRMod(crand);
			CRMod(crandc);
			CRMod(creqv);
			CRMod(crnand);
			CRMod(crnor);
			CRMod(cror);
			CRMod(crorc);
			CRMod(crxor);

#undef CRMod

			case Operation::mtcrf: {
				AssertEqualOperationName(operation,
					instruction.crm() != 0xff ? @"mtcrf" : @"mtcr");

				NSString *const rS = [NSString stringWithFormat:@"r%d", instruction.rS()];
				XCTAssertEqualObjects(rS, [columns lastObject]);

				if(columns.count > 4) {
					const auto crm = strtol([columns[3] UTF8String], NULL, 16);
					XCTAssertEqual(crm, instruction.crm());
				}
			} break;


#define ArithImm(x) \
			case Operation::x: {	\
				NSString *const rD = [NSString stringWithFormat:@"r%d", instruction.rD()];	\
				NSString *const rA = [NSString stringWithFormat:@"r%d", instruction.rA()];	\
				const auto simm = strtol([columns[5] UTF8String], NULL, 16);	\
				AssertEqualOperationName(operation, @#x);	\
				XCTAssertEqualObjects(columns[3], rD);	\
				XCTAssertEqualObjects(columns[4], rA);	\
				XCTAssertEqual(simm, instruction.simm());	\
			} break;

			ArithImm(mulli);
			ArithImm(subfic);
			ArithImm(addi);
			ArithImm(addic);
			ArithImm(addic_);
			ArithImm(addis);

#undef ArithImm

#define ABCz(x)	\
			case Operation::x:	\
				AssertEqualOperationName(operation, @#x);	\
				[self testABDInstruction:instruction columns:columns testZero:YES];	\
			break;

			ABCz(lwzx);
			ABCz(lwzux);
			ABCz(lbzx);
			ABCz(lbzux);
			ABCz(stwx);
			ABCz(stwux);
			ABCz(stbx);
			ABCz(stbux);
			ABCz(lhzx);
			ABCz(lhzux);
			ABCz(lhax);
			ABCz(lhaux);
			ABCz(sthx);
			ABCz(sthux);

#undef ABCz

#define ABD(x)	\
			case Operation::x:	\
				AssertEqualOperationNameOE(@#x, instruction, operation);	\
				[self testABDInstruction:instruction columns:columns testZero:NO];	\
			break;

			ABD(subfcx);
			ABD(subfx);
			ABD(negx);
			ABD(subfex);
			ABD(subfzex);
			ABD(subfmex);
			ABD(dozx);
			ABD(absx);
			ABD(nabsx);
			ABD(addx);
			ABD(addcx);
			ABD(addex);
			ABD(addmex);
			ABD(addzex);
			ABD(mulhwx);
			ABD(mulhwux);
			ABD(mulx);
			ABD(mullwx);
			ABD(divx);
			ABD(divsx);
			ABD(divwux);
			ABD(divwx);

#undef ABD

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
					AssertEqualOperationName(operation, baseOperation);
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
					case 0:	AssertEqualOperationName(operation, @"b");		break;
					case 1:	AssertEqualOperationName(operation, @"bl");		break;
					case 2:	AssertEqualOperationName(operation, @"ba");		break;
					case 3:	AssertEqualOperationName(operation, @"bla");	break;
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
