//
//  i8272.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "i8272.hpp"

#include "../../Outputs/Log.hpp"

using namespace Intel::i8272;

#define SetDataRequest()				(main_status_ |= 0x80)
#define ResetDataRequest()				(main_status_ &= ~0x80)
#define DataRequest()					(main_status_ & 0x80)

#define SetDataDirectionToProcessor()	(main_status_ |= 0x40)
#define SetDataDirectionFromProcessor()	(main_status_ &= ~0x40)
#define DataDirectionToProcessor()		(main_status_ & 0x40)

#define SetNonDMAExecution()			(main_status_ |= 0x20)
#define ResetNonDMAExecution()			(main_status_ &= ~0x20)

#define SetBusy()						(main_status_ |= 0x10)
#define ResetBusy()						(main_status_ &= ~0x10)
#define Busy()							(main_status_ & 0x10)

#define SetAbnormalTermination()		(status_[0] |= 0x40)
#define SetInvalidCommand()				(status_[0] |= 0x80)
#define SetReadyChanged()				(status_[0] |= 0xc0)
#define SetSeekEnd()					(status_[0] |= 0x20)
#define SetEquipmentCheck()				(status_[0] |= 0x10)
#define SetNotReady()					(status_[0] |= 0x08)
#define SetSide2()						(status_[0] |= 0x04)

#define SetEndOfCylinder()				(status_[1] |= 0x80)
#define SetDataError()					(status_[1] |= 0x20)
#define SetOverrun()					(status_[1] |= 0x10)
#define SetNoData()						(status_[1] |= 0x04)
#define SetNotWriteable()				(status_[1] |= 0x02)
#define SetMissingAddressMark()			(status_[1] |= 0x01)

#define SetControlMark()				(status_[2] |= 0x40)
#define ClearControlMark()				(status_[2] &= ~0x40)
#define ControlMark()					(status_[2] & 0x40)

#define SetDataFieldDataError()			(status_[2] |= 0x20)
#define SetWrongCyinder()				(status_[2] |= 0x10)
#define SetScanEqualHit()				(status_[2] |= 0x08)
#define SetScanNotSatisfied()			(status_[2] |= 0x04)
#define SetBadCylinder()				(status_[2] |= 0x02)
#define SetMissingDataAddressMark()		(status_[2] |= 0x01)

namespace {
	const uint8_t CommandReadData = 0x06;
	const uint8_t CommandReadDeletedData = 0x0c;

	const uint8_t CommandWriteData = 0x05;
	const uint8_t CommandWriteDeletedData = 0x09;

	const uint8_t CommandReadTrack = 0x02;
	const uint8_t CommandReadID = 0x0a;
	const uint8_t CommandFormatTrack = 0x0d;

	const uint8_t CommandScanLow = 0x11;
	const uint8_t CommandScanLowOrEqual = 0x19;
	const uint8_t CommandScanHighOrEqual = 0x1d;

	const uint8_t CommandRecalibrate = 0x07;
	const uint8_t CommandSeek = 0x0f;

	const uint8_t CommandSenseInterruptStatus = 0x08;
	const uint8_t CommandSpecify = 0x03;
	const uint8_t CommandSenseDriveStatus = 0x04;
}

i8272::i8272(BusHandler &bus_handler, Cycles clock_rate) :
	Storage::Disk::MFMController(clock_rate),
	bus_handler_(bus_handler) {
	posit_event(static_cast<int>(Event8272::CommandByte));
}

ClockingHint::Preference i8272::preferred_clocking() {
	const auto mfm_controller_preferred_clocking = Storage::Disk::MFMController::preferred_clocking();
	if(mfm_controller_preferred_clocking != ClockingHint::Preference::None) return mfm_controller_preferred_clocking;
	return is_sleeping_ ? ClockingHint::Preference::None : ClockingHint::Preference::JustInTime;
}

void i8272::run_for(Cycles cycles) {
	Storage::Disk::MFMController::run_for(cycles);

	if(is_sleeping_) return;

	// check for an expired timer
	if(delay_time_ > 0) {
		if(cycles.as_integral() >= delay_time_) {
			delay_time_ = 0;
			posit_event(static_cast<int>(Event8272::Timer));
		} else {
			delay_time_ -= cycles.as_integral();
		}
	}

	// update seek status of any drives presently seeking
	if(drives_seeking_) {
		int drives_left = drives_seeking_;
		for(int c = 0; c < 4; c++) {
			if(drives_[c].phase == Drive::Seeking) {
				drives_[c].step_rate_counter += cycles.as_integral();
				auto steps = drives_[c].step_rate_counter / (8000 * step_rate_time_);
				drives_[c].step_rate_counter %= (8000 * step_rate_time_);
				while(steps--) {
					// Perform a step.
					int direction = (drives_[c].target_head_position < drives_[c].head_position) ? -1 : 1;
					LOG("Target " << PADDEC(0) << drives_[c].target_head_position << " versus believed " << static_cast<int>(drives_[c].head_position));
					select_drive(c);
					get_drive().step(Storage::Disk::HeadPosition(direction));
					if(drives_[c].target_head_position >= 0) drives_[c].head_position += direction;

					// Check for completion.
					if(seek_is_satisfied(c)) {
						drives_[c].phase = Drive::CompletedSeeking;
						drives_seeking_--;
						break;
					}
				}

				drives_left--;
				if(!drives_left) break;
			}
		}
	}

	// check for any head unloads
	if(head_timers_running_) {
		int timers_left = head_timers_running_;
		for(int c = 0; c < 8; c++) {
			int drive = (c >> 1);
			int head = c&1;

			if(drives_[drive].head_unload_delay[head] > 0) {
				if(cycles.as_integral() >= drives_[drive].head_unload_delay[head]) {
					drives_[drive].head_unload_delay[head] = 0;
					drives_[drive].head_is_loaded[head] = false;
					head_timers_running_--;
				} else {
					drives_[drive].head_unload_delay[head] -= cycles.as_integral();
				}
				timers_left--;
				if(!timers_left) break;
			}
		}
	}

	// check for busy plus ready disabled
	if(is_executing_ && !get_drive().get_is_ready()) {
		posit_event(static_cast<int>(Event8272::NoLongerReady));
	}

	is_sleeping_ = !delay_time_ && !drives_seeking_ && !head_timers_running_;
	if(is_sleeping_) update_clocking_observer();
}

void i8272::set_register(int address, uint8_t value) {
	// don't consider attempted sets to the status register
	if(!address) return;

	// if not ready for commands, do nothing
	if(!DataRequest() || DataDirectionToProcessor()) return;

	if(expects_input_) {
		input_ = value;
		has_input_ = true;
		ResetDataRequest();
	} else {
		// accumulate latest byte in the command byte sequence
		command_.push_back(value);
		posit_event(static_cast<int>(Event8272::CommandByte));
	}
}

uint8_t i8272::get_register(int address) {
	if(address) {
		if(result_stack_.empty()) return 0xff;
		uint8_t result = result_stack_.back();
		result_stack_.pop_back();
		if(result_stack_.empty()) posit_event(static_cast<int>(Event8272::ResultEmpty));

		return result;
	} else {
		return main_status_;
	}
}

#define BEGIN_SECTION()	switch(resume_point_) { default:
#define END_SECTION()	}

#define MS_TO_CYCLES(x)			x * 8000
#define WAIT_FOR_EVENT(mask)	resume_point_ = __LINE__; interesting_event_mask_ = static_cast<int>(mask); return; case __LINE__:
#define WAIT_FOR_TIME(ms)		resume_point_ = __LINE__; interesting_event_mask_ = static_cast<int>(Event8272::Timer); delay_time_ = MS_TO_CYCLES(ms); is_sleeping_ = false; update_clocking_observer(); case __LINE__: if(delay_time_) return;

#define PASTE(x, y) x##y
#define CONCAT(x, y) PASTE(x, y)

#define FIND_HEADER()	\
	set_data_mode(DataMode::Scanning);	\
	CONCAT(find_header, __LINE__): WAIT_FOR_EVENT(static_cast<int>(Event::Token) | static_cast<int>(Event::IndexHole)); \
	if(event_type == static_cast<int>(Event::IndexHole)) { index_hole_limit_--; }	\
	else if(get_latest_token().type == Token::ID) goto CONCAT(header_found, __LINE__);	\
	\
	if(index_hole_limit_) goto CONCAT(find_header, __LINE__);	\
	CONCAT(header_found, __LINE__):	(void)0;\

#define FIND_DATA()	\
	set_data_mode(DataMode::Scanning);	\
	CONCAT(find_data, __LINE__): WAIT_FOR_EVENT(static_cast<int>(Event::Token) | static_cast<int>(Event::IndexHole)); \
	if(event_type == static_cast<int>(Event::Token)) { \
		if(get_latest_token().type == Token::Byte || get_latest_token().type == Token::Sync) goto CONCAT(find_data, __LINE__);	\
	}

#define READ_HEADER()	\
	distance_into_section_ = 0;	\
	set_data_mode(DataMode::Reading);	\
	CONCAT(read_header, __LINE__): WAIT_FOR_EVENT(Event::Token); \
	header_[distance_into_section_] = get_latest_token().byte_value;	\
	distance_into_section_++; \
	if(distance_into_section_ < 6) goto CONCAT(read_header, __LINE__);	\

#define SET_DRIVE_HEAD_MFM()	\
	active_drive_ = command_[1]&3;	\
	active_head_ = (command_[1] >> 2)&1;	\
	status_[0] = (command_[1]&7);	\
	select_drive(active_drive_);	\
	get_drive().set_head(active_head_);	\
	set_is_double_density(command_[0] & 0x40);

#define WAIT_FOR_BYTES(n) \
	distance_into_section_ = 0;	\
	CONCAT(wait_bytes, __LINE__): WAIT_FOR_EVENT(Event::Token);	\
	if(get_latest_token().type == Token::Byte) distance_into_section_++;	\
	if(distance_into_section_ < (n)) goto CONCAT(wait_bytes, __LINE__);

#define LOAD_HEAD()	\
	if(!drives_[active_drive_].head_is_loaded[active_head_]) {	\
		drives_[active_drive_].head_is_loaded[active_head_] = true;	\
		WAIT_FOR_TIME(head_load_time_);	\
	} else {	\
		if(drives_[active_drive_].head_unload_delay[active_head_] > 0) {	\
			drives_[active_drive_].head_unload_delay[active_head_] = 0;	\
			head_timers_running_--;	\
		}	\
	}

#define SCHEDULE_HEAD_UNLOAD()	\
	if(drives_[active_drive_].head_is_loaded[active_head_]) {\
		if(drives_[active_drive_].head_unload_delay[active_head_] == 0) {	\
			head_timers_running_++;	\
			is_sleeping_ = false;	\
			update_clocking_observer();	\
		}	\
		drives_[active_drive_].head_unload_delay[active_head_] = MS_TO_CYCLES(head_unload_time_);\
	}

void i8272::posit_event(int event_type) {
	if(event_type == static_cast<int>(Event::IndexHole)) index_hole_count_++;
	if(event_type == static_cast<int>(Event8272::NoLongerReady)) {
		SetNotReady();
		goto abort;
	}
	if(!(interesting_event_mask_ & event_type)) return;
	interesting_event_mask_ &= ~event_type;

	BEGIN_SECTION();

	// Resets busy and non-DMA execution, clears the command buffer, sets the data mode to scanning and flows
	// into wait_for_complete_command_sequence.
	wait_for_command:
			expects_input_ = false;
			set_data_mode(Storage::Disk::MFMController::DataMode::Scanning);
			ResetBusy();
			ResetNonDMAExecution();
			command_.clear();

	// Sets the data request bit, and waits for a byte. Then sets the busy bit. Continues accepting bytes
	// until it has a quantity that make up an entire command, then resets the data request bit and
	// branches to that command.
	wait_for_complete_command_sequence:
			SetDataRequest();
			SetDataDirectionFromProcessor();
			WAIT_FOR_EVENT(Event8272::CommandByte)
			SetBusy();

			static const std::size_t required_lengths[32] = {
				0,	0,	9,	3,	2,	9,	9,	2,
				1,	9,	2,	0,	9,	6,	0,	3,
				0,	9,	0,	0,	0,	0,	0,	0,
				0,	9,	0,	0,	0,	9,	0,	0,
			};

			if(command_.size() < required_lengths[command_[0] & 0x1f]) goto wait_for_complete_command_sequence;
			if(command_.size() == 9) {
				cylinder_ = command_[2];
				head_ = command_[3];
				sector_ = command_[4];
				size_ = command_[5];
			}
			ResetDataRequest();
			status_[0] = status_[1] = status_[2] = 0;

			// If this is not clearly a command that's safe to carry out in parallel to a seek, end all seeks.
			switch(command_[0] & 0x1f) {
				case CommandReadData:
				case CommandReadDeletedData:
				case CommandWriteData:
				case CommandWriteDeletedData:
				case CommandReadTrack:
				case CommandReadID:
				case CommandFormatTrack:
				case CommandScanLow:
				case CommandScanLowOrEqual:
				case CommandScanHighOrEqual:
					is_access_command_ = true;
				break;

				default:
					is_access_command_ = false;
				break;
			}

			if(is_access_command_) {
				for(int c = 0; c < 4; c++) {
					if(drives_[c].phase == Drive::Seeking) {
						drives_[c].phase = Drive::NotSeeking;
						drives_seeking_--;
					}
				}
				// Establishes the drive and head being addressed, and whether in double density mode; populates the internal
				// cylinder, head, sector and size registers from the command stream.
				is_executing_ = true;
				if(!dma_mode_) SetNonDMAExecution();
				SET_DRIVE_HEAD_MFM();
				LOAD_HEAD();
				if(!get_drive().get_is_ready()) {
					SetNotReady();
					goto abort;
				}
			}

			// Jump to the proper place.
			switch(command_[0] & 0x1f) {
				case CommandReadData:
				case CommandReadDeletedData:
					goto read_data;

				case CommandWriteData:
				case CommandWriteDeletedData:
					goto write_data;

				case CommandReadTrack:				goto read_track;
				case CommandReadID:					goto read_id;
				case CommandFormatTrack:			goto format_track;

				case CommandScanLow:				goto scan_low;
				case CommandScanLowOrEqual:			goto scan_low_or_equal;
				case CommandScanHighOrEqual:		goto scan_high_or_equal;

				case CommandRecalibrate:			goto recalibrate;
				case CommandSeek:					goto seek;

				case CommandSenseInterruptStatus:	goto sense_interrupt_status;
				case CommandSpecify:				goto specify;
				case CommandSenseDriveStatus:		goto sense_drive_status;

				default:							goto invalid;
			}

	// Decodes drive, head and density, loads the head, loads the internal cylinder, head, sector and size registers,
	// and searches for a sector that meets those criteria. If one is found, inspects the instruction in use and
	// jumps to an appropriate handler.
	read_write_find_header:

		// Sets a maximum index hole limit of 2 then performs a find header/read header loop, continuing either until
		// the index hole limit is breached or a sector is found with a cylinder, head, sector and size equal to the
		// values in the internal registers.
			index_hole_limit_ = 2;
//			LOG("Seeking " << PADDEC(0) << cylinder_ << " " << head_ " " << sector_ << " " << size_);
		find_next_sector:
			FIND_HEADER();
			if(!index_hole_limit_) {
				// Two index holes have passed wihout finding the header sought.
//				LOG("Not found");
				SetNoData();
				goto abort;
			}
			index_hole_count_ = 0;
//			LOG("Header");
			READ_HEADER();
			if(index_hole_count_) {
				// This implies an index hole was sighted within the header. Error out.
				SetEndOfCylinder();
				goto abort;
			}
			if(get_crc_generator().get_value()) {
				// This implies a CRC error in the header; mark as such but continue.
				SetDataError();
			}
//			LOG("Considering << PADHEX(2) << header_[0] << " " << header_[1] << " " << header_[2] << " " << header_[3] << " [" << get_crc_generator().get_value() << "]");
			if(header_[0] != cylinder_ || header_[1] != head_ || header_[2] != sector_ || header_[3] != size_) goto find_next_sector;

			// Branch to whatever is supposed to happen next
//			LOG("Proceeding");
			switch(command_[0] & 0x1f) {
				case CommandReadData:
				case CommandReadDeletedData:
				goto read_data_found_header;

				case CommandWriteData:	// write data
				case CommandWriteDeletedData:	// write deleted data
				goto write_data_found_header;
			}


	// Performs the read data or read deleted data command.
	read_data:
			LOG(PADHEX(2) << "Read [deleted] data ["
				<< static_cast<int>(command_[2]) << " "
				<< static_cast<int>(command_[3]) << " "
				<< static_cast<int>(command_[4]) << " "
				<< static_cast<int>(command_[5]) << " ... "
				<< static_cast<int>(command_[6]) << " "
				<< static_cast<int>(command_[8]) << "]");
		read_next_data:
			goto read_write_find_header;

		// Finds the next data block and sets data mode to reading, setting an error flag if the on-disk deleted
		// flag doesn't match the sort the command was looking for.
		read_data_found_header:
			FIND_DATA();
			ClearControlMark();
			if(event_type == static_cast<int>(Event::Token)) {
				if(get_latest_token().type != Token::Data && get_latest_token().type != Token::DeletedData) {
					// Something other than a data mark came next, impliedly an ID or index mark.
					SetMissingAddressMark();
					SetMissingDataAddressMark();
					goto abort;	// TODO: or read_next_data?
				} else {
					if((get_latest_token().type == Token::Data) != ((command_[0] & 0x1f) == CommandReadData)) {
						if(!(command_[0]&0x20)) {
							// SK is not set; set the error flag but read this sector before finishing.
							SetControlMark();
						} else {
							// SK is set; skip this sector.
							goto read_next_data;
						}
					}
				}
			} else {
				// An index hole appeared before the data mark.
				SetEndOfCylinder();
				goto abort;	// TODO: or read_next_data?
			}

			distance_into_section_ = 0;
			set_data_mode(Reading);

		// Waits for the next token, then supplies it to the CPU by: (i) setting data request and direction; and (ii) resetting
		// data request once the byte has been taken. Continues until all bytes have been read.
		//
		// TODO: consider DTL.
		read_data_get_byte:
			WAIT_FOR_EVENT(static_cast<int>(Event::Token) | static_cast<int>(Event::IndexHole));
			if(event_type == static_cast<int>(Event::Token)) {
				result_stack_.push_back(get_latest_token().byte_value);
				distance_into_section_++;
				SetDataRequest();
				SetDataDirectionToProcessor();
				WAIT_FOR_EVENT(static_cast<int>(Event8272::ResultEmpty) | static_cast<int>(Event::Token) | static_cast<int>(Event::IndexHole));
			}
			switch(event_type) {
				case static_cast<int>(Event8272::ResultEmpty):	// The caller read the byte in time; proceed as normal.
					ResetDataRequest();
					if(distance_into_section_ < (128 << size_)) goto read_data_get_byte;
				break;
				case static_cast<int>(Event::Token):				// The caller hasn't read the old byte yet and a new one has arrived
					SetOverrun();
					goto abort;
				break;
				case static_cast<int>(Event::IndexHole):
					SetEndOfCylinder();
					goto abort;
				break;
			}

		// read CRC, without transferring it, then check it
			WAIT_FOR_EVENT(Event::Token);
			WAIT_FOR_EVENT(Event::Token);
			if(get_crc_generator().get_value()) {
				// This implies a CRC error in the sector; mark as such and temrinate.
				SetDataError();
				SetDataFieldDataError();
				goto abort;
			}

		// check whether that's it: either the final requested sector has been read, or because
		// a sector that was [/wasn't] marked as deleted when it shouldn't [/should] have been
			if(sector_ != command_[6] && !ControlMark()) {
				sector_++;
				goto read_next_data;
			}

		// For a final result phase, post the standard ST0, ST1, ST2, C, H, R, N
			goto post_st012chrn;

	write_data:
			LOG(PADHEX(2) << "Write [deleted] data ["
				<< static_cast<int>(command_[2]) << " "
				<< static_cast<int>(command_[3]) << " "
				<< static_cast<int>(command_[4]) << " "
				<< static_cast<int>(command_[5]) << " ... "
				<< static_cast<int>(command_[6]) << " "
				<< static_cast<int>(command_[8]) << "]");

			if(get_drive().get_is_read_only()) {
				SetNotWriteable();
				goto abort;
			}

		write_next_data:
			goto read_write_find_header;

		write_data_found_header:
			WAIT_FOR_BYTES(get_is_double_density() ? 22 : 11);
			begin_writing(true);

			write_id_data_joiner((command_[0] & 0x1f) == CommandWriteDeletedData, true);

			SetDataDirectionFromProcessor();
			SetDataRequest();
			expects_input_ = true;
			distance_into_section_ = 0;

		write_loop:
			WAIT_FOR_EVENT(Event::DataWritten);
			if(!has_input_) {
				SetOverrun();
				goto abort;
			}
			write_byte(input_);
			has_input_ = false;
			distance_into_section_++;
			if(distance_into_section_ < (128 << size_)) {
				SetDataRequest();
				goto write_loop;
			}

			LOG("Wrote " << PADDEC(0) << distance_into_section_ << " bytes");
			write_crc();
			expects_input_ = false;
			WAIT_FOR_EVENT(Event::DataWritten);
			end_writing();

			if(sector_ != command_[6]) {
				sector_++;
				goto write_next_data;
			}

		goto post_st012chrn;

	// Performs the read ID command.
	read_id:
		// Establishes the drive and head being addressed, and whether in double density mode.
			LOG(PADHEX(2) << "Read ID [" << static_cast<int>(command_[0]) << " " << static_cast<int>(command_[1]) << "]");

		// Sets a maximum index hole limit of 2 then waits either until it finds a header mark or sees too many index holes.
		// If a header mark is found, reads in the following bytes that produce a header. Otherwise branches to data not found.
			index_hole_limit_ = 2;
			FIND_HEADER();
			if(!index_hole_limit_) {
				SetMissingAddressMark();
				goto abort;
			}
			READ_HEADER();

		// Sets internal registers from the discovered header and posts the standard ST0, ST1, ST2, C, H, R, N.
			cylinder_ = header_[0];
			head_ = header_[1];
			sector_ = header_[2];
			size_ = header_[3];

			goto post_st012chrn;

	// Performs read track.
	read_track:
			LOG(PADHEX(2) << "Read track ["
				<< static_cast<int>(command_[2]) << " "
				<< static_cast<int>(command_[3]) << " "
				<< static_cast<int>(command_[4]) << " "
				<< static_cast<int>(command_[5]) << "]");

			// Wait for the index hole.
			WAIT_FOR_EVENT(Event::IndexHole);

			sector_ = 0;
			index_hole_limit_ = 2;

		// While not index hole again, stream all sector contents until EOT sectors have been read.
		read_track_next_sector:
			FIND_HEADER();
			if(!index_hole_limit_) {
				if(!sector_) {
					SetMissingAddressMark();
					goto abort;
				} else {
					goto post_st012chrn;
				}
			}
			READ_HEADER();

			FIND_DATA();
			distance_into_section_ = 0;
			SetDataDirectionToProcessor();
		read_track_get_byte:
			WAIT_FOR_EVENT(Event::Token);
			result_stack_.push_back(get_latest_token().byte_value);
			distance_into_section_++;
			SetDataRequest();
			// TODO: other possible exit conditions; find a way to merge with the read_data version of this.
			WAIT_FOR_EVENT(static_cast<int>(Event8272::ResultEmpty));
			ResetDataRequest();
			if(distance_into_section_ < (128 << header_[2])) goto read_track_get_byte;

			sector_++;
			if(sector_ < command_[6]) goto read_track_next_sector;

			goto post_st012chrn;

	// Performs format [/write] track.
	format_track:
			LOG("Format track");
			if(get_drive().get_is_read_only()) {
				SetNotWriteable();
				goto abort;
			}

			// Wait for the index hole.
			WAIT_FOR_EVENT(Event::IndexHole);
			index_hole_count_ = 0;
			begin_writing(true);

			// Write start-of-track.
			write_start_of_track();
			WAIT_FOR_EVENT(Event::DataWritten);
			sector_ = 0;

		format_track_write_sector:
			write_id_joiner();

			// Write the sector header, obtaining its contents
			// from the processor.
			SetDataDirectionFromProcessor();
			SetDataRequest();
			expects_input_ = true;
			distance_into_section_ = 0;
		format_track_write_header:
			WAIT_FOR_EVENT(static_cast<int>(Event::DataWritten) | static_cast<int>(Event::IndexHole));
			switch(event_type) {
				case static_cast<int>(Event::IndexHole):
					SetOverrun();
					goto abort;
				break;
				case static_cast<int>(Event::DataWritten):
					header_[distance_into_section_] = input_;
					write_byte(input_);
					has_input_ = false;
					distance_into_section_++;
					if(distance_into_section_ < 4) {
						SetDataRequest();
						goto format_track_write_header;
					}
				break;
			}

			LOG(PADHEX(2) << "W:"
				<< static_cast<int>(header_[0]) << " "
				<< static_cast<int>(header_[1]) << " "
				<< static_cast<int>(header_[2]) << " "
				<< static_cast<int>(header_[3]) << ", "
				<< get_crc_generator().get_value());
			write_crc();

			// Write the sector body.
			write_id_data_joiner(false, false);
			write_n_bytes(128 << command_[2], command_[5]);
			write_crc();

			// Write the prescribed gap.
			write_n_bytes(command_[4], get_is_double_density() ? 0x4e : 0xff);

			// Consider repeating.
			sector_++;
			if(sector_ < command_[3] && !index_hole_count_)
				goto format_track_write_sector;

			// Otherwise, pad out to the index hole.
		format_track_pad:
			write_byte(get_is_double_density() ? 0x4e : 0xff);
			WAIT_FOR_EVENT(static_cast<int>(Event::DataWritten) | static_cast<int>(Event::IndexHole));
			if(event_type != static_cast<int>(Event::IndexHole)) goto format_track_pad;

			end_writing();

			cylinder_ = header_[0];
			head_ = header_[1];
			sector_ = header_[2] + 1;
			size_ = header_[3];

		goto post_st012chrn;

	scan_low:
		ERROR("Scan low unimplemented!!");
		goto wait_for_command;

	scan_low_or_equal:
		ERROR("Scan low or equal unimplemented!!");
		goto wait_for_command;

	scan_high_or_equal:
		ERROR("Scan high or equal unimplemented!!");
		goto wait_for_command;

	// Performs both recalibrate and seek commands. These commands occur asynchronously, so the actual work
	// occurs in ::run_for; this merely establishes that seeking should be ongoing.
	recalibrate:
	seek:
			{
				int drive = command_[1]&3;
				select_drive(drive);

				// Increment the seeking count if this drive wasn't already seeking.
				if(drives_[drive].phase != Drive::Seeking) {
					drives_seeking_++;
					is_sleeping_ = false;
					update_clocking_observer();
				}

				// Set currently seeking, with a step to occur right now (yes, it sounds like jamming these
				// in could damage your drive motor).
				drives_[drive].phase = Drive::Seeking;
				drives_[drive].step_rate_counter = 8000 * step_rate_time_;
				drives_[drive].steps_taken = 0;
				drives_[drive].seek_failed = false;
				main_status_ |= 1 << (command_[1]&3);

				// If this is a seek, set the processor-supplied target location; otherwise it is a recalibrate,
				// which means resetting the current state now but aiming to hit '-1' (which the stepping code
				// up in run_for understands to mean 'keep going until track 0 is active').
				if(command_.size() > 2) {
					drives_[drive].target_head_position = command_[2];
					LOG(PADHEX(2) << "Seek to " << static_cast<int>(command_[2]));
				} else {
					drives_[drive].target_head_position = -1;
					drives_[drive].head_position = 0;
					LOG("Recalibrate");
				}

				// Check whether any steps are even needed; if not then mark as completed already.
				if(seek_is_satisfied(drive)) {
					drives_[drive].phase = Drive::CompletedSeeking;
					drives_seeking_--;
				}
			}
			goto wait_for_command;

	// Performs sense interrupt status.
	sense_interrupt_status:
			LOG("Sense interrupt status");
			{
				// Find the first drive that is in the CompletedSeeking state.
				int found_drive = -1;
				for(int c = 0; c < 4; c++) {
					if(drives_[c].phase == Drive::CompletedSeeking) {
						found_drive = c;
						break;
					}
				}

				// If a drive was found, return its results. Otherwise return a single 0x80.
				if(found_drive != -1) {
					drives_[found_drive].phase = Drive::NotSeeking;
					status_[0] = static_cast<uint8_t>(found_drive);
					main_status_ &= ~(1 << found_drive);
					SetSeekEnd();

					result_stack_ = { drives_[found_drive].head_position, status_[0]};
				} else {
					result_stack_ = { 0x80 };
				}
			}
			goto post_result;

	// Performs specify.
	specify:
		// Just store the values, and terminate the command.
			LOG("Specify");
			step_rate_time_ = 16 - (command_[1] >> 4);			// i.e. 1 to 16ms
			head_unload_time_ = (command_[1] & 0x0f) << 4;		// i.e. 16 to 240ms
			head_load_time_ = command_[2] & ~1;					// i.e. 2 to 254 ms in increments of 2ms

			if(!head_unload_time_) head_unload_time_ = 16;
			if(!head_load_time_) head_load_time_ = 2;
			dma_mode_ = !(command_[2] & 1);
			goto wait_for_command;

	sense_drive_status:
			LOG("Sense drive status");
			{
				int drive = command_[1] & 3;
				select_drive(drive);
				result_stack_= {
					static_cast<uint8_t>(
						(command_[1] & 7) |	// drive and head number
						0x08 |				// single sided
						(get_drive().get_is_track_zero() ? 0x10 : 0x00)	|
						(get_drive().get_is_ready() ? 0x20 : 0x00)		|
						(get_drive().get_is_read_only() ? 0x40 : 0x00)
					)
				};
			}
			goto post_result;

	// Performs any invalid command.
	invalid:
			// A no-op, but posts ST0 (but which ST0?)
			result_stack_ = {0x80};
			goto post_result;

	// Sets abnormal termination of the current command and proceeds to an ST0, ST1, ST2, C, H, R, N result phase.
	abort:
		end_writing();
		SetAbnormalTermination();
		goto post_st012chrn;

	// Posts ST0, ST1, ST2, C, H, R and N as a result phase.
	post_st012chrn:
			SCHEDULE_HEAD_UNLOAD();

			result_stack_ = {size_, sector_, head_, cylinder_, status_[2], status_[1], status_[0]};

			goto post_result;

	// Posts whatever is in result_stack_ as a result phase. Be aware that it is a stack, so the
	// last thing in it will be returned first.
	post_result:
			LOGNBR(PADHEX(2) << "Result to " << static_cast<int>(command_[0] & 0x1f) << ", main " << static_cast<int>(main_status_) << "; ");
			for(std::size_t c = 0; c < result_stack_.size(); c++) {
				LOGNBR(" " << static_cast<int>(result_stack_[result_stack_.size() - 1 - c]));
			}
			LOGNBR(std::endl);

			// Set ready to send data to the processor, no longer in non-DMA execution phase.
			is_executing_ = false;
			ResetNonDMAExecution();
			SetDataRequest();
			SetDataDirectionToProcessor();

			// The actual stuff of unwinding result_stack_ is handled by ::get_register; wait
			// until the processor has read all result bytes.
			WAIT_FOR_EVENT(Event8272::ResultEmpty);

			// Reset data direction and end the command.
			goto wait_for_command;

	END_SECTION()
}

bool i8272::seek_is_satisfied(int drive) {
	return	(drives_[drive].target_head_position == drives_[drive].head_position) ||
			(drives_[drive].target_head_position == -1 && get_drive().get_is_track_zero());
}

void i8272::set_dma_acknowledge(bool dack) {
}

void i8272::set_terminal_count(bool tc) {
}

void i8272::set_data_input(uint8_t value) {
}

uint8_t i8272::get_data_output() {
	return 0xff;
}
