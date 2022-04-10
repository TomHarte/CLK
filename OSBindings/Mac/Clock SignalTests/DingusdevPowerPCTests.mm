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

@interface NSString (HexConversion)

- (long int)hexInt;

@end

@implementation NSString (HexConversion)

- (long int)hexInt {
	return strtol([self UTF8String], NULL, 16);
}

@end

@interface DingusdevPowerPCTests : XCTestCase
@end

namespace {

enum class NamingConvention {
	None = 0,
	ApplyO = 1,
	ApplyE = 2,
	ApplyOE = 3
};

/// Converts any underscores in @c rhs to dots.
///
/// If convention != NamingConvention::None, trims the trailing 'x' from the @c rhs and
/// appends one or more of @c o and @c . depending on the @c oe() and @c rc() flags of @c instruction.
///
/// Then compares with the @c lhs.
void AssertEqualOperationName(NSString *lhs, NSString *rhs, Instruction instruction = Instruction(), NamingConvention convention = NamingConvention::None) {
	rhs = [rhs stringByReplacingOccurrencesOfString:@"_" withString:@"."];

	if(convention != NamingConvention::None) {
		XCTAssert([rhs characterAtIndex:rhs.length - 1] == 'x');
		rhs = [rhs substringToIndex:rhs.length - 1];
	}
	if(int(convention) & int(NamingConvention::ApplyO) && instruction.oe()) rhs = [rhs stringByAppendingString:@"o"];
	if(int(convention) & int(NamingConvention::ApplyE) && instruction.rc()) rhs = [rhs stringByAppendingString:@"."];

	XCTAssertEqualObjects(lhs, rhs);
}
void AssertEqualOperationNameO(NSString *lhs, NSString *rhs, Instruction instruction) {
	AssertEqualOperationName(lhs, rhs, instruction, NamingConvention::ApplyO);
}
void AssertEqualOperationNameE(NSString *lhs, NSString *rhs, Instruction instruction) {
	AssertEqualOperationName(lhs, rhs, instruction, NamingConvention::ApplyE);
}
void AssertEqualOperationNameOE(NSString *lhs, NSString *rhs, Instruction instruction) {
	AssertEqualOperationName(lhs, rhs, instruction, NamingConvention::ApplyOE);
}

/// Forms the string @c r[reg] and compares it to @c name
void AssertEqualR(NSString *name, uint32_t reg, bool permitR0 = true) {
	NSString *const regName = (reg || permitR0) ? [NSString stringWithFormat:@"r%d", reg] : @"0";
	XCTAssertEqualObjects(name, regName);
}

/// Forms the string @c f[reg] and compares it to @c name
void AssertEqualFR(NSString *name, uint32_t reg, bool permitR0 = true) {
	NSString *const regName = (reg || permitR0) ? [NSString stringWithFormat:@"f%d", reg] : @"0";
	XCTAssertEqualObjects(name, regName);
}

/// @returns the text name of the condition code @c code
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

NSString *conditionreg(uint32_t code) {
	return [NSString stringWithFormat:@"cr%d", code];
}


NSString *offset(Instruction instruction) {
	NSString *const hexPart = [NSString stringWithFormat:@"%s%X", (instruction.d() < 0) ? "-0x" : "0x", abs(instruction.d())];

	if(instruction.rA()) {
		return [hexPart stringByAppendingFormat:@"(r%d)", instruction.rA()];
	} else {
		return hexPart;
	}
}

}

@implementation DingusdevPowerPCTests

- (void)testABDInstruction:(Instruction)instruction columns:(NSArray<NSString *> *)columns testZero:(BOOL)testZero {
	NSString *const rA = (instruction.rA() || !testZero) ? [NSString stringWithFormat:@"r%d", instruction.rA()] : @"0";
	XCTAssertEqualObjects(rA, columns[4]);
	AssertEqualR(columns[3], instruction.rD());

	if([columns count] > 5) {
		AssertEqualR(columns[5], instruction.rB());
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

	InstructionSet::PowerPC::Decoder<InstructionSet::PowerPC::Model::MPC601, true> decoder;
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
		const auto address = uint32_t([columns[0] hexInt]);
		const auto opcode = uint32_t([columns[1] hexInt]);
		NSString *const operation = columns[2];
		const auto instruction = decoder.decode(opcode);

		// Deal with some of the simplified mnemonics as special cases;
		// underlying observation: distinguishing special cases isn't that
		// important to a mere decoder.
		if([operation isEqualToString:@"nop"]) {
			// These tests use ORI for NOP.
			XCTAssertEqual(instruction.operation, Operation::ori);
			XCTAssertEqual(instruction.rD(), instruction.rA());
			XCTAssertEqual(instruction.simm(), 0);
			continue;
		}

		if([operation isEqualToString:@"mr"] || [operation isEqualToString:@"mr."]) {
			// OR is used to achieve MR (i.e. move register).
			XCTAssertEqual(instruction.operation, Operation::orx);
			XCTAssertEqual((instruction.rc() != 0), ([operation characterAtIndex:operation.length - 1] == '.'));
			AssertEqualR(columns[3], instruction.rA());
			AssertEqualR(columns[4], instruction.rS());
			AssertEqualR(columns[4], instruction.rB());
			continue;
		}

		if([operation isEqualToString:@"li"]) {
			// ADDI is used to achieve LI (i.e. load immediate).
			XCTAssertEqual(instruction.operation, Operation::addi);
			AssertEqualR(columns[3], instruction.rD());
			XCTAssertEqual(0, instruction.rA());
			XCTAssertEqual([columns[4] hexInt], instruction.simm());
			continue;
		}

		switch(instruction.operation) {
			default:
				NSAssert(FALSE, @"Didn't handle %@", line);
			break;

			case Operation::Undefined:
				XCTAssertEqualObjects(operation, @"dc.l");
			break;

			case Operation::rlwimix: {
				// This maps the opposite way from most of the other tests
				// owing to the simplified names being a shade harder to
				// detect motivationally.
				XCTAssertEqual((instruction.rc() != 0), ([operation characterAtIndex:operation.length - 1] == '.'));
				AssertEqualR(columns[3], instruction.rA());
				AssertEqualR(columns[4], instruction.rS());

				const auto n = [columns[5] intValue];
				const auto b = [columns[6] intValue];

				if([operation isEqualToString:@"inslwi"] || [operation isEqualToString:@"inslwi."]) {
					XCTAssertEqual(instruction.sh<uint32_t>(), 32 - b);
					XCTAssertEqual(instruction.mb<uint32_t>(), b);
					XCTAssertEqual(instruction.me<uint32_t>(), b + n - 1);
					break;
				}

				if([operation isEqualToString:@"insrwi"] || [operation isEqualToString:@"insrwi."]) {
					XCTAssertEqual(instruction.sh<uint32_t>(), 32 - (b + n));
					XCTAssertEqual(instruction.mb<uint32_t>(), b);
					XCTAssertEqual(instruction.me<uint32_t>(), b + n - 1);
					break;
				}

				NSAssert(FALSE, @"Didn't handle rlwimix %@", line);
			} break;

			case Operation::rlwnmx: {
				XCTAssertEqual((instruction.rc() != 0), ([operation characterAtIndex:operation.length - 1] == '.'));
				AssertEqualR(columns[3], instruction.rA());
				AssertEqualR(columns[4], instruction.rS());
				AssertEqualR(columns[5], instruction.rB());

				if([operation isEqualToString:@"rotlw"] || [operation isEqualToString:@"rotlw."]) {
					XCTAssertEqual(instruction.mb<uint32_t>(), 0);
					XCTAssertEqual(instruction.me<uint32_t>(), 31);
					break;
				}

				NSAssert(FALSE, @"Didn't handle rlwnmx %@", line);
			} break;

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
					XCTAssertEqual(instruction.sh<uint32_t>(), b);
					XCTAssertEqual(instruction.mb<uint32_t>(), 0);
					XCTAssertEqual(instruction.me<uint32_t>(), n - 1);
					break;
				}

				if([operation isEqualToString:@"extrwi"] || [operation isEqualToString:@"extrwi."]) {
					XCTAssertEqual(instruction.sh<uint32_t>(), b + n);
					XCTAssertEqual(instruction.mb<uint32_t>(), 32 - n);
					XCTAssertEqual(instruction.me<uint32_t>(), 31);
					break;
				}

				if([operation isEqualToString:@"rotlwi"] || [operation isEqualToString:@"rotlwi."]) {
					XCTAssertEqual(instruction.sh<uint32_t>(), n);
					XCTAssertEqual(instruction.mb<uint32_t>(), 0);
					XCTAssertEqual(instruction.me<uint32_t>(), 31);
					break;
				}

				if([operation isEqualToString:@"rotrwi"] || [operation isEqualToString:@"rotrwi."]) {
					XCTAssertEqual(instruction.sh<uint32_t>(), 32 - n);
					XCTAssertEqual(instruction.mb<uint32_t>(), 0);
					XCTAssertEqual(instruction.me<uint32_t>(), 31);
					break;
				}

				if([operation isEqualToString:@"slwi"] || [operation isEqualToString:@"slwi."]) {
					XCTAssertEqual(instruction.sh<uint32_t>(), n);
					XCTAssertEqual(instruction.mb<uint32_t>(), 0);
					XCTAssertEqual(instruction.me<uint32_t>(), 31 - n);
					break;
				}

				if([operation isEqualToString:@"srwi"] || [operation isEqualToString:@"srwi."]) {
					XCTAssertEqual(instruction.sh<uint32_t>(), 32 - n);
					XCTAssertEqual(instruction.mb<uint32_t>(), n);
					XCTAssertEqual(instruction.me<uint32_t>(), 31);
					break;
				}

				if([operation isEqualToString:@"clrlwi"] || [operation isEqualToString:@"clrlwi."]) {
					XCTAssertEqual(instruction.sh<uint32_t>(), 0);
					XCTAssertEqual(instruction.mb<uint32_t>(), n);
					XCTAssertEqual(instruction.me<uint32_t>(), 31);
					break;
				}

				if([operation isEqualToString:@"clrrwi"] || [operation isEqualToString:@"clrrwi."]) {
					XCTAssertEqual(instruction.sh<uint32_t>(), 0);
					XCTAssertEqual(instruction.mb<uint32_t>(), 0);
					XCTAssertEqual(instruction.me<uint32_t>(), 31 - n);
					break;
				}

				if([operation isEqualToString:@"clrlslwi"] || [operation isEqualToString:@"clrlslwi."]) {
					// FreeScale switched the order of b and n in the short-form instruction here;
					// they are therefore transposed in the tests below.
					XCTAssertEqual(instruction.sh<uint32_t>(), b);
					XCTAssertEqual(instruction.mb<uint32_t>(), n - b);
					XCTAssertEqual(instruction.me<uint32_t>(), 31 - b);
					break;
				}

				NSAssert(FALSE, @"Didn't handle rlwinmx %@", line);
			} break;

			case Operation::tw:
				AssertEqualOperationName(operation, @"tw");
				XCTAssertEqual([columns[3] intValue], instruction.to());
				AssertEqualR(columns[4], instruction.rA());
				AssertEqualR(columns[5], instruction.rB());
			break;

			case Operation::twi:
				AssertEqualOperationName(operation, @"twi");
				XCTAssertEqual([columns[3] intValue], instruction.to());
				AssertEqualR(columns[4], instruction.rA());
				XCTAssertEqual([columns[5] hexInt], instruction.simm());
			break;

			case Operation::mtfsfx:
				AssertEqualOperationNameE(operation, @"mtfsfx", instruction);
				XCTAssertEqual([columns[3] intValue], instruction.fm());
				AssertEqualFR(columns[4], instruction.frB());
			break;

			case Operation::mtfsfix:
				AssertEqualOperationNameE(operation, @"mtfsfix", instruction);
				XCTAssertEqualObjects(columns[3], conditionreg(instruction.crfD()));
				XCTAssertEqual([columns[4] intValue], instruction.imm());
			break;

			case Operation::mcrxr:
				AssertEqualOperationName(operation, @"mcrxr");
				XCTAssertEqualObjects(columns[3], conditionreg(instruction.crfD()));
			break;

			case Operation::mffsx:
				AssertEqualOperationNameE(operation, @"mffsx", instruction);
				AssertEqualFR(columns[3], instruction.frD());
			break;

			case Operation::mtmsr:
				AssertEqualOperationName(operation, @"mtmsr");
				AssertEqualR(columns[3], instruction.rS());
			break;

			case Operation::mtsrin:
				AssertEqualOperationName(operation, @"mtsrin");
				AssertEqualR(columns[3], instruction.rD());
				AssertEqualR(columns[4], instruction.rB());
			break;

			case Operation::clcs:
				AssertEqualOperationName(operation, @"clcs");
				AssertEqualR(columns[3], instruction.rD());
				AssertEqualR(columns[4], instruction.rA());
			break;

			case Operation::mtsr:
				AssertEqualOperationName(operation, @"mtsr");
				XCTAssertEqual([columns[3] intValue], instruction.sr());
				AssertEqualR(columns[4], instruction.rS());
			break;

			case Operation::tlbie:
				AssertEqualOperationName(operation, @"tlbie");
				AssertEqualR(columns[3], instruction.rB());
			break;

			case Operation::icbi:
				AssertEqualOperationName(operation, @"icbi");
				AssertEqualR(columns[3], instruction.rA(), false);
				AssertEqualR(columns[4], instruction.rB());
			break;

			case Operation::lswi:
				AssertEqualOperationName(operation, @"lswi");
				AssertEqualR(columns[3], instruction.rD());
				AssertEqualR(columns[4], instruction.rA());
				XCTAssertEqual([columns[5] hexInt], instruction.nb());
			break;

			case Operation::stswi:
				AssertEqualOperationName(operation, @"stswi");
				AssertEqualR(columns[3], instruction.rS());
				AssertEqualR(columns[4], instruction.rA());
				XCTAssertEqual([columns[5] hexInt], instruction.nb());
			break;

			case Operation::rlmix:
				AssertEqualOperationNameE(operation, @"rlmix", instruction);
				AssertEqualR(columns[3], instruction.rA());
				AssertEqualR(columns[4], instruction.rS());
				AssertEqualR(columns[5], instruction.rB());
				XCTAssertEqual([columns[6] intValue], instruction.mb<uint32_t>());
				XCTAssertEqual([columns[7] intValue], instruction.me<uint32_t>());
			break;

			case Operation::cmp:
				if([operation isEqualToString:@"cmpw"]) {
					XCTAssertFalse(instruction.l());
					XCTAssertFalse(instruction.crfD());
					AssertEqualR(columns[3], instruction.rA());
					AssertEqualR(columns[4], instruction.rB());
					break;
				}

				if([operation isEqualToString:@"cmp"]) {
					XCTAssertEqualObjects(columns[3], conditionreg(instruction.crfD()));
					AssertEqualR(columns[4], instruction.rA());
					AssertEqualR(columns[5], instruction.rB());
					break;
				}

				NSAssert(FALSE, @"Didn't handle cmp %@", line);
			break;

			case Operation::cmpl:
				if([operation isEqualToString:@"cmplw"]) {
					XCTAssertFalse(instruction.l());
					if(instruction.crfD()) {
						XCTAssertEqualObjects(columns[3], conditionreg(instruction.crfD()));
						AssertEqualR(columns[4], instruction.rA());
						AssertEqualR(columns[5], instruction.rB());
					} else {
						AssertEqualR(columns[3], instruction.rA());
						AssertEqualR(columns[4], instruction.rB());
					}
					break;
				}

				NSAssert(FALSE, @"Didn't handle cmpl %@", line);
			break;

			case Operation::cmpli:
			case Operation::cmpi:
				if([operation isEqualToString:@"cmpwi"] || [operation isEqualToString:@"cmplwi"]) {
					XCTAssertFalse(instruction.l());
				} else {
					XCTAssertTrue(instruction.l());
				}

				XCTAssertEqualObjects(columns[3], conditionreg(instruction.crfD()));
				AssertEqualR(columns[4], instruction.rA());
				XCTAssertEqual([columns[5] hexInt], instruction.simm());
			break;

			case Operation::mtfsb0x:
			case Operation::mtfsb1x:
				AssertEqualOperationNameE(
					operation,
					instruction.operation == Operation::mtfsb0x ? @"mtfsb0x" : @"mtfsb1x",
					instruction);
				XCTAssertEqual([columns[3] intValue], instruction.crbD());
			break;

#define NoArg(x)	\
			case Operation::x:	\
				AssertEqualOperationName(operation, @#x);	\
			break;

			NoArg(isync);
			NoArg(sync);
			NoArg(eieio);
			NoArg(tlbia);

#undef NoArg

#define D(x)	\
			case Operation::x:	\
				AssertEqualOperationName(operation, @#x);	\
				AssertEqualR(columns[3], instruction.rD());	\
			break;

			D(mfcr);
			D(mfmsr);

#undef D

#define Shift(x)	\
			case Operation::x:	\
				AssertEqualOperationNameE(operation, @#x, instruction);	\
				AssertEqualR(columns[3], instruction.rA());				\
				AssertEqualR(columns[4], instruction.rS());				\
				AssertEqualR(columns[5], instruction.rB());				\
			break;

			Shift(slwx);
			Shift(srwx);
			Shift(srawx);

#undef Shift


#define ASsh(x)	\
			case Operation::x:											\
				AssertEqualOperationNameE(operation, @#x, instruction);	\
				AssertEqualR(columns[3], instruction.rA());				\
				AssertEqualR(columns[4], instruction.rS());				\
				XCTAssertEqual([columns[5] hexInt], instruction.sh<uint32_t>());	\
			break;

			ASsh(sliqx);
			ASsh(slliqx);
			ASsh(sraiqx);
			ASsh(sriqx);
			ASsh(srliqx);
			ASsh(srawix);

#undef ASsh

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

				AssertEqualR([columns lastObject], instruction.rS());

				if(columns.count > 4) {
					XCTAssertEqual([columns[3] hexInt], instruction.crm());
				}
			} break;


#define ArithImm(x) \
			case Operation::x: {	\
				AssertEqualOperationName(operation, @#x);	\
				AssertEqualR(columns[3], instruction.rD());	\
				AssertEqualR(columns[4], instruction.rA());	\
				XCTAssertEqual([columns[5] hexInt], instruction.simm());	\
			} break;

			ArithImm(dozi);
			ArithImm(addi);
			ArithImm(addic);
			ArithImm(addic_);
			ArithImm(addis);
			ArithImm(mulli);
			ArithImm(subfic);

#undef ArithImm

#define LogicImm(x) \
			case Operation::x: {	\
				AssertEqualOperationName(operation, @#x);	\
				AssertEqualR(columns[3], instruction.rA());	\
				AssertEqualR(columns[4], instruction.rS());	\
				XCTAssertEqual([columns[5] hexInt], instruction.uimm());	\
			} break;

			LogicImm(andi_);
			LogicImm(andis_);
			LogicImm(ori);
			LogicImm(oris);
			LogicImm(xori);
			LogicImm(xoris);

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
			ABCz(lhzx);
			ABCz(lhzux);
			ABCz(lhax);
			ABCz(lhaux);
			ABCz(lhbrx);
			ABCz(lwbrx);
			ABCz(lwarx);
			ABCz(eciwx);
			ABCz(lswx);
			ABCz(lwa);
			ABCz(lwaux);
			ABCz(lwax);

#undef ABCz

#define DABe(x)	\
			case Operation::x:	\
				AssertEqualOperationNameOE(operation, @#x, instruction);	\
				[self testABDInstruction:instruction columns:columns testZero:NO];	\
			break;

			DABe(subfcx);
			DABe(subfx);
			DABe(negx);
			DABe(subfex);
			DABe(subfzex);
			DABe(subfmex);
			DABe(dozx);
			DABe(absx);
			DABe(nabsx);
			DABe(addx);
			DABe(addcx);
			DABe(addex);
			DABe(addmex);
			DABe(addzex);
			DABe(mulhwx);
			DABe(mulhwux);
			DABe(mulx);
			DABe(mullwx);
			DABe(divx);
			DABe(divsx);
			DABe(divwux);
			DABe(divwx);
			DABe(lscbxx);

#undef DABe

#define ASB(x)	\
			case Operation::x:	\
				AssertEqualOperationNameE(operation, @#x, instruction);	\
				AssertEqualR(columns[3], instruction.rA());	\
				AssertEqualR(columns[4], instruction.rS());	\
				AssertEqualR(columns[5], instruction.rB());	\
			break;

			ASB(maskgx);
			ASB(maskirx);
			ASB(rribx);
			ASB(slex);
			ASB(sleqx);
			ASB(sllqx);
			ASB(slqx);
			ASB(sraqx);
			ASB(srex);
			ASB(sreax);
			ASB(sreqx);
			ASB(srlqx);
			ASB(srqx);

			ASB(andx);
			ASB(andcx);
			ASB(norx);
			ASB(eqvx);
			ASB(xorx);
			ASB(orcx);
			ASB(orx);
			ASB(nandx);

#undef ASB

#define SAB(x)	\
			case Operation::x:	\
				AssertEqualOperationName(operation, @#x);	\
				AssertEqualR(columns[3], instruction.rS());	\
				AssertEqualR(columns[4], instruction.rA(), false);	\
				AssertEqualR(columns[5], instruction.rB());	\
			break;

			SAB(ecowx);
			SAB(stbux);
			SAB(stbx);
			SAB(sthbrx);
			SAB(sthux);
			SAB(sthx);
			SAB(stswx);
			SAB(stwbrx);
			SAB(stwcx_);
			SAB(stwux);
			SAB(stwx);
			SAB(stdcx_);
			SAB(stdux);
			SAB(stdx);

#undef SAB

#define AS(x)	\
			case Operation::x:	\
				AssertEqualOperationNameE(operation, @#x, instruction);	\
				AssertEqualR(columns[3], instruction.rA());	\
				AssertEqualR(columns[4], instruction.rS());	\
			break;

			AS(cntlzwx);
			AS(extsbx);
			AS(extshx);
			AS(extswx);

#undef AS

#define fSAB(x)	\
			case Operation::x:	\
				AssertEqualOperationName(operation, @#x, instruction);	\
				AssertEqualFR(columns[3], instruction.frS());	\
				AssertEqualR(columns[4], instruction.rA(), false);	\
				AssertEqualR(columns[5], instruction.rB());	\
			break;

			fSAB(stfdx);
			fSAB(stfdux);
			fSAB(stfsx);
			fSAB(stfsux);
			fSAB(stfiwx);

#undef fSAB

#define fDAB(x)	\
			case Operation::x:	\
				AssertEqualOperationName(operation, @#x, instruction);	\
				AssertEqualFR(columns[3], instruction.frD());	\
				AssertEqualR(columns[4], instruction.rA(), false);	\
				AssertEqualR(columns[5], instruction.rB());	\
			break;

			fDAB(lfdux);
			fDAB(lfdx);
			fDAB(lfsux);
			fDAB(lfsx);

#undef fDAB

#define fDfAfB(x)	\
			case Operation::x:	\
				AssertEqualOperationNameE(operation, @#x, instruction);	\
				AssertEqualFR(columns[3], instruction.frD());	\
				AssertEqualFR(columns[4], instruction.frA(), false);	\
				AssertEqualFR(columns[5], instruction.frB());	\
			break;

			fDfAfB(faddx);
			fDfAfB(faddsx);
			fDfAfB(fsubx);
			fDfAfB(fsubsx);
			fDfAfB(fdivx);
			fDfAfB(fdivsx);

#undef fDfAfB

#define fDfB(x)	\
			case Operation::x:	\
				AssertEqualOperationNameE(operation, @#x, instruction);	\
				AssertEqualFR(columns[3], instruction.frD());	\
				AssertEqualFR(columns[4], instruction.frB());	\
			break;

			fDfB(fabsx);
			fDfB(fnabsx);
			fDfB(fnegx);
			fDfB(frsqrtex);
			fDfB(frspx);
			fDfB(fctiwx);
			fDfB(fctiwzx);
			fDfB(fctidx);
			fDfB(fctidzx);
			fDfB(fcfidx);
			fDfB(fresx);
			fDfB(fmrx);

#undef fDfB

#define fDfAfC(x)	\
			case Operation::x:	\
				AssertEqualOperationNameE(operation, @#x, instruction);	\
				AssertEqualFR(columns[3], instruction.frD());	\
				AssertEqualFR(columns[4], instruction.frA(), false);	\
				AssertEqualFR(columns[5], instruction.frC());	\
			break;

			fDfAfC(fmulx);
			fDfAfC(fmulsx);

#undef fDfAfC

#define fDfAfCfB(x)	\
			case Operation::x:	\
				AssertEqualOperationNameE(operation, @#x, instruction);	\
				AssertEqualFR(columns[3], instruction.frD());	\
				AssertEqualFR(columns[4], instruction.frA());	\
				AssertEqualFR(columns[5], instruction.frC());	\
				AssertEqualFR(columns[6], instruction.frB());	\
			break;

			fDfAfCfB(fnmaddx);
			fDfAfCfB(fnmaddsx);
			fDfAfCfB(fnmsubx);
			fDfAfCfB(fnmsubsx);
			fDfAfCfB(fmaddx);
			fDfAfCfB(fmaddsx);
			fDfAfCfB(fmsubx);
			fDfAfCfB(fmsubsx);
			fDfAfCfB(fselx);

#undef fDfAfBfC

#define DDA(x)	\
			case Operation::x: {	\
				AssertEqualOperationName(operation, @#x, instruction);	\
				AssertEqualR(columns[3], instruction.rD());	\
				XCTAssertEqualObjects(columns[4], offset(instruction));	\
			} break;

			DDA(lbz);
			DDA(lbzu);
			DDA(lmw);
			DDA(lwz);
			DDA(lwzu);
			DDA(lhz);
			DDA(lhzu);
			DDA(lha);
			DDA(lhau);

#undef DDA

#define fDDA(x)	\
			case Operation::x: {	\
				AssertEqualOperationName(operation, @#x, instruction);	\
				AssertEqualFR(columns[3], instruction.rD());	\
				XCTAssertEqualObjects(columns[4], offset(instruction));	\
			} break;

			fDDA(lfd);
			fDDA(lfdu);
			fDDA(lfs);
			fDDA(lfsu);

#undef fDDA

#define SDA(x)	\
			case Operation::x: {	\
				AssertEqualOperationName(operation, @#x, instruction);	\
				AssertEqualR(columns[3], instruction.rS());	\
				XCTAssertEqualObjects(columns[4], offset(instruction));	\
			} break;

			SDA(stb);
			SDA(stbu);
			SDA(sth);
			SDA(sthu);
			SDA(stmw);
			SDA(stw);
			SDA(stwu);

#undef SDA

#define fSDA(x)	\
			case Operation::x: {	\
				AssertEqualOperationName(operation, @#x, instruction);	\
				AssertEqualFR(columns[3], instruction.rS());	\
				XCTAssertEqualObjects(columns[4], offset(instruction));	\
			} break;

			fSDA(stfd);
			fSDA(stfdu);
			fSDA(stfs);
			fSDA(stfsu);

#undef fSDA

#define crfDS(x)	\
			case Operation::x:	\
				AssertEqualOperationName(operation, @#x, instruction);	\
				XCTAssertEqualObjects(columns[3], conditionreg(instruction.crfD()));	\
				XCTAssertEqualObjects(columns[4], conditionreg(instruction.crfS()));	\
			break;

			crfDS(mcrf);
			crfDS(mcrfs);

#undef crfDS

#define crfDfAfB(x)	\
			case Operation::x:	\
				AssertEqualOperationName(operation, @#x, instruction);	\
				XCTAssertEqualObjects(columns[3], conditionreg(instruction.crfD()));	\
				AssertEqualFR(columns[4], instruction.frA());	\
				AssertEqualFR(columns[5], instruction.frB());	\
			break;

			crfDfAfB(fcmpo);
			crfDfAfB(fcmpu);

#undef crfDfAfB

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

						XCTAssertEqual(decoded_destination, [[columns lastObject] hexInt]);
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
						expectedCR = condition(instruction.bi());
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

				const uint32_t decoded_destination =
					instruction.li() + (instruction.aa() ? 0 : address);
				XCTAssertEqual(decoded_destination, [columns[3] hexInt]);
			} break;
		}
	}
}

@end
