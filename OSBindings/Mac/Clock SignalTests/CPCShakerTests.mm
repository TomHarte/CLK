//
//  CPCShakerTests.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 28/06/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#import <XCTest/XCTest.h>

#include <array>
#include <cassert>

#include "CSL.hpp"
#include "AmstradCPC.hpp"
#include "../../../Analyser/Static/AmstradCPC/Target.hpp"
#include "../../../Machines/AmstradCPC/Keyboard.hpp"
#include "CSROMFetcher.hpp"
#include "TimedMachine.hpp"
#include "MediaTarget.hpp"
#include "KeyboardMachine.hpp"
#include "MachineForTarget.hpp"

struct SSMDelegate: public AmstradCPC::Machine::SSMDelegate {
	void perform(uint16_t code) {
		NSLog(@"SSM %04x", code);
	}
};

//
// Runs a local capture of the test cases found at https://shaker.logonsystem.eu
//
@interface CPCShakerTests : XCTestCase
@end

@implementation CPCShakerTests {}

- (void)testCSLPath:(NSString *)path name:(NSString *)name {
	using namespace Storage::Automation;
	const auto steps = CSL::parse([[path stringByAppendingPathComponent:name] UTF8String]);

	SSMDelegate ssm_delegate;

	std::unique_ptr<Machine::DynamicMachine> lazy_machine;
	CSL::KeyDelay key_delay;
	using Target = Analyser::Static::AmstradCPC::Target;
	Target target;
	target.catch_ssm_codes = true;
	target.model = Target::Model::CPC6128;

	const auto machine = [&]() -> Machine::DynamicMachine& {
		if(!lazy_machine) {
			Machine::Error error;
			lazy_machine = Machine::MachineForTarget(&target, CSROMFetcher(), error);
			reinterpret_cast<AmstradCPC::Machine *>(lazy_machine->raw_pointer())
				->set_ssm_delegate(&ssm_delegate);
		}
		return *lazy_machine;
	};
	const auto delay = [&](uint64_t micros) {
		machine().timed_machine()->run_for((double)micros / 1'000'000.0);
	};

	using Type = CSL::Instruction::Type;
	for(const auto &step: steps) {
		switch(step.type) {
			case Type::Version:
				if(std::get<std::string>(step.argument) != "1.0") {
					XCTAssert(false, "Unrecognised file version");
				}
			break;

			case Type::CRTCSelect: {
				switch(std::get<uint64_t>(step.argument)) {
					default: break;
					case 0:	target.crtc_type = Target::CRTCType::Type0;	break;
					case 1:	target.crtc_type = Target::CRTCType::Type1;	break;
					case 2:	target.crtc_type = Target::CRTCType::Type2;	break;
					case 3:	target.crtc_type = Target::CRTCType::Type3;	break;
				}
			} break;

			case Type::Reset:
				// Temporarily a no-op.
			break;

			case Type::Wait:
				delay(std::get<uint64_t>(step.argument));
			break;

			case Type::DiskInsert: {
				const auto &disk = std::get<CSL::DiskInsert>(step.argument);
				XCTAssertEqual(disk.drive, 0);	// Only drive 0 is supported for now.

				NSString *diskName = [NSString stringWithUTF8String:disk.file.c_str()];
				NSString *const diskPath =
					[[NSBundle bundleForClass:[self class]]
						pathForResource:diskName ofType:nil inDirectory:@"Shaker"];

				XCTAssertNotNil(diskPath);

				const auto media = Analyser::Static::GetMedia(diskPath.UTF8String);
				machine().media_target()->insert_media(media);
			} break;

			case Type::KeyDelay:
				key_delay = std::get<CSL::KeyDelay>(step.argument);
			break;

			case Type::KeyOutput: {
				auto &key_target = *machine().keyboard_machine();

				const auto &events = std::get<std::vector<CSL::KeyEvent>>(step.argument);
				bool last_down = false;
				for(const auto &event: events) {
					// Apply the interpress delay before if this is a second consecutive press;
					// if this is a release then apply the regular key delay.
					if(event.down && !last_down) {
						delay(key_delay.interpress_delay);
					} else if(!event.down) {
						delay(key_delay.press_delay);
					}

					key_target.set_key_state(event.key, event.down);
					last_down = event.down;

					// If this was the release of a carriage return, wait some more after release.
					if(key_delay.carriage_return_delay && (event.key == AmstradCPC::Key::KeyEnter || event.key == AmstradCPC::Key::KeyReturn)) {
						delay(*key_delay.carriage_return_delay);
					}
				}
			} break;

			case Type::LoadCSL:
				// Quick fix: just recurse.
				[self
					testCSLPath:path
					name:
						[NSString stringWithUTF8String:
							(std::get<std::string>(step.argument) + ".csl").c_str()
						]];
			break;

			default:
				XCTAssert(false, "Unrecognised command: %d", step.type);
			break;
		}
	}
}

- (void)testModulePath:(NSString *)path name:(NSString *)name {
	NSString *basePath =
		[[NSBundle bundleForClass:[self class]]
			pathForResource:@"Shaker"
			ofType:nil];
	[self testCSLPath:[basePath stringByAppendingPathComponent:path] name:name];
}

- (void)testModuleA {	[self testModulePath:@"MODULE A" name:@"SHAKE26A-0.CSL"];	}
- (void)testModuleB {	[self testModulePath:@"MODULE B" name:@"SHAKE26B-0.CSL"];	}
- (void)testModuleC {	[self testModulePath:@"MODULE C" name:@"SHAKE26C-0.CSL"];	}
- (void)testModuleD {	[self testModulePath:@"MODULE D" name:@"SHAKE26D-0.CSL"];	}
- (void)testModuleE {	[self testModulePath:@"MODULE E" name:@"SHAKE26E-0.CSL"];	}

@end
