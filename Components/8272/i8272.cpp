//
//  i8272.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "i8272.hpp"

#include <cstdio>

using namespace Intel;

namespace {
const uint8_t StatusRQM = 0x80;	// Set: ready to send or receive from processor.
const uint8_t StatusDIO = 0x40;	// Set: data is expected to be taken from the 8272 by the processor.
const uint8_t StatusNDM = 0x20;	// Set: the execution phase of a data transfer command is ongoing and DMA mode is disabled.
const uint8_t StatusCB = 0x10;	// Set: the FDC is busy.
//const uint8_t StatusD3B = 0x08;	// Set: drive 3 is seeking.
//const uint8_t StatusD2B = 0x04;	// Set: drive 2 is seeking.
//const uint8_t StatusD1B = 0x02;	// Set: drive 1 is seeking.
//const uint8_t StatusD0B = 0x01;	// Set: drive 0 is seeking.
}

i8272::i8272(Cycles clock_rate, int clock_rate_multiplier, int revolutions_per_minute) :
	Storage::Disk::MFMController(clock_rate, clock_rate_multiplier, revolutions_per_minute),
	main_status_(StatusRQM),
	interesting_event_mask_((int)Event8272::CommandByte),
	resume_point_(0),
	delay_time_(0),
	status_{0, 0, 0, 0} {
	posit_event((int)Event8272::CommandByte);
}

void i8272::run_for(Cycles cycles) {
	Storage::Disk::MFMController::run_for(cycles);

	// check for an expired timer
	if(delay_time_ > 0) {
		if(cycles.as_int() >= delay_time_) {
			delay_time_ = 0;
			posit_event((int)Event8272::Timer);
		} else {
			delay_time_ -= cycles.as_int();
		}
	}

	// update seek status of any drives presently seeking
	for(int c = 0; c < 4; c++) {
		if(drives_[c].phase == Drive::Seeking) {
			drives_[c].step_rate_counter += cycles.as_int();
			int steps = drives_[c].step_rate_counter / (8000 * step_rate_time_);
			drives_[c].step_rate_counter %= (8000 * step_rate_time_);
			while(steps--) {
				if(
					(drives_[c].target_head_position == (int)drives_[c].head_position) ||
					(drives_[c].drive->get_is_track_zero() && drives_[c].target_head_position == -1)) {
					drives_[c].phase = Drive::CompletedSeeking;
					if(drives_[c].target_head_position == -1) drives_[c].head_position = 0;
					main_status_ &= ~(1 << c);
				} else {
					int direction = (drives_[c].target_head_position < drives_[c].head_position) ? -1 : 1;
					drives_[c].drive->step(direction);
					drives_[c].head_position += direction;
				}
			}
		}
	}
}

void i8272::set_register(int address, uint8_t value) {
	// don't consider attempted sets to the status register
	if(!address) return;

	// if not ready for commands, do nothing
	if(!(main_status_ & StatusRQM)) return;

	// accumulate latest byte in the command byte sequence
	command_.push_back(value);
	posit_event((int)Event8272::CommandByte);
}

uint8_t i8272::get_register(int address) {
	if(address) {
		printf("8272 get data\n");

		if(result_stack_.empty()) return 0xff;
		uint8_t result = result_stack_.back();
		result_stack_.pop_back();
		if(result_stack_.empty()) posit_event((int)Event8272::ResultEmpty);

		return result;
	} else {
//		printf("8272 get main status\n");
		return main_status_;
	}
}

void i8272::set_disk(std::shared_ptr<Storage::Disk::Disk> disk, int drive) {
	if(drive < 4 && drive >= 0) {
		drives_[drive].drive->set_disk(disk);
	}
}

#define BEGIN_SECTION()	switch(resume_point_) { default:
#define END_SECTION()	}

#define WAIT_FOR_EVENT(mask)	resume_point_ = __LINE__; interesting_event_mask_ = (int)mask; return; case __LINE__:

#define PASTE(x, y) x##y
#define CONCAT(x, y) PASTE(x, y)

#define FIND_HEADER()	\
	CONCAT(find_header, __LINE__): WAIT_FOR_EVENT((int)Event::Token | (int)Event::IndexHole); \
	if(event_type == (int)Event::IndexHole) index_hole_limit_--;	\
	else if(get_latest_token().type == Token::ID) goto CONCAT(header_found, __LINE__);	\
	\
	if(index_hole_limit_) goto CONCAT(find_header, __LINE__);	\
	CONCAT(header_found, __LINE__):	0;\

#define FIND_DATA()	\
	CONCAT(find_data, __LINE__): WAIT_FOR_EVENT((int)Event::Token | (int)Event::IndexHole); \
	if(event_type == (int)Event::Token && get_latest_token().type != Token::Data) goto CONCAT(find_data, __LINE__);

#define READ_HEADER()	\
	distance_into_section_ = 0;	\
	set_data_mode(Reading);	\
	CONCAT(read_header, __LINE__): WAIT_FOR_EVENT(Event::Token); \
	header_[distance_into_section_] = get_latest_token().byte_value;	\
	distance_into_section_++; \
	if(distance_into_section_ != 7) goto CONCAT(read_header, __LINE__);	\
	set_data_mode(Scanning);

#define SET_DRIVE_HEAD_MFM()	\
	if(!dma_mode_) main_status_ |= StatusNDM;	\
	set_drive(drives_[command_[1]&3].drive);	\
	set_is_double_density(command_[0] & 0x40);

void i8272::posit_event(int event_type) {
	if(!(interesting_event_mask_ & event_type)) return;
	interesting_event_mask_ &= ~event_type;

	BEGIN_SECTION();

	wait_for_command:
		set_data_mode(Storage::Disk::MFMController::DataMode::Scanning);
		main_status_ &= ~(StatusCB | StatusNDM);
		command_.clear();

	wait_for_complete_command_sequence:
		main_status_ |= StatusRQM;
		WAIT_FOR_EVENT(Event8272::CommandByte)
		main_status_ |= StatusCB;
		main_status_ &= ~StatusRQM;

		switch(command_[0] & 0x1f) {
			case 0x06:	// read data
				if(command_.size() < 9) goto wait_for_complete_command_sequence;
				goto read_data;

			case 0x0b:	// read deleted data
				if(command_.size() < 9) goto wait_for_complete_command_sequence;
				goto read_deleted_data;

			case 0x05:	// write data
				if(command_.size() < 9) goto wait_for_complete_command_sequence;
				goto write_data;

			case 0x09:	// write deleted data
				if(command_.size() < 9) goto wait_for_complete_command_sequence;
				goto write_deleted_data;

			case 0x02:	// read track
				if(command_.size() < 9) goto wait_for_complete_command_sequence;
				goto read_track;

			case 0x0a:	// read ID
				if(command_.size() < 2) goto wait_for_complete_command_sequence;
				goto read_id;

			case 0x0d:	// format track
				if(command_.size() < 6) goto wait_for_complete_command_sequence;
				goto format_track;

			case 0x11:	// scan low
				if(command_.size() < 9) goto wait_for_complete_command_sequence;
				goto scan_low;

			case 0x19:	// scan low or equal
				if(command_.size() < 9) goto wait_for_complete_command_sequence;
				goto scan_low_or_equal;

			case 0x1d:	// scan high or equal
				if(command_.size() < 9) goto wait_for_complete_command_sequence;
				goto scan_high_or_equal;

			case 0x07:	// recalibrate
				if(command_.size() < 2) goto wait_for_complete_command_sequence;
				goto recalibrate;

			case 0x08:	// sense interrupt status
				goto sense_interrupt_status;

			case 0x03:	// specify
				if(command_.size() < 3) goto wait_for_complete_command_sequence;
				goto specify;

			case 0x04:	// sense drive status
				if(command_.size() < 2) goto wait_for_complete_command_sequence;
				goto sense_drive_status;

			case 0x0f:	// seek
				if(command_.size() < 3) goto wait_for_complete_command_sequence;
				goto seek;

			default:	// invalid
				goto invalid;
		}

	read_data:
		printf("Read data, sector %02x %02x %02x %02x\n", command_[2], command_[3], command_[4], command_[5]);
		SET_DRIVE_HEAD_MFM();
		cylinder_ = command_[2];
		head_ = command_[3];
		sector_ = command_[4];
		size_ = command_[5];

		index_hole_limit_ = 2;
	find_next_sector:
		FIND_HEADER();
		if(!index_hole_limit_) goto read_data_not_found;
		READ_HEADER();
		printf("Comparing with %02x\n", header_[2]);
		if(header_[2] != sector_) goto find_next_sector;

		FIND_DATA();
		distance_into_section_ = 0;
		set_data_mode(Reading);

	get_byte:
		WAIT_FOR_EVENT(Event::Token);
		result_stack_.push_back(get_latest_token().byte_value);
		distance_into_section_++;
		main_status_ |= StatusRQM | StatusDIO;
		WAIT_FOR_EVENT(Event8272::ResultEmpty);
		if(distance_into_section_ < (128 << size_)) goto get_byte;

		set_data_mode(Scanning);
		goto return_st012chrn;

	read_data_not_found:
		printf("Not found\n");

		status_[1] |= 0x4;
		status_[0] = 0x40;	// (status_[0] & ~0xc0) |
		goto return_st012chrn;

	read_deleted_data:
		printf("Read deleted data unimplemented!!\n");
		goto wait_for_command;

	write_data:
		printf("Write data unimplemented!!\n");
		goto wait_for_command;

	write_deleted_data:
		printf("Write deleted data unimplemented!!\n");
		goto wait_for_command;

	read_track:
		printf("Read track unimplemented!!\n");
		goto wait_for_command;

	read_id:
		printf("Read ID\n");
		SET_DRIVE_HEAD_MFM();

		index_hole_limit_ = 2;
	read_id_find_next_sector:
		FIND_HEADER();
		if(!index_hole_limit_) goto read_id_not_found;
		READ_HEADER();

		cylinder_ = header_[0];
		head_ = header_[1];
		sector_ = header_[2];
		size_ = header_[3];

		goto return_st012chrn;

	read_id_not_found:
		status_[1] |= 0x4;
		status_[0] = 0x40;	// (status_[0] & ~0xc0) |

	return_st012chrn:
		result_stack_.push_back(size_);
		result_stack_.push_back(sector_);
		result_stack_.push_back(head_);
		result_stack_.push_back(cylinder_);

		result_stack_.push_back(status_[2]);
		result_stack_.push_back(status_[1]);
		result_stack_.push_back(status_[0]);

		goto post_result;

	format_track:
		printf("Fromat track unimplemented!!\n");
		goto wait_for_command;

	scan_low:
		printf("Scan low unimplemented!!\n");
		goto wait_for_command;

	scan_low_or_equal:
		printf("Scan low or equal unimplemented!!\n");
		goto wait_for_command;

	scan_high_or_equal:
		printf("Scan high or equal unimplemented!!\n");
		goto wait_for_command;

	recalibrate:
		printf("Recalibrate\n");
		drives_[command_[1]&3].phase = Drive::Seeking;
		drives_[command_[1]&3].permitted_steps = 77;
		drives_[command_[1]&3].target_head_position = -1;
		drives_[command_[1]&3].step_rate_counter = 0;
		main_status_ |= (1 << command_[1]&3);
		goto wait_for_command;

	sense_interrupt_status:
		printf("Sense interrupt status\n");
		// Find the first drive that is in the CompletedSeeking state and return for that;
		// if none has done so then return a single 0x80.
		{
			int found_drive = -1;
			for(int c = 0; c < 4; c++) {
				if(drives_[c].phase == Drive::CompletedSeeking) {
					found_drive = c;
					break;
				}
			}
			if(found_drive != -1) {
				drives_[found_drive].phase = Drive::NotSeeking;
				status_[0] = (uint8_t)found_drive | 0x20;

				result_stack_.push_back(drives_[found_drive].head_position);
				result_stack_.push_back(status_[0]);
			} else {
				result_stack_.push_back(0x80);
			}
		}
		goto post_result;

	specify:
		printf("Specify\n");
		step_rate_time_ = command_[1] &0xf0;		// i.e. 16 to 240m
		head_unload_time_ = command_[1] & 0x0f;		// i.e. 1 to 16ms
		head_load_time_ = command_[2] & ~1;			// i.e. 2 to 254 ms in increments of 2ms
		dma_mode_ = !(command_[2] & 1);
		goto wait_for_command;

	sense_drive_status:
		printf("Sense drive status\n");
		result_stack_.push_back(status_[3]);
		goto post_result;

	seek:
		printf("Seek\n");
		drives_[command_[1]&3].phase = Drive::Seeking;
		drives_[command_[1]&3].permitted_steps = -1;
		drives_[command_[1]&3].target_head_position = command_[2];
		drives_[command_[1]&3].step_rate_counter = 0;
		main_status_ |= (1 << command_[1]&3);
		goto wait_for_command;

	invalid:
		// A no-op, causing the FDC to go back into standby mode.
		goto wait_for_command;

	post_result:
		// Set ready to send data to the processor, no longer in non-DMA execution phase.
		main_status_ |= StatusRQM | StatusDIO;
		main_status_ &= ~StatusNDM;

		WAIT_FOR_EVENT(Event8272::ResultEmpty);
		main_status_ &= ~StatusDIO;
		goto wait_for_command;

	END_SECTION()
}
