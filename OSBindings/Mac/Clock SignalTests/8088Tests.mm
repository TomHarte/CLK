//
//  8088Tests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 13/09/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <array>
#include <cassert>

#include <iostream>
#include <sstream>
#include <fstream>

#include "NSData+dataWithContentsOfGZippedFile.h"

#include "../../../InstructionSets/x86/Decoder.hpp"

namespace {

// The tests themselves are not duplicated in this repository;
// provide their real path here.
constexpr char TestSuiteHome[] = "/Users/tharte/Projects/ProcessorTests/8088/v1";

std::string to_hex(int value, int digits, bool with_suffix = true) {
	auto stream = std::stringstream();
	stream << std::setfill('0') << std::uppercase << std::hex << std::setw(digits);
	switch(digits) {
		case 2: stream << +uint8_t(value);	break;
		case 4: stream << +uint16_t(value);	break;
		default: stream << value;	break;
	}
	if (with_suffix) stream << 'h';
	return stream.str();
};

template <typename InstructionT>
std::string to_string(InstructionSet::x86::DataPointer pointer, const InstructionT &instruction, int offset_length, int immediate_length) {
	std::string operand;

	auto append = [](std::stringstream &stream, auto value, int length, const char *prefix) {
		switch(length) {
			case 0:
				if(!value) {
					break;
				}
				[[fallthrough]];
			case 2:
				// If asked to pretend the offset was originally two digits then either of: an unsigned
				// 8-bit value or a sign-extended 8-bit value as having been originally 8-bit.
				//
				// This kicks the issue of whether sign was extended appropriately to functionality tests.
				if(
					!(value & 0xff00) ||
					((value & 0xff80) == 0xff80) ||
					((value & 0xff80) == 0x0000)
				) {
					stream << prefix << to_hex(value, 2);
					break;
				}
				[[fallthrough]];
			default:
				stream << prefix << to_hex(value, 4);
				break;
		}
	};

	using Source = InstructionSet::x86::Source;
	const Source source = pointer.source<false>();
	switch(source) {
		// to_string handles all direct register names correctly.
		default:	return InstructionSet::x86::to_string(source, instruction.operation_size());

		case Source::Immediate: {
			std::stringstream stream;
			append(stream, instruction.operand(), immediate_length, "");
			return stream.str();
		}

		case Source::DirectAddress:
		case Source::Indirect:
		case Source::IndirectNoBase: {
			std::stringstream stream;

			if(!InstructionSet::x86::mnemonic_implies_data_size(instruction.operation)) {
				stream << InstructionSet::x86::to_string(instruction.operation_size()) << ' ';
			}

			Source segment = instruction.data_segment();
			if(segment == Source::None) {
				segment = pointer.default_segment();
				if(segment == Source::None) {
					segment = Source::DS;
				}
			}
			stream << InstructionSet::x86::to_string(segment, InstructionSet::x86::DataSize::None) << ':';

			stream << '[';
			bool addOffset = false;
			switch(source) {
				default: break;
				case Source::Indirect:
					stream << InstructionSet::x86::to_string(pointer.base(), data_size(instruction.address_size()));
					if(pointer.index() != Source::None) {
						stream << '+' << InstructionSet::x86::to_string(pointer.index(), data_size(instruction.address_size()));
					}
					addOffset = true;
				break;
				case Source::IndirectNoBase:
					stream << InstructionSet::x86::to_string(pointer.index(), data_size(instruction.address_size()));
					addOffset = true;
				break;
				case Source::DirectAddress:
					stream << to_hex(instruction.offset(), 4);
				break;
			}
			if(addOffset) {
				append(stream, instruction.offset(), offset_length, "+");
			}
			stream << ']';
			return stream.str();
		}
	}

	return operand;
};

}

@interface i8088Tests : XCTestCase
@end

@implementation i8088Tests

- (NSArray<NSString *> *)testFiles {
	NSString *path = [NSString stringWithUTF8String:TestSuiteHome];
	NSSet *allowList = [NSSet setWithArray:@[
//		@"F6.0.json.gz",
	]];

	// Unofficial opcodes; ignored for now.
	NSSet *ignoreList =
		[NSSet setWithObjects:
			@"60.json.gz",		@"61.json.gz",		@"62.json.gz",		@"63.json.gz",
			@"64.json.gz",		@"65.json.gz",		@"66.json.gz",		@"67.json.gz",
			@"68.json.gz",		@"69.json.gz",		@"6A.json.gz",		@"6B.json.gz",
			@"6C.json.gz",		@"6D.json.gz",		@"6E.json.gz",		@"6F.json.gz",

			@"82.0.json.gz",	@"82.1.json.gz",	@"82.2.json.gz",	@"82.3.json.gz",
			@"82.4.json.gz",	@"82.5.json.gz",	@"82.6.json.gz",	@"82.7.json.gz",

			@"C0.json.gz",		@"C1.json.gz",		@"C8.json.gz",		@"C9.json.gz",

			@"D0.6.json.gz",	@"D1.6.json.gz",	@"D2.6.json.gz",	@"D3.6.json.gz",
			@"D6.json.gz",

			@"F6.1.json.gz",	@"F7.1.json.gz",
			@"FF.7.json.gz",

			nil
		];

	NSArray<NSString *> *files = [[NSFileManager defaultManager] contentsOfDirectoryAtPath:path error:nil];
	files = [files filteredArrayUsingPredicate:[NSPredicate predicateWithBlock:^BOOL(NSString* evaluatedObject, NSDictionary<NSString *,id> *) {
		if(allowList.count && ![allowList containsObject:[evaluatedObject lastPathComponent]]) {
			return NO;
		}
		if([ignoreList containsObject:[evaluatedObject lastPathComponent]]) {
			return NO;
		}
		return [evaluatedObject hasSuffix:@"json.gz"];
	}]];

	NSMutableArray<NSString *> *fullPaths = [[NSMutableArray alloc] init];
	for(NSString *file in files) {
		[fullPaths addObject:[path stringByAppendingPathComponent:file]];
	}

	return [fullPaths sortedArrayUsingSelector:@selector(compare:)];
}

- (NSString *)toString:(const InstructionSet::x86::Instruction<false> &)instruction offsetLength:(int)offsetLength immediateLength:(int)immediateLength {
	// Form string version, compare.
	std::string operation;
	using Operation = InstructionSet::x86::Operation;
	using Repetition = InstructionSet::x86::Repetition;
	using Source = InstructionSet::x86::Source;

	switch(instruction.repetition()) {
		case Repetition::None: break;
		case Repetition::RepE:
			switch(instruction.operation) {
				default:	operation += "repe ";	break;

				case Operation::MOVS:
				case Operation::STOS:
				case Operation::LODS:
					operation += "rep ";
				break;
			}
		break;
		case Repetition::RepNE: operation += "repne ";	break;
	}

	operation += to_string(instruction.operation, instruction.operation_size());

	// Deal with a few special cases up front.
	switch(instruction.operation) {
		default: {
			const int operands = num_operands(instruction.operation);
			const bool displacement = has_displacement(instruction.operation);
			operation += " ";
			if(operands > 1) {
				operation += to_string(instruction.destination(), instruction, offsetLength, immediateLength);
				operation += ", ";
			}
			if(operands > 0) {
				operation += to_string(instruction.source(), instruction, offsetLength, immediateLength);
			}
			if(displacement) {
				operation += to_hex(instruction.displacement(), 2);
			}
		} break;

		case Operation::CALLfar:
		case Operation::JMPfar: {
			operation += " 0x";
			operation += to_hex(instruction.segment(), 4, false);
			operation += ":0x";
			operation += to_hex(instruction.offset(), 4, false);
		} break;

		// Rolls and shifts list eCX as a source on the understanding that everyone knows that rolls and shifts
		// use CL even when they're shifting or rolling a word-sized quantity.
		case Operation::RCL:	case Operation::RCR:
		case Operation::ROL:	case Operation::ROR:
		case Operation::SAL:	case Operation::SAR:
		case Operation::SHR:
			const int operands = num_operands(instruction.operation);
			const bool displacement = has_displacement(instruction.operation);
			operation += " ";
			if(operands > 1) {
				operation += to_string(instruction.destination(), instruction, offsetLength, immediateLength);
			}
			if(operands > 0) {
				switch(instruction.source().source()) {
					case Source::None:	break;
					case Source::eCX:	operation += ", cl"; break;
					case Source::Immediate:
						// Providing an immediate operand of 1 is a little future-proofing by the decoder; the '1'
						// is actually implicit on a real 8088. So omit it.
						if(instruction.operand() == 1) break;
						[[fallthrough]];
					default:
						operation += ", ";
						operation += to_string(instruction.source(), instruction, offsetLength, immediateLength);
					break;
				}
			}
			if(displacement) {
				operation += to_hex(instruction.displacement(), 2);
			}
		break;
	}

	return [NSString stringWithUTF8String:operation.c_str()];
}

- (bool)applyDecodingTest:(NSDictionary *)test file:(NSString *)file {
	using Decoder = InstructionSet::x86::Decoder<InstructionSet::x86::Model::i8086>;
	Decoder decoder;

	// Build a vector of the instruction bytes; this makes manual step debugging easier.
	NSArray<NSNumber *> *encoding = test[@"bytes"];
	std::vector<uint8_t> data;
	data.reserve(encoding.count);
	for(NSNumber *number in encoding) {
		data.push_back([number intValue]);
	}
	auto hex_instruction = [&]() -> NSString * {
		NSMutableString *hexInstruction = [[NSMutableString alloc] init];
		for(uint8_t byte: data) {
			[hexInstruction appendFormat:@"%02x ", byte];
		}
		return hexInstruction;
	};

	const auto decoded = decoder.decode(data.data(), data.size());
	XCTAssert(
		decoded.first == [encoding count],
		"Wrong length of instruction decoded for %@ — decoded %d rather than %lu from %@; file %@",
			test[@"name"],
			decoded.first,
			(unsigned long)[encoding count],
			hex_instruction(),
			file
	);

	// The decoder doesn't preserve the original offset length, which makes no functional difference but
	// does affect the way that offsets are printed in the test set.
	NSSet<NSString *> *decodings = [NSSet setWithObjects:
		[self toString:decoded.second offsetLength:4 immediateLength:4],
		[self toString:decoded.second offsetLength:2 immediateLength:4],
		[self toString:decoded.second offsetLength:0 immediateLength:4],
		[self toString:decoded.second offsetLength:4 immediateLength:2],
		[self toString:decoded.second offsetLength:2 immediateLength:2],
		[self toString:decoded.second offsetLength:0 immediateLength:2],
		nil];

	auto compare_decoding = [&](NSString *name) -> bool {
		return [decodings containsObject:name];
	};


	bool isEqual = compare_decoding(test[@"name"]);

	// Attempt clerical reconciliation:
	//
	// TEMPORARY HACK: the test set incorrectly states 'bp+si' whenever it means 'bp+di'.
	// Though it also uses 'bp+si' correctly when it means 'bp+si'. Until fixed, take
	// a pass on potential issues there.
	//
	// SEPARATELY: The test suite retains a distinction between SHL and SAL, which the decoder doesn't. So consider that
	// a potential point of difference.
	bool adjust_si = true;
	bool adjust_sal = false;
	while(!isEqual && (adjust_sal || adjust_si)) {
		NSString *alteredName = test[@"name"];

		if(adjust_si) {
			alteredName = [alteredName stringByReplacingOccurrencesOfString:@"bp+si" withString:@"bp+di"];
		}
		if(adjust_sal) {
			alteredName = [alteredName stringByReplacingOccurrencesOfString:@"shl" withString:@"sal"];
		}

		isEqual = compare_decoding(alteredName);
		adjust_sal ^= adjust_si;
		adjust_si ^= true;
	}

	XCTAssert(isEqual, "%@ doesn't match %@ or similar, was %@ within %@", test[@"name"], [decodings anyObject], hex_instruction(), file);

	// Repetition, to allow for easy breakpoint placement.
	if(!isEqual) {
		// Repeat operand conversions, for debugging.
		Decoder decoder;
		const auto instruction = decoder.decode(data.data(), data.size());
		const InstructionSet::x86::Source sources[] = {
			instruction.second.source().source<false>(),
			instruction.second.destination().source<false>(),
		};
		(void)sources;

		const auto destination = instruction.second.destination();
		std::cout << to_string(destination, instruction.second, 4, 2);
		const auto source = instruction.second.source();
		std::cout << to_string(source, instruction.second, 4, 2);
		return false;
	}

	// Known existing failures versus the provided 8088 disassemblies:
	//
	//	* quite a lot of instances similar to [bp+si+1DEAh] being decoded as [bp+di+1DEAh]; there is an error in the
	//		test set where si will appear where di should, which obscures potential problems here;
	//	* shifts that have been given a literal source of '1' shouldn't print it; that's a figment ofd this encoding;
	//	* similarly, shifts should print cl as a source rather than cx even when shifting a word;
	//	* ... and in/out should always use an 8-bit source;
	//	* ... and RCL ditto is SP, CL rather than CX;
	//	* rep/etc shouldn't be printed on instructions that don't allow it, and may be just 'rep' rather than 'repe';
	//	* I strongly suspect I'm dealing with ESC operands incorrectly;
	//	* possibly LEA implies a word operand?;
	//	* D1.6, which is undocumented, apparently maps to SETMO — add that;
	//	* 36 8e 6b 7c should mirror to mov cs, word ss:[bp+di+7Ch]; it's literally mov gs, word ss:[bp+di+7Ch] so the
	//		decoder rejects it for 16-bit mode;
	//	* I'm using 'sal' whereas the test set is using 'shl';
	//	* logging of far jumps and calls is way off; add a special case for printing segment:offset;
	//	* CALLrel seems to lose a byte of argument? e8 4b 9c is printed as just call 4Bh rather than call 9C4Bh;
	//	* JMPrel seems to have the same issue;
	//	* my RETnear seems to fill in source, but not destination?
	//	* there are still some false sign extensions going on, e.g. 26 83 8c 47 32 ad produces
	//		`or word es:[si+3247h], FFADh` when the second argument should just be `ADh`;
	//	* the decoder provides int3 as int with an operand of 3, so a special-case printing is necessary there;
	//	* the provided dissassemblies sometimes include an offset of 00h, which won't be printed by this code;
	//	* e5 4b is decoded as in ax, word ds:[0000h] rather than in ax, 4Bh;
	//	* 36 f6 04 4b is decoded as the nonsensical test byte ss:[si], byte ss:[si] rather than test byte ss:[si], 4Bh;
	//	* a1 4b 50 is decoded as `mov ax, word ds:[0000h]` rather than `mov ax, word ds:[504Bh]`, and other tests seem
	//		similarly affected;
	//	* `a0 4b 50`, `26 a2 d2 80`, also drop the suffix somewhere (or, actually, fail subsequently to surface it);
	//	* POPs in general don't realise to print the other possible operand;
	//	* c4 4b 9c somehow decodes as a _dword_ version of LES, and LDS seems similarly affected;
	//


	return true;
}

- (void)testDecoding {
	NSMutableSet<NSString *> *failures = [[NSMutableSet alloc] init];
	NSArray<NSString *> *testFiles = [self testFiles];

	for(NSString *file in testFiles) {
		NSData *data = [NSData dataWithContentsOfGZippedFile:file];
		NSArray<NSDictionary *> *testsInFile = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
		NSUInteger successes = 0;
		for(NSDictionary *test in testsInFile) {
			// A single failure per instruction is fine.
			if(![self applyDecodingTest:test file:file]) {
				[failures addObject:file];
				break;
			}
			++successes;
		}
		if(successes != [testsInFile count]) {
			NSLog(@"Failed after %ld successes", successes);
		}
	}

	NSLog(@"%ld failures out of %ld tests: %@", failures.count, testFiles.count, [[failures allObjects] sortedArrayUsingSelector:@selector(caseInsensitiveCompare:)]);
}

@end
