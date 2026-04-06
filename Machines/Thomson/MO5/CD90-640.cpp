//
//  CD90-640.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#include "CD90-640.hpp"

using namespace Thomson;

CD90_640::CD90_640() : WD::WD1770(P1770) {
	// 325 is a peculiar RPM, but seems to match a spin-up test in the disk ROM that polls the WD1770's status register
	// index hole bit and counts time. Furthermore there are other machines with unusual RPMs. Could definitely
	// still just imply an issue elsewhere in the emulator though.
	emplace_drives(2, 8'000'000, 325, 2);

	//
	// On "325 RPM":
	//
	// The DOS ROM verifies that a disk is present and spinning before proceeding.
	// Part of that test is of spin speed.
	//
	// Specifically, with X = 0xa7d0 it performs:
	//
	//		;
	//		; Wait for index hole set.
	//		;
	//		a29b	LDA     ,X			; 4 µs
	//		a29d	ANDA    #$02		; 2 µs
	//		a29f	BEQ     ZA29B		; 3 µs
	//
	//		;
	//		; i.e. start of track falls about here
	//		;
	//
	//		;
	//		; Fixed delay...
	//		;
	//		a2a1	LDY     #M09C4		; 4 µs
	//		a2a5	DEY					; 4 µs
	//		a2a7	BNE     ZA2A5		; 3 µs	= net delay of 4 + 9C4*7 = 17,504 µs
	//
	//		a2a9	PSHS    CC			; 6 µs
	//		a2ab	SEIF				; 3 µs
	//
	//		;
	//		; Count time until index hole is set again
	//		;
	//		a2ad	LDA     ,X			; 4 µs, read WD status
	//		a2af	INY					; 4 µs
	//		a2b1	ANDA    #$02		; 2 µs
	//		a2b3	BEQ     ZA2AD		; 3 µs
	//
	//		a2b5	PULS    CC
	//
	//		;
	//		; Check whether rotation speed was within bounds
	//		;
	//		a2b7	CMPY    #M311B
	//		a2bb	BMI     ZA2C7
	//		a2bd	CMPY    #M3357
	//		a2c1	BPL     ZA2C7
	//
	//		[test passed code here, test failed code from a2c7]
	//
	// If I've disassembled that correctly, then the process is:
	//
	//	(1) wait for index hole, with up to 9 µs of latency;
	//	(2) spin in a loop for 17,504 µs;
	//	(3) do minor stack/flag work for 9 µs;
	//	(4) count time from here until next index hole, ending with Y = time/13;
	//	(5) test that that is within the bounds 0x311b, 0x3357.
	//
	// Ignoring minor potential latencies in loop exits, that's:
	//
	//	(1) wait for index hole;
	//	(2) spend 17,513 µs doing something else;
	//	(3) get Y = [µs from there to next index hole] / 13.
	//
	// i.e. it's a test that the rotation it samples takes n µs, where:
	//
	//	13 * 0x311b < n - 17,513 < 13 * 0x3357
	//	13 * 12,571 + 17,513 < n < 13 * 13,143 + 17,513
	//	180,936 < n < 188,372
	//
	// i.e.
	//
	//	~331 >= RPM >= ~319 RPM.
	//
	// So 325 RPM is a really weird number but I can't currently say why it should be
	// wrong rather than merely unexpected. So here it is.

}

uint8_t CD90_640::control() {
	return control_;	// Possibly only b7 is loaded?
}

void CD90_640::set_control(const uint8_t value) {
	control_ = value;

	// Following along from the schematic in https://github.com/OlivierP-To8/CD90-640/ :
	//
	//	The 74LS123 on the second page is fed with:
	//
	//		D0 = external data line 7
	//		D1 = line 0
	//		D2 = line 1
	//		D3 = line 2
	//
	//	It then routes:
	//
	//		Q0 = DDEN select
	//		Q1 = side sleect
	//		Q2, Q3 = drive selects
	//
	set_is_double_density(!(value & 0x80));
	for_all_drives( [&](Storage::Disk::Drive &drive, size_t) {
		drive.set_head(value & 1);
	});
	set_drive((value >> 1) & 3);
}

void CD90_640::set_motor_on(const bool motor) {
	for_all_drives( [&](Storage::Disk::Drive &drive, size_t) {
		drive.set_motor_on(motor);
	});
}

void CD90_640::set_activity_observer(Activity::Observer *const observer) {
	for_all_drives([observer] (Storage::Disk::Drive &drive, size_t index) {
		drive.set_activity_observer(observer, "Drive " + std::to_string(index+1), true);
	});
}

// TODO: the code below is fairly boilerplate; can it be factored out?

void CD90_640::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, const size_t drive) {
	get_drive(drive).set_disk(disk);
}

const Storage::Disk::Disk *CD90_640::disk(const std::string &name) {
	const Storage::Disk::Disk *result = nullptr;
	for_all_drives( [&](Storage::Disk::Drive &drive, size_t) {
		const auto disk = drive.disk();
		if(disk && disk->represents(name)) {
			result = disk;
		}
	});
	return result;
}
