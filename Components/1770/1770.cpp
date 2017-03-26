//
//  1770.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "1770.hpp"
#include "../../Storage/Disk/Encodings/MFM.hpp"

using namespace WD;

WD1770::Status::Status() :
		type(Status::One),
		write_protect(false),
		record_type(false),
		spin_up(false),
		record_not_found(false),
		crc_error(false),
		seek_error(false),
		lost_data(false),
		data_request(false),
		interrupt_request(false),
		busy(false) {}

WD1770::WD1770(Personality p) :
		Storage::Disk::Controller(8000000, 16, 300),
		crc_generator_(0x1021, 0xffff),
		interesting_event_mask_(Event::Command),
		resume_point_(0),
		delay_time_(0),
		index_hole_count_target_(-1),
		is_awaiting_marker_value_(false),
		data_mode_(DataMode::Scanning),
		delegate_(nullptr),
		personality_(p),
		head_is_loaded_(false) {
	set_is_double_density(false);
	posit_event(Event::Command);
}

void WD1770::set_is_double_density(bool is_double_density) {
	is_double_density_ = is_double_density;
	Storage::Time bit_length;
	bit_length.length = 1;
	bit_length.clock_rate = is_double_density ? 500000 : 250000;
	set_expected_bit_length(bit_length);

	if(!is_double_density) is_awaiting_marker_value_ = false;
}

void WD1770::set_register(int address, uint8_t value) {
	switch(address&3) {
		case 0: {
			if((value&0xf0) == 0xd0) {
				printf("!!!TODO: force interrupt!!!\n");
				update_status([] (Status &status) {
					status.type = Status::One;
				});
			} else {
				command_ = value;
				posit_event(Event::Command);
			}
		}
		break;
		case 1:		track_ = value;		break;
		case 2:		sector_ = value;	break;
		case 3:
			data_ = value;
			update_status([] (Status &status) {
				status.data_request = false;
			});
		break;
	}
}

uint8_t WD1770::get_register(int address) {
	switch(address&3) {
		default: {
			update_status([] (Status &status) {
				status.interrupt_request = false;
			});
			uint8_t status =
					(status_.write_protect ? Flag::WriteProtect : 0) |
					(status_.crc_error ? Flag::CRCError : 0) |
					(status_.busy ? Flag::Busy : 0);
			switch(status_.type) {
				case Status::One:
					status |=
						(get_is_track_zero() ? Flag::TrackZero : 0) |
						(status_.seek_error ? Flag::SeekError : 0);
						// TODO: index hole
				break;

				case Status::Two:
				case Status::Three:
					status |=
						(status_.record_type ? Flag::RecordType : 0) |
						(status_.lost_data ? Flag::LostData : 0) |
						(status_.data_request ? Flag::DataRequest : 0) |
						(status_.record_not_found ? Flag::RecordNotFound : 0);
				break;
			}

			if(!has_motor_on_line()) {
				status |= get_drive_is_ready() ? 0 : Flag::NotReady;
				if(status_.type == Status::One)
					status |= (head_is_loaded_ ? Flag::HeadLoaded : 0);
			} else {
				status |= (get_motor_on() ? Flag::MotorOn : 0);
				if(status_.type == Status::One)
					status |= (status_.spin_up ? Flag::SpinUp : 0);
			}
			return status;
		}
		case 1:		return track_;
		case 2:		return sector_;
		case 3:
			update_status([] (Status &status) {
				status.data_request = false;
			});
		return data_;
	}
}

void WD1770::run_for_cycles(unsigned int number_of_cycles) {
	Storage::Disk::Controller::run_for_cycles((int)number_of_cycles);

	if(delay_time_) {
		if(delay_time_ <= number_of_cycles) {
			delay_time_ = 0;
			posit_event(Event::Timer);
		} else {
			delay_time_ -= number_of_cycles;
		}
	}
}

void WD1770::process_input_bit(int value, unsigned int cycles_since_index_hole) {
	if(data_mode_ == DataMode::Writing) return;

	shift_register_ = (shift_register_ << 1) | value;
	bits_since_token_++;

	if(data_mode_ == DataMode::Scanning) {
		Token::Type token_type = Token::Byte;
		if(!is_double_density_) {
			switch(shift_register_ & 0xffff) {
				case Storage::Encodings::MFM::FMIndexAddressMark:
					token_type = Token::Index;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::IndexAddressByte);
				break;
				case Storage::Encodings::MFM::FMIDAddressMark:
					token_type = Token::ID;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::IDAddressByte);
				break;
				case Storage::Encodings::MFM::FMDataAddressMark:
					token_type = Token::Data;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::DataAddressByte);
				break;
				case Storage::Encodings::MFM::FMDeletedDataAddressMark:
					token_type = Token::DeletedData;
					crc_generator_.reset();
					crc_generator_.add(latest_token_.byte_value = Storage::Encodings::MFM::DeletedDataAddressByte);
				break;
				default:
				break;
			}
		} else {
			switch(shift_register_ & 0xffff) {
				case Storage::Encodings::MFM::MFMIndexSync:
					bits_since_token_ = 0;
					is_awaiting_marker_value_ = true;

					token_type = Token::Sync;
					latest_token_.byte_value = Storage::Encodings::MFM::MFMIndexSyncByteValue;
				break;
				case Storage::Encodings::MFM::MFMSync:
					bits_since_token_ = 0;
					is_awaiting_marker_value_ = true;
					crc_generator_.set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);

					token_type = Token::Sync;
					latest_token_.byte_value = Storage::Encodings::MFM::MFMSyncByteValue;
				break;
				default:
				break;
			}
		}

		if(token_type != Token::Byte) {
			latest_token_.type = token_type;
			bits_since_token_ = 0;
			posit_event(Event::Token);
			return;
		}
	}

	if(bits_since_token_ == 16) {
		latest_token_.type = Token::Byte;
		latest_token_.byte_value = (uint8_t)(
			((shift_register_ & 0x0001) >> 0) |
			((shift_register_ & 0x0004) >> 1) |
			((shift_register_ & 0x0010) >> 2) |
			((shift_register_ & 0x0040) >> 3) |
			((shift_register_ & 0x0100) >> 4) |
			((shift_register_ & 0x0400) >> 5) |
			((shift_register_ & 0x1000) >> 6) |
			((shift_register_ & 0x4000) >> 7));
		bits_since_token_ = 0;

		if(is_awaiting_marker_value_ && is_double_density_) {
			is_awaiting_marker_value_ = false;
			switch(latest_token_.byte_value) {
				case Storage::Encodings::MFM::IndexAddressByte:
					latest_token_.type = Token::Index;
				break;
				case Storage::Encodings::MFM::IDAddressByte:
					latest_token_.type = Token::ID;
				break;
				case Storage::Encodings::MFM::DataAddressByte:
					latest_token_.type = Token::Data;
				break;
				case Storage::Encodings::MFM::DeletedDataAddressByte:
					latest_token_.type = Token::DeletedData;
				break;
				default: break;
			}
		}

		crc_generator_.add(latest_token_.byte_value);
		posit_event(Event::Token);
		return;
	}
}

void WD1770::process_index_hole() {
	index_hole_count_++;
	posit_event(Event::IndexHole);
	if(index_hole_count_target_ == index_hole_count_) {
		posit_event(Event::IndexHoleTarget);
		index_hole_count_target_ = -1;
	}

	// motor power-down
	if(index_hole_count_ == 9 && !status_.busy && has_motor_on_line()) {
		set_motor_on(false);
	}

	// head unload
	if(index_hole_count_ == 15 && !status_.busy && has_head_load_line()) {
		set_head_load_request(false);
	}
}

void WD1770::process_write_completed() {
	posit_event(Event::DataWritten);
}

#define WAIT_FOR_EVENT(mask)	resume_point_ = __LINE__; interesting_event_mask_ = mask; return; case __LINE__:
#define WAIT_FOR_TIME(ms)		resume_point_ = __LINE__; interesting_event_mask_ = Event::Timer; delay_time_ = ms * 8000; if(delay_time_) return; case __LINE__:
#define WAIT_FOR_BYTES(count)	resume_point_ = __LINE__; interesting_event_mask_ = Event::Token; distance_into_section_ = 0; return; case __LINE__: if(latest_token_.type == Token::Byte) distance_into_section_++; if(distance_into_section_ < count) { interesting_event_mask_ = Event::Token; return; }
#define BEGIN_SECTION()	switch(resume_point_) { default:
#define END_SECTION()	0; }

#define READ_ID()	\
		if(new_event_type == Event::Token) {	\
			if(!distance_into_section_ && latest_token_.type == Token::ID) {data_mode_ = DataMode::Reading; distance_into_section_++; }	\
			else if(distance_into_section_ && distance_into_section_ < 7 && latest_token_.type == Token::Byte) {	\
				header_[distance_into_section_ - 1] = latest_token_.byte_value;	\
				distance_into_section_++;	\
			}	\
		}

#define CONCATENATE(x, y) x ## y
#define INDIRECT_CONCATENATE(x, y) TOKENPASTE(x, y)
#define LINE_LABEL INDIRECT_CONCATENATE(label, __LINE__)

#define SPIN_UP()	\
		set_motor_on(true);	\
		index_hole_count_ = 0;	\
		index_hole_count_target_ = 6;	\
		WAIT_FOR_EVENT(Event::IndexHoleTarget);	\
		status_.spin_up = true;

//     +--------+----------+-------------------------+
//     !	    !	       !          BITS           !
//     ! TYPE   ! COMMAND  !  7  6	5  4  3  2  1  0 !
//     +--------+----------+-------------------------+
//     !	 1  ! Restore  !  0  0	0  0  h  v r1 r0 !
//     !	 1  ! Seek     !  0  0	0  1  h  v r1 r0 !
//     !	 1  ! Step     !  0  0	1  u  h  v r1 r0 !
//     !	 1  ! Step-in  !  0  1	0  u  h  v r1 r0 !
//     !	 1  ! Step-out !  0  1	1  u  h  v r1 r0 !
//     !	 2  ! Rd sectr !  1  0	0  m  h  E  0  0 !
//     !	 2  ! Wt sectr !  1  0	1  m  h  E  P a0 !
//     !	 3  ! Rd addr  !  1  1	0  0  h  E  0  0 !
//     !	 3  ! Rd track !  1  1	1  0  h  E  0  0 !
//     !	 3  ! Wt track !  1  1	1  1  h  E  P  0 !
//     !	 4  ! Forc int !  1  1	0  1 i3 i2 i1 i0 !
//     +--------+----------+-------------------------+

void WD1770::posit_event(Event new_event_type) {
	if(!(interesting_event_mask_ & (int)new_event_type)) return;
	interesting_event_mask_ &= ~new_event_type;

	Status new_status;
	BEGIN_SECTION()

	// Wait for a new command, branch to the appropriate handler.
	wait_for_command:
		printf("Idle...\n");
		data_mode_ = DataMode::Scanning;
		index_hole_count_ = 0;

		update_status([] (Status &status) {
			status.busy = false;
			status.interrupt_request = true;
		});

		WAIT_FOR_EVENT(Event::Command);

		update_status([] (Status &status) {
			status.busy = true;
			status.interrupt_request = false;
		});

		printf("Starting %02x\n", command_);

		if(!(command_ & 0x80)) goto begin_type_1;
		if(!(command_ & 0x40)) goto begin_type_2;
		goto begin_type_3;


	/*
		Type 1 entry point.
	*/
//     +--------+----------+-------------------------+
//     !	    !	       !          BITS           !
//     ! TYPE   ! COMMAND  !  7  6	5  4  3  2  1  0 !
//     +--------+----------+-------------------------+
//     !	 1  ! Restore  !  0  0	0  0  h  v r1 r0 !
//     !	 1  ! Seek     !  0  0	0  1  h  v r1 r0 !
//     !	 1  ! Step     !  0  0	1  u  h  v r1 r0 !
//     !	 1  ! Step-in  !  0  1	0  u  h  v r1 r0 !
//     !	 1  ! Step-out !  0  1	1  u  h  v r1 r0 !
//     +--------+----------+-------------------------+

	begin_type_1:
		// Set initial flags, skip spin-up if possible.
		update_status([] (Status &status) {
			status.type = Status::One;
			status.seek_error = false;
			status.crc_error = false;
			status.data_request = false;
		});

		if(!has_motor_on_line() && !has_head_load_line()) goto test_type1_type;

		if(has_motor_on_line()) goto begin_type1_spin_up;
		goto begin_type1_load_head;

	begin_type1_load_head:
		if(!(command_&0x08)) {
			set_head_load_request(false);
			goto test_type1_type;
		}
		set_head_load_request(true);
		if(head_is_loaded_) goto test_type1_type;
		WAIT_FOR_EVENT(Event::HeadLoad);
		goto test_type1_type;

	begin_type1_spin_up:
		if((command_&0x08) || get_motor_on()) goto test_type1_type;
		SPIN_UP();

	test_type1_type:
		// Set step direction if this is a step in or out.
		if((command_ >> 5) == 2) step_direction_ = 1;
		if((command_ >> 5) == 3) step_direction_ = 0;
		if((command_ >> 5) != 0) goto perform_step_command;

		// This is now definitely either a seek or a restore; if it's a restore then set track to 0xff and data to 0x00.
		if(!(command_ & 0x10)) {
			track_ = 0xff;
			data_ = 0;
		}

	perform_seek_or_restore_command:
		if(track_ == data_) goto verify;
		step_direction_ = (data_ > track_);

	adjust_track:
		if(step_direction_) track_++; else track_--;

	perform_step:
		if(!step_direction_ && get_is_track_zero()) {
			track_ = 0;
			goto verify;
		}
		step(step_direction_ ? 1 : -1);
		int time_to_wait;
		switch(command_ & 3) {
			default:
			case 0: time_to_wait = 6;	break;
			case 1: time_to_wait = 12;	break;
			case 2: time_to_wait = (personality_ == P1772) ? 2 : 20;	break;
			case 3: time_to_wait = (personality_ == P1772) ? 3 : 30;	break;
		}
		WAIT_FOR_TIME(time_to_wait);
		if(command_ >> 5) goto verify;
		goto perform_seek_or_restore_command;

	perform_step_command:
		if(command_ & 0x10) goto adjust_track;
		goto perform_step;

	verify:
		if(!(command_ & 0x04)) {
			goto wait_for_command;
		}

		index_hole_count_ = 0;
		distance_into_section_ = 0;

	verify_read_data:
		WAIT_FOR_EVENT(Event::IndexHole | Event::Token);
		READ_ID();

		if(index_hole_count_ == 6) {
			update_status([] (Status &status) {
				status.seek_error = true;
			});
			goto wait_for_command;
		}
		if(distance_into_section_ == 7) {
			data_mode_ = DataMode::Scanning;
			if(crc_generator_.get_value()) {
				update_status([] (Status &status) {
					status.crc_error = true;
				});
				goto verify_read_data;
			}

			if(header_[0] == track_) {
				printf("Reached track %d\n", track_);
				update_status([] (Status &status) {
					status.crc_error = false;
				});
				goto wait_for_command;
			}

			distance_into_section_ = 0;
		}
		goto verify_read_data;


	/*
		Type 2 entry point.
	*/
//     +--------+----------+-------------------------+
//     !	    !	       !          BITS           !
//     ! TYPE   ! COMMAND  !  7  6	5  4  3  2  1  0 !
//     +--------+----------+-------------------------+
//     !	 2  ! Rd sectr !  1  0	0  m  h  E  0  0 !
//     !	 2  ! Wt sectr !  1  0	1  m  h  E  P a0 !
//     +--------+----------+-------------------------+

	begin_type_2:
		update_status([] (Status &status) {
			status.type = Status::Two;
			status.lost_data = false;
			status.record_not_found = false;
			status.write_protect = false;
			status.record_type = false;
			status.data_request = false;
		});
		distance_into_section_ = 0;

		if((command_&0x08) && has_motor_on_line()) goto test_type2_delay;
		if(!has_motor_on_line() && !has_head_load_line()) goto test_type2_delay;

		if(has_motor_on_line()) goto begin_type2_spin_up;
		goto begin_type2_load_head;

	begin_type2_load_head:
		set_head_load_request(true);
		if(head_is_loaded_) goto test_type2_delay;
		WAIT_FOR_EVENT(Event::HeadLoad);
		goto test_type2_delay;

	begin_type2_spin_up:
		if(get_motor_on()) goto test_type2_delay;
		// Perform spin up.
		SPIN_UP();

	test_type2_delay:
		index_hole_count_ = 0;
		if(!(command_ & 0x04)) goto test_type2_write_protection;
		WAIT_FOR_TIME(30);

	test_type2_write_protection:
		if(command_&0x20 && get_drive_is_read_only()) {
			update_status([] (Status &status) {
				status.write_protect = true;
			});
			goto wait_for_command;
		}

	type2_get_header:
		WAIT_FOR_EVENT(Event::IndexHole | Event::Token);
		READ_ID();

		if(index_hole_count_ == 5) {
			printf("Failed to find sector %d\n", sector_);
			update_status([] (Status &status) {
				status.record_not_found = true;
			});
			goto wait_for_command;
		}
		if(distance_into_section_ == 7) {
			printf("Considering %d/%d\n", header_[0], header_[2]);
			data_mode_ = DataMode::Scanning;
			if(		header_[0] == track_ && header_[2] == sector_ &&
					(has_motor_on_line() || !(command_&0x02) || ((command_&0x08) >> 3) == header_[1])) {
				printf("Found %d/%d\n", header_[0], header_[2]);
				if(crc_generator_.get_value()) {
					printf("CRC error; back to searching\n");
					update_status([] (Status &status) {
						status.crc_error = true;
					});
					goto type2_get_header;
				}

				update_status([] (Status &status) {
					status.crc_error = false;
				});
				goto type2_read_or_write_data;
			}
			distance_into_section_ = 0;
		}
		goto type2_get_header;


	type2_read_or_write_data:
		if(command_&0x20) goto type2_write_data;
		goto type2_read_data;

	type2_read_data:
		WAIT_FOR_EVENT(Event::Token);
		// TODO: timeout
		if(latest_token_.type == Token::Data || latest_token_.type == Token::DeletedData) {
			update_status([this] (Status &status) {
				status.record_type = (latest_token_.type == Token::DeletedData);
			});
			distance_into_section_ = 0;
			data_mode_ = DataMode::Reading;
			goto type2_read_byte;
		}
		goto type2_read_data;

	type2_read_byte:
		WAIT_FOR_EVENT(Event::Token);
		if(latest_token_.type != Token::Byte) goto type2_read_byte;
		data_ = latest_token_.byte_value;
		update_status([] (Status &status) {
			status.lost_data |= status.data_request;
			status.data_request = true;
		});
		distance_into_section_++;
		if(distance_into_section_ == 128 << header_[3]) {
			distance_into_section_ = 0;
			goto type2_check_crc;
		}
		goto type2_read_byte;

	type2_check_crc:
		WAIT_FOR_EVENT(Event::Token);
		if(latest_token_.type != Token::Byte) goto type2_read_byte;
		header_[distance_into_section_] = latest_token_.byte_value;
		distance_into_section_++;
		if(distance_into_section_ == 2) {
			if(crc_generator_.get_value()) {
				printf("CRC error; terminating\n");
				update_status([this] (Status &status) {
					status.crc_error = true;
				});
				goto wait_for_command;
			}

			if(command_ & 0x10) {
				sector_++;
				goto test_type2_write_protection;
			}
			printf("Read sector %d\n", sector_);
			goto wait_for_command;
		}
		goto type2_check_crc;


	type2_write_data:
		WAIT_FOR_BYTES(2);
		update_status([] (Status &status) {
			status.data_request = true;
		});
		WAIT_FOR_BYTES(9);
		if(status_.data_request) {
			update_status([] (Status &status) {
				status.lost_data = true;
			});
			goto wait_for_command;
		}
		WAIT_FOR_BYTES(1);
		if(is_double_density_) {
			WAIT_FOR_BYTES(11);
		}

		data_mode_ = DataMode::Writing;
		begin_writing();
		for(int c = 0; c < (is_double_density_ ? 12 : 6); c++) {
			write_byte(0);
		}
		WAIT_FOR_EVENT(Event::DataWritten);

		if(is_double_density_) {
			crc_generator_.set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);
			for(int c = 0; c < 3; c++) write_raw_short(Storage::Encodings::MFM::MFMSync);
			write_byte((command_&0x01) ? Storage::Encodings::MFM::DeletedDataAddressByte : Storage::Encodings::MFM::DataAddressByte);
		} else {
			crc_generator_.reset();
			crc_generator_.add((command_&0x01) ? Storage::Encodings::MFM::DeletedDataAddressByte : Storage::Encodings::MFM::DataAddressByte);
			write_raw_short((command_&0x01) ? Storage::Encodings::MFM::FMDeletedDataAddressMark : Storage::Encodings::MFM::FMDataAddressMark);
		}

		WAIT_FOR_EVENT(Event::DataWritten);
		distance_into_section_ = 0;

	type2_write_loop:
		/*
			This deviates from the data sheet slightly since that would prima facie request one more byte
			of data than is actually written — the last time around the loop it has transferred from the
			data register to the data shift register, set data request, written the byte, checked that data
			request has been satified, then finally considers whether all bytes are done. Based on both
			natural expectations and the way that emulated machines responded, I believe that to be a
			documentation error.
		*/
		write_byte(data_);
		distance_into_section_++;
		if(distance_into_section_ == 128 << header_[3]) {
			goto type2_write_crc;
		}

		update_status([] (Status &status) {
			status.data_request = true;
		});
		WAIT_FOR_EVENT(Event::DataWritten);
		if(status_.data_request) {
			end_writing();
			update_status([] (Status &status) {
				status.lost_data = true;
			});
			goto wait_for_command;
		}

		goto type2_write_loop;

	type2_write_crc: {
			uint16_t crc = crc_generator_.get_value();
			write_byte(crc >> 8);
			write_byte(crc & 0xff);
		}
		write_byte(0xff);
		WAIT_FOR_EVENT(Event::DataWritten);
		end_writing();

		if(command_ & 0x10) {
			sector_++;
			goto test_type2_write_protection;
		}
		printf("Wrote sector %d\n", sector_);
		goto wait_for_command;


	/*
		Type 3 entry point.
	*/
//     +--------+----------+-------------------------+
//     !	    !	       !          BITS           !
//     ! TYPE   ! COMMAND  !  7  6	5  4  3  2  1  0 !
//     +--------+----------+-------------------------+
//     !	 3  ! Rd addr  !  1  1	0  0  h  E  0  0 !
//     !	 3  ! Rd track !  1  1	1  0  h  E  0  0 !
//     !	 3  ! Wt track !  1  1	1  1  h  E  P  0 !
//     +--------+----------+-------------------------+
	begin_type_3:
		update_status([] (Status &status) {
			status.type = Status::Three;
			status.crc_error = false;
			status.lost_data = false;
			status.record_not_found = false;
		});
		if(!has_motor_on_line() && !has_head_load_line()) goto type3_test_delay;

		if(has_motor_on_line()) goto begin_type3_spin_up;
		goto begin_type3_load_head;

	begin_type3_load_head:
		set_head_load_request(true);
		if(head_is_loaded_) goto type3_test_delay;
		WAIT_FOR_EVENT(Event::HeadLoad);
		goto type3_test_delay;

	begin_type3_spin_up:
		if((command_&0x08) || get_motor_on()) goto type3_test_delay;
		SPIN_UP();

	type3_test_delay:
		if(!(command_&0x04)) goto test_type3_type;
		WAIT_FOR_TIME(30);

	test_type3_type:
		if(!(command_&0x20)) goto begin_read_address;
		if(!(command_&0x10)) goto begin_read_track;
		goto begin_write_track;

	begin_read_address:
		index_hole_count_ = 0;
		distance_into_section_ = 0;

	read_address_get_header:
		WAIT_FOR_EVENT(Event::IndexHole | Event::Token);
		if(new_event_type == Event::Token) {
			if(!distance_into_section_ && latest_token_.type == Token::ID) {data_mode_ = DataMode::Reading; distance_into_section_++; }
			else if(distance_into_section_ && distance_into_section_ < 7 && latest_token_.type == Token::Byte) {
				if(status_.data_request) {
					update_status([] (Status &status) {
						status.lost_data = true;
					});
					goto wait_for_command;
				}
				header_[distance_into_section_ - 1] = data_ = latest_token_.byte_value;
				track_ = header_[0];
				update_status([] (Status &status) {
					status.data_request = true;
				});
				distance_into_section_++;

				if(distance_into_section_ == 7) {
					if(crc_generator_.get_value()) {
						update_status([] (Status &status) {
							status.crc_error = true;
						});
					}
					goto wait_for_command;
				}
			}
		}

		if(index_hole_count_ == 6) {
			update_status([] (Status &status) {
				status.record_not_found = true;
			});
			goto wait_for_command;
		}
		goto read_address_get_header;

	begin_read_track:
		WAIT_FOR_EVENT(Event::IndexHole);
		index_hole_count_ = 0;

	read_track_read_byte:
		WAIT_FOR_EVENT(Event::Token | Event::IndexHole);
		if(index_hole_count_) {
			goto wait_for_command;
		}
		if(status_.data_request) {
			update_status([] (Status &status) {
				status.lost_data = true;
			});
			goto wait_for_command;
		}
		data_ = latest_token_.byte_value;
		update_status([] (Status &status) {
			status.data_request = true;
		});
		goto read_track_read_byte;

	begin_write_track:
		update_status([] (Status &status) {
			status.data_request = false;
			status.lost_data = false;
		});

	write_track_test_write_protect:
		if(get_drive_is_read_only()) {
			update_status([] (Status &status) {
				status.write_protect = true;
			});
			goto wait_for_command;
		}

		update_status([] (Status &status) {
			status.data_request = true;
		});
		WAIT_FOR_BYTES(3);
		if(status_.data_request) {
			update_status([] (Status &status) {
				status.lost_data = true;
			});
			goto wait_for_command;
		}

		WAIT_FOR_EVENT(Event::IndexHoleTarget);
		begin_writing();
		index_hole_count_ = 0;

	write_track_write_loop:
		if(is_double_density_) {
			switch(data_) {
				case 0xf5:
					write_raw_short(Storage::Encodings::MFM::MFMSync);
					crc_generator_.set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);
				break;
				case 0xf6:
					write_raw_short(Storage::Encodings::MFM::MFMIndexSync);
				break;
				case 0xff: {
					uint16_t crc = crc_generator_.get_value();
					write_byte(crc >> 8);
					write_byte(crc & 0xff);
				} break;
				default:
					write_byte(data_);
				break;
			}
		} else {
			switch(data_) {
				case 0xf8: case 0xf9: case 0xfa: case 0xfb:
				case 0xfd: case 0xfe:
					// clock is 0xc7 = 1010 0000 0010 1010 = 0xa022
					write_raw_short(
						(uint16_t)(
							0xa022 |
							((data_ & 0x80) << 7) |
							((data_ & 0x40) << 6) |
							((data_ & 0x20) << 5) |
							((data_ & 0x10) << 4) |
							((data_ & 0x08) << 3) |
							((data_ & 0x04) << 2) |
							((data_ & 0x02) << 1) |
							(data_ & 0x01)
						)
					);
					crc_generator_.reset();
					crc_generator_.add(data_);
				break;
				case 0xfc:
					write_raw_short(Storage::Encodings::MFM::FMIndexAddressMark);
				break;
				case 0xf7: {
					uint16_t crc = crc_generator_.get_value();
					write_byte(crc >> 8);
					write_byte(crc & 0xff);
				} break;
				default:
					write_byte(data_);
				break;
			}
		}

		update_status([] (Status &status) {
			status.data_request = true;
		});
		WAIT_FOR_EVENT(Event::DataWritten);
		if(status_.data_request) {
			update_status([] (Status &status) {
				status.lost_data = true;
			});
			end_writing();
			goto wait_for_command;
		}
		if(index_hole_count_) {
			end_writing();
			goto wait_for_command;
		}

		goto write_track_write_loop;

	END_SECTION()
}

void WD1770::update_status(std::function<void(Status &)> updater) {
	if(delegate_) {
		Status old_status = status_;
		updater(status_);
		bool did_change =
			(status_.busy != old_status.busy) ||
			(status_.data_request != old_status.data_request);
		if(did_change) delegate_->wd1770_did_change_output(this);
	}
	else updater(status_);
}

void WD1770::set_head_load_request(bool head_load) {}

void WD1770::set_head_loaded(bool head_loaded) {
	head_is_loaded_ = head_loaded;
	if(head_loaded) posit_event(Event::HeadLoad);
}

void WD1770::write_bit(int bit) {
	if(is_double_density_) {
		Controller::write_bit(!bit && !last_bit_);
		Controller::write_bit(!!bit);
		last_bit_ = bit;
	} else {
		Controller::write_bit(true);
		Controller::write_bit(!!bit);
	}
}

void WD1770::write_byte(uint8_t byte) {
	for(int c = 0; c < 8; c++) write_bit((byte << c)&0x80);
	crc_generator_.add(byte);
}

void WD1770::write_raw_short(uint16_t value) {
	for(int c = 0; c < 16; c++) {
		Controller::write_bit(!!((value << c)&0x8000));
	}
}
