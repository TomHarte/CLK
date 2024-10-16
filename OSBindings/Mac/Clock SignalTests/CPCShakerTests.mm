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
#include "../../../Outputs/ScanTarget.hpp"
#include "CSROMFetcher.hpp"
#include "TimedMachine.hpp"
#include "MediaTarget.hpp"
#include "KeyboardMachine.hpp"
#include "MachineForTarget.hpp"

struct ScanTarget: public Outputs::Display::ScanTarget {
	void set_modals(Modals modals) override {
		modals_ = modals;
	}
	Scan *begin_scan() override {
		return &scan_;
	}
	uint8_t *begin_data(size_t, size_t) override {
		return data_.data();
	}


	void end_scan() override {
		// Empirical, CPC-specific observation: x positions end up
		// being multiplied by 61 compared to a 1:1 pixel sampling at
		// the CPC's highest resolution.
		const int WidthDivider = 61;

		const int src_pixels = scan_.end_points[1].data_offset - scan_.end_points[0].data_offset;
		const int dst_pixels = (scan_.end_points[1].x - scan_.end_points[0].x) / WidthDivider;

		const auto x1 = scan_.end_points[0].x / WidthDivider;
		const auto x2 = scan_.end_points[1].x / WidthDivider;

		uint8_t *const line = &raw_image_[line_ * ImageWidth];
		if(x_ < x1) {
			std::fill(&line[x_], &line[x1], 0);
		}

		if(x2 != x1) {
			const int step = (src_pixels << 16) / dst_pixels;
			int position = 0;

			for(int x = x1; x < x2; x++) {
				line[x] = data_[position >> 16];
				position += step;
			}
		}
		x_ = x2;
	}
	void announce(Event event, bool, const Scan::EndPoint &, uint8_t) override {
		switch(event) {
			case Event::EndHorizontalRetrace: {
				if(line_ == ImageHeight - 1) break;

				if(x_ < ImageWidth) {
					uint8_t *const line = &raw_image_[line_ * ImageWidth];
					std::fill(&line[x_], &line[ImageWidth], 0);
				}

				++line_;
				x_ = 0;
			} break;
			case Event::EndVerticalRetrace:
				std::fill(&raw_image_[line_ * ImageWidth], &raw_image_[ImageHeight * ImageWidth], 0);
				line_ = 0;
				x_ = 0;
			break;
			default: break;
		}
	}

	NSBitmapImageRep *image_representation() {
		NSBitmapImageRep *const result =
			[[NSBitmapImageRep alloc]
				initWithBitmapDataPlanes:NULL
				pixelsWide:ImageWidth
				pixelsHigh:ImageHeight
				bitsPerSample:8
				samplesPerPixel:4
				hasAlpha:YES
				isPlanar:NO
				colorSpaceName:NSDeviceRGBColorSpace
				bytesPerRow:4 * ImageWidth
				bitsPerPixel:0];
		uint8_t *const data = result.bitmapData;

		for(int c = 0; c < ImageWidth * ImageHeight; c++) {
			data[c * 4 + 0] = ((raw_image_[c] >> 4) & 3) * 127;
			data[c * 4 + 1] = ((raw_image_[c] >> 2) & 3) * 127;
			data[c * 4 + 2] = ((raw_image_[c] >> 0) & 3) * 127;
			data[c * 4 + 3] = 0xff;
		}

		return result;
	}


private:
	Modals modals_;
	Scan scan_;
	std::array<uint8_t, 2048> data_;
	int line_ = 0;
	int x_ = 0;

	static constexpr int ImageWidth = 914;
	static constexpr int ImageHeight = 312;
	std::array<uint8_t, ImageWidth*ImageHeight> raw_image_;
};

struct SSMDelegate: public AmstradCPC::Machine::SSMDelegate {
	SSMDelegate(ScanTarget &scan_target) : scan_target_(scan_target) {
		temp_dir_ = NSTemporaryDirectory();
		NSLog(@"Outputting to %@", temp_dir_);
	}

	void set_crtc(int number) {
		crtc_ = number;
	}

	void perform(uint16_t code) {
		if(!code) {
			// A code of 0000 is supposed to end a wait0000 command; at present
			// there seem to be no wait0000 commands to unblock.
			return;
		}

		NSData *const data =
			[scan_target_.image_representation() representationUsingType:NSPNGFileType properties:@{}];
		NSString *const name = [temp_dir_ stringByAppendingPathComponent:[NSString stringWithFormat:@"CLK_%d_%04x.png", crtc_, code]];
		[data
			writeToFile:name
			atomically:NO];
		NSLog(@"Wrote %@", name);
	}

private:
	ScanTarget &scan_target_;
	NSString *temp_dir_;
	int crtc_ = 0;
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

	ScanTarget scan_target;
	SSMDelegate ssm_delegate(scan_target);

	std::unique_ptr<Machine::DynamicMachine> lazy_machine;
	CSL::KeyDelay key_delay;
	using Target = Analyser::Static::AmstradCPC::Target;
	Target target;
	target.catch_ssm_codes = true;
	target.model = Target::Model::CPC6128;

	NSString *diskPath;
	const auto machine = [&]() -> Machine::DynamicMachine& {
		if(!lazy_machine) {
			Machine::Error error;
			lazy_machine = Machine::MachineForTarget(&target, CSROMFetcher(), error);
			static_cast<AmstradCPC::Machine *>(lazy_machine->raw_pointer())
				->set_ssm_delegate(&ssm_delegate);
			lazy_machine->scan_producer()->set_scan_target(&scan_target);

			if(diskPath) {
				const auto media = Analyser::Static::GetMedia(diskPath.UTF8String);
				lazy_machine->media_target()->insert_media(media);
			}
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
				const auto argument = static_cast<int>(std::get<uint64_t>(step.argument));
				switch(argument) {
					default:
						NSLog(@"Unrecognised CRTC type %d", argument);
					break;
					case 0:	target.crtc_type = Target::CRTCType::Type0;	break;
					case 1:	target.crtc_type = Target::CRTCType::Type1;	break;
					case 2:	target.crtc_type = Target::CRTCType::Type2;	break;
					case 3:	target.crtc_type = Target::CRTCType::Type3;	break;
				}
				ssm_delegate.set_crtc(argument);
			} break;

			case Type::Reset:
				lazy_machine.reset();
			break;

			case Type::Wait:
				delay(std::get<uint64_t>(step.argument));
			break;

			case Type::DiskInsert: {
				const auto &disk = std::get<CSL::DiskInsert>(step.argument);
				XCTAssertEqual(disk.drive, 0);	// Only drive 0 is supported for now.

				NSString *diskName = [NSString stringWithUTF8String:disk.file.c_str()];
				diskPath =
					[[NSBundle bundleForClass:[self class]]
						pathForResource:diskName ofType:nil inDirectory:@"Shaker"];
				XCTAssertNotNil(diskPath);

				if(lazy_machine) {
					const auto media = Analyser::Static::GetMedia(diskPath.UTF8String);
					machine().media_target()->insert_media(media);
				}
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

- (void)testModuleA {
	[self testModulePath:@"MODULE A" name:@"SHAKE26A-0.CSL"];
//	[self testModulePath:@"MODULE A" name:@"SHAKE26A-1.CSL"];
//	[self testModulePath:@"MODULE A" name:@"SHAKE26A-2.CSL"];
//	[self testModulePath:@"MODULE A" name:@"SHAKE26A-3.CSL"];
//	[self testModulePath:@"MODULE A" name:@"SHAKE26A-4.CSL"];
}
- (void)testModuleB {
	[self testModulePath:@"MODULE B" name:@"SHAKE26B-0.CSL"];
//	[self testModulePath:@"MODULE B" name:@"SHAKE26B-1.CSL"];
//	[self testModulePath:@"MODULE B" name:@"SHAKE26B-2.CSL"];
//	[self testModulePath:@"MODULE B" name:@"SHAKE26B-3.CSL"];
//	[self testModulePath:@"MODULE B" name:@"SHAKE26B-4.CSL"];
}
- (void)testModuleC {
	[self testModulePath:@"MODULE C" name:@"SHAKE26C-0.CSL"];
//	[self testModulePath:@"MODULE C" name:@"SHAKE26C-1.CSL"];
//	[self testModulePath:@"MODULE C" name:@"SHAKE26C-2.CSL"];
//	[self testModulePath:@"MODULE C" name:@"SHAKE26C-3.CSL"];
//	[self testModulePath:@"MODULE C" name:@"SHAKE26C-4.CSL"];
}
- (void)testModuleD {
	[self testModulePath:@"MODULE D" name:@"SHAKE26D-0.CSL"];
//	[self testModulePath:@"MODULE D" name:@"SHAKE26D-1.CSL"];
//	[self testModulePath:@"MODULE D" name:@"SHAKE26D-2.CSL"];
//	[self testModulePath:@"MODULE D" name:@"SHAKE26D-3.CSL"];
//	[self testModulePath:@"MODULE D" name:@"SHAKE26D-4.CSL"];
}
- (void)testModuleE {
	[self testModulePath:@"MODULE E" name:@"SHAKE26E-0.CSL"];
//	[self testModulePath:@"MODULE E" name:@"SHAKE26E-1.CSL"];
//	[self testModulePath:@"MODULE E" name:@"SHAKE26E-2.CSL"];
//	[self testModulePath:@"MODULE E" name:@"SHAKE26E-3.CSL"];
//	[self testModulePath:@"MODULE E" name:@"SHAKE26E-4.CSL"];
}

@end
