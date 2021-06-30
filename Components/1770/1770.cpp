//
//  1770.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "1770.hpp"

#include "../../Storage/Disk/Encodings/MFM/Constants.hpp"

#define LOG_PREFIX "[WD FDC] "
#include "../../Outputs/Log.hpp"

using namespace WD;

WD1770::WD1770(Personality p) :
		Storage::Disk::MFMController(8000000),
		personality_(p),
		interesting_event_mask_(int(Event1770::Command)) {
	set_is_double_density(false);
	posit_event(int(Event1770::Command));
}

void WD1770::write(int address, uint8_t value) {
	switch(address&3) {
		case 0: {
			if((value&0xf0) == 0xd0) {
				if(value == 0xd0) {
					// Force interrupt **immediately**.
					LOG("Force interrupt immediately");
					posit_event(int(Event1770::ForceInterrupt));
				} else {
					ERROR("!!!TODO: force interrupt!!!");
					update_status([] (Status &status) {
						status.type = Status::One;
					});
				}
			} else {
				command_ = value;
				posit_event(int(Event1770::Command));
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

uint8_t WD1770::read(int address) {
	switch(address&3) {
		default: {
			update_status([] (Status &status) {
				status.interrupt_request = false;
			});
			uint8_t status =
				(status_.crc_error ? Flag::CRCError : 0) |
				(status_.busy ? Flag::Busy : 0);

			// Per Jean Louis-Guérin's documentation:
			//
			//	* 	the write-protect bit is locked into place by a type 2 or type 3 command, but is
			//		read live after a type 1.
			//	*	the track 0 bit is captured during a type 1 instruction and lost upon any other type,
			//		it is not live sampled.
			switch(status_.type) {
				case Status::One:
					status |=
						(status_.track_zero ? Flag::TrackZero : 0) |
						(status_.seek_error ? Flag::SeekError : 0) |
						(get_drive().get_is_read_only() ? Flag::WriteProtect : 0) |
						(get_drive().get_index_pulse() ? Flag::Index : 0);
				break;

				case Status::Two:
				case Status::Three:
					status |=
						(status_.write_protect ? Flag::WriteProtect : 0) |
						(status_.record_type ? Flag::RecordType : 0) |
						(status_.lost_data ? Flag::LostData : 0) |
						(status_.data_request ? Flag::DataRequest : 0) |
						(status_.record_not_found ? Flag::RecordNotFound : 0);
				break;
			}

			if(!has_motor_on_line()) {
				status |= get_drive().get_is_ready() ? 0 : Flag::NotReady;
				if(status_.type == Status::One)
					status |= (head_is_loaded_ ? Flag::HeadLoaded : 0);
			} else {
				status |= (get_drive().get_motor_on() ? Flag::MotorOn : 0);
				if(status_.type == Status::One)
					status |= (status_.spin_up ? Flag::SpinUp : 0);
			}
//			LOG("Returned status " << PADHEX(2) << int(status) << " of type " << 1+int(status_.type));
			return status;
		}
		case 1:
			LOG("Returned track " << int(track_));
			return track_;
		case 2:
			LOG("Returned sector " << int(sector_));
			return sector_;
		case 3:
			update_status([] (Status &status) {
				status.data_request = false;
			});
		return data_;
	}
}

void WD1770::run_for(const Cycles cycles) {
	Storage::Disk::Controller::run_for(cycles);

	if(delay_time_) {
		const auto number_of_cycles = cycles.as_integral();
		if(delay_time_ <= number_of_cycles) {
			delay_time_ = 0;
			posit_event(int(Event1770::Timer));
		} else {
			delay_time_ -= number_of_cycles;
		}
	}
}

#define WAIT_FOR_EVENT(mask)	resume_point_ = __LINE__; interesting_event_mask_ = int(mask); return; case __LINE__:
#define WAIT_FOR_TIME(ms)		resume_point_ = __LINE__; delay_time_ = ms * 8000; WAIT_FOR_EVENT(Event1770::Timer);
#define WAIT_FOR_BYTES(count)	resume_point_ = __LINE__; distance_into_section_ = 0; WAIT_FOR_EVENT(Event::Token); if(get_latest_token().type == Token::Byte) distance_into_section_++; if(distance_into_section_ < count) { interesting_event_mask_ = int(Event::Token); return; }
#define BEGIN_SECTION()	switch(resume_point_) { default:
#define END_SECTION()	(void)0; }

#define READ_ID()	\
		if(new_event_type == int(Event::Token)) {	\
			if(!distance_into_section_ && get_latest_token().type == Token::ID) {\
				set_data_mode(DataMode::Reading);	\
				++distance_into_section_;	\
			} else if(distance_into_section_ && distance_into_section_ < 7 && get_latest_token().type == Token::Byte) {	\
				header_[distance_into_section_ - 1] = get_latest_token().byte_value;	\
				++distance_into_section_;	\
			}	\
		}

#define CONCATENATE(x, y) x ## y
#define INDIRECT_CONCATENATE(x, y) TOKENPASTE(x, y)
#define LINE_LABEL INDIRECT_CONCATENATE(label, __LINE__)

#define SPIN_UP()	\
		set_motor_on(true);	\
		index_hole_count_ = 0;	\
		index_hole_count_target_ = 6;	\
		WAIT_FOR_EVENT(Event1770::IndexHoleTarget);	\
		status_.spin_up = true;

// +--------+----------+-------------------------+
// !	    !	       !          BITS           !
// ! TYPE   ! COMMAND  !  7  6	5  4  3  2  1  0 !
// +--------+----------+-------------------------+
// !	 1  ! Restore  !  0  0	0  0  h  v r1 r0 !
// !	 1  ! Seek     !  0  0	0  1  h  v r1 r0 !
// !	 1  ! Step     !  0  0	1  u  h  v r1 r0 !
// !	 1  ! Step-in  !  0  1	0  u  h  v r1 r0 !
// !	 1  ! Step-out !  0  1	1  u  h  v r1 r0 !
// !	 2  ! Rd sectr !  1  0	0  m  h  E  0  0 !
// !	 2  ! Wt sectr !  1  0	1  m  h  E  P a0 !
// !	 3  ! Rd addr  !  1  1	0  0  h  E  0  0 !
// !	 3  ! Rd track !  1  1	1  0  h  E  0  0 !
// !	 3  ! Wt track !  1  1	1  1  h  E  P  0 !
// !	 4  ! Forc int !  1  1	0  1 i3 i2 i1 i0 !
// +--------+----------+-------------------------+

void WD1770::posit_event(int new_event_type) {
	if(new_event_type == int(Event::IndexHole)) {
		index_hole_count_++;
		if(index_hole_count_target_ == index_hole_count_) {
			posit_event(int(Event1770::IndexHoleTarget));
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

	if(new_event_type == int(Event1770::ForceInterrupt)) {
		interesting_event_mask_ = 0;
		resume_point_ = 0;
		update_status([] (Status &status) {
			status.type = Status::One;
			status.data_request = false;
			status.spin_up = false;
		});
	} else {
		if(!(interesting_event_mask_ & int(new_event_type))) return;
		interesting_event_mask_ &= ~new_event_type;
	}

	BEGIN_SECTION()

	// Wait for a new command, branch to the appropriate handler.
	case 0:
	wait_for_command:
		LOG("Idle...");
		set_data_mode(DataMode::Scanning);
		index_hole_count_ = 0;

		update_status([] (Status &status) {
			status.busy = false;
			status.interrupt_request = true;
		});

		WAIT_FOR_EVENT(Event1770::Command);

		update_status([] (Status &status) {
			status.busy = true;
			status.interrupt_request = false;
			status.track_zero = false;	// Always reset by a non-type 1; so reset regardless and set properly later.
		});

		LOG("Starting " << PADHEX(2) << int(command_));

		if(!(command_ & 0x80)) goto begin_type_1;
		if(!(command_ & 0x40)) goto begin_type_2;
		goto begin_type_3;


	/*
		Type 1 entry point.
	*/
	// +--------+----------+-------------------------+
	// !	    !	       !          BITS           !
	// ! TYPE   ! COMMAND  !  7  6	5  4  3  2  1  0 !
	// +--------+----------+-------------------------+
	// !	 1  ! Restore  !  0  0	0  0  h  v r1 r0 !
	// !	 1  ! Seek     !  0  0	0  1  h  v r1 r0 !
	// !	 1  ! Step     !  0  0	1  u  h  v r1 r0 !
	// !	 1  ! Step-in  !  0  1	0  u  h  v r1 r0 !
	// !	 1  ! Step-out !  0  1	1  u  h  v r1 r0 !
	// +--------+----------+-------------------------+

	begin_type_1:
		// Set initial flags, skip spin-up if possible.
		update_status([] (Status &status) {
			status.type = Status::One;
			status.seek_error = false;
			status.crc_error = false;
			status.data_request = false;
		});

		LOG("Step/Seek/Restore with track " << int(track_) << " data " << int(data_));
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
		WAIT_FOR_EVENT(Event1770::HeadLoad);
		goto test_type1_type;

	begin_type1_spin_up:
		if((command_&0x08) || get_drive().get_motor_on()) {
			set_motor_on(true);
			goto test_type1_type;
		}
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
		if(track_ == data_) goto verify_seek;
		step_direction_ = (data_ > track_);

	adjust_track:
		if(step_direction_) ++track_; else --track_;

	perform_step:
		if(!step_direction_ && get_drive().get_is_track_zero()) {
			track_ = 0;
			goto verify_seek;
		}
		get_drive().step(Storage::Disk::HeadPosition(step_direction_ ? 1 : -1));
		Cycles::IntType time_to_wait;
		switch(command_ & 3) {
			default:
			case 0: time_to_wait = 6;	break;
			case 1: time_to_wait = 12;	break;
			case 2: time_to_wait = (personality_ == P1772) ? 2 : 20;	break;
			case 3: time_to_wait = (personality_ == P1772) ? 3 : 30;	break;
		}
		WAIT_FOR_TIME(time_to_wait);
		if(command_ >> 5) goto verify_seek;
		goto perform_seek_or_restore_command;

	perform_step_command:
		if(command_ & 0x10) goto adjust_track;
		goto perform_step;

	verify_seek:
		update_status([this] (Status &status) {
			status.track_zero = get_drive().get_is_track_zero();
		});
		if(!(command_ & 0x04)) {
			goto wait_for_command;
		}

		index_hole_count_ = 0;
		distance_into_section_ = 0;

	verify_read_data:
		WAIT_FOR_EVENT(int(Event::IndexHole) | int(Event::Token));
		READ_ID();

		if(index_hole_count_ == 6) {
			LOG("Nothing found to verify");
			update_status([] (Status &status) {
				status.seek_error = true;
			});
			goto wait_for_command;
		}
		if(distance_into_section_ == 7) {
			distance_into_section_ = 0;
			set_data_mode(DataMode::Scanning);

			if(get_crc_generator().get_value()) {
				update_status([] (Status &status) {
					status.crc_error = true;
				});
				goto verify_read_data;
			}

			if(header_[0] == track_) {
				LOG("Reached track " << std::dec << int(track_));
				update_status([] (Status &status) {
					status.crc_error = false;
				});
				goto wait_for_command;
			}
		}
		goto verify_read_data;


	/*
		Type 2 entry point.
	*/
	// +--------+----------+-------------------------+
	// !	    !	       !          BITS           !
	// ! TYPE   ! COMMAND  !  7  6	5  4  3  2  1  0 !
	// +--------+----------+-------------------------+
	// !	 2  ! Rd sectr !  1  0	0  m  h  E  0  0 !
	// !	 2  ! Wt sectr !  1  0	1  m  h  E  P a0 !
	// +--------+----------+-------------------------+

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
		if(!has_motor_on_line() && !has_head_load_line()) {
			if(has_motor_on_line()) set_motor_on(true);
			goto test_type2_delay;
		}

		if(has_motor_on_line()) goto begin_type2_spin_up;
		goto begin_type2_load_head;

	begin_type2_load_head:
		set_head_load_request(true);
		if(head_is_loaded_) goto test_type2_delay;
		WAIT_FOR_EVENT(Event1770::HeadLoad);
		goto test_type2_delay;

	begin_type2_spin_up:
		if(get_drive().get_motor_on()) goto test_type2_delay;
		// Perform spin up.
		SPIN_UP();

	test_type2_delay:
		index_hole_count_ = 0;
		if(!(command_ & 0x04)) goto test_type2_write_protection;
		WAIT_FOR_TIME(30);

	test_type2_write_protection:
		if(command_&0x20 && get_drive().get_is_read_only()) {
			update_status([] (Status &status) {
				status.write_protect = true;
			});
			goto wait_for_command;
		}

		distance_into_section_ = 0;
		set_data_mode(DataMode::Scanning);

	type2_get_header:
		WAIT_FOR_EVENT(int(Event::IndexHole) | int(Event::Token));
		READ_ID();

		if(index_hole_count_ == 5) {
			LOG("Failed to find sector " << std::dec << int(sector_));
			update_status([] (Status &status) {
				status.record_not_found = true;
			});
			goto wait_for_command;
		}
		if(distance_into_section_ == 7) {
			distance_into_section_ = 0;
			set_data_mode(DataMode::Scanning);

			LOG("Considering " << std::dec << int(header_[0]) << "/" << int(header_[2]));
			if(		header_[0] == track_ && header_[2] == sector_ &&
					(has_motor_on_line() || !(command_&0x02) || ((command_&0x08) >> 3) == header_[1])) {
				LOG("Found " << std::dec << int(header_[0]) << "/" << int(header_[2]));
				if(get_crc_generator().get_value()) {
					LOG("CRC error; back to searching");
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
		}
		goto type2_get_header;


	type2_read_or_write_data:
		if(command_&0x20) goto type2_write_data;
		goto type2_read_data;

	type2_read_data:
		WAIT_FOR_EVENT(Event::Token);
		// TODO: timeout
		if(get_latest_token().type == Token::Data || get_latest_token().type == Token::DeletedData) {
			update_status([this] (Status &status) {
				status.record_type = (get_latest_token().type == Token::DeletedData);
			});
			distance_into_section_ = 0;
			set_data_mode(DataMode::Reading);
			goto type2_read_byte;
		}
		goto type2_read_data;

	type2_read_byte:
		WAIT_FOR_EVENT(Event::Token);
		if(get_latest_token().type != Token::Byte) goto type2_read_byte;
		data_ = get_latest_token().byte_value;
		update_status([] (Status &status) {
			status.lost_data |= status.data_request;
			status.data_request = true;
		});
		distance_into_section_++;
		if(distance_into_section_ == 128 << (header_[3]&3)) {
			distance_into_section_ = 0;
			goto type2_check_crc;
		}
		goto type2_read_byte;

	type2_check_crc:
		WAIT_FOR_EVENT(Event::Token);
		if(get_latest_token().type != Token::Byte) goto type2_read_byte;
		header_[distance_into_section_] = get_latest_token().byte_value;
		distance_into_section_++;
		if(distance_into_section_ == 2) {
			distance_into_section_ = 0;
			set_data_mode(DataMode::Scanning);

			if(get_crc_generator().get_value()) {
				LOG("CRC error; terminating");
				update_status([] (Status &status) {
					status.crc_error = true;
				});
				goto wait_for_command;
			}

			LOG("Finished reading sector " << std::dec << int(sector_));

			if(command_ & 0x10) {
				sector_++;
				LOG("Advancing to search for sector " << std::dec << int(sector_));
				goto test_type2_write_protection;
			}
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
		if(get_is_double_density()) {
			WAIT_FOR_BYTES(11);
		}

		set_data_mode(DataMode::Writing);
		begin_writing(false);
		for(int c = 0; c < (get_is_double_density() ? 12 : 6); c++) {
			write_byte(0);
		}
		WAIT_FOR_EVENT(Event::DataWritten);

		if(get_is_double_density()) {
			get_crc_generator().set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);
			for(int c = 0; c < 3; c++) write_raw_short(Storage::Encodings::MFM::MFMSync);
			write_byte((command_&0x01) ? Storage::Encodings::MFM::DeletedDataAddressByte : Storage::Encodings::MFM::DataAddressByte);
		} else {
			get_crc_generator().reset();
			get_crc_generator().add((command_&0x01) ? Storage::Encodings::MFM::DeletedDataAddressByte : Storage::Encodings::MFM::DataAddressByte);
			write_raw_short((command_&0x01) ? Storage::Encodings::MFM::FMDeletedDataAddressMark : Storage::Encodings::MFM::FMDataAddressMark);
		}

		WAIT_FOR_EVENT(Event::DataWritten);
		distance_into_section_ = 0;

	type2_write_loop:
		/*
			This deviates from the data sheet slightly since that would prima facie request one more byte
			of data than is actually written; the last time around the loop it has transferred from the
			data register to the data shift register, set data request, written the byte, checked that data
			request has been satified, then finally considers whether all bytes are done. Based on both
			natural expectations and the way that emulated machines responded, I believe that to be a
			documentation error.
		*/
		write_byte(data_);
		distance_into_section_++;
		if(distance_into_section_ == 128 << (header_[3]&3)) {
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

	type2_write_crc:
		write_crc();
		write_byte(0xff);
		WAIT_FOR_EVENT(Event::DataWritten);
		end_writing();

		if(command_ & 0x10) {
			sector_++;
			goto test_type2_write_protection;
		}
		LOG("Wrote sector " << std::dec << int(sector_));
		goto wait_for_command;


	/*
		Type 3 entry point.
	*/
	// +--------+----------+-------------------------+
	// !	    !	       !          BITS           !
	// ! TYPE   ! COMMAND  !  7  6	5  4  3  2  1  0 !
	// +--------+----------+-------------------------+
	// !	 3  ! Rd addr  !  1  1	0  0  h  E  0  0 !
	// !	 3  ! Rd track !  1  1	1  0  h  E  0  0 !
	// !	 3  ! Wt track !  1  1	1  1  h  E  P  0 !
	// +--------+----------+-------------------------+
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
		WAIT_FOR_EVENT(Event1770::HeadLoad);
		goto type3_test_delay;

	begin_type3_spin_up:
		if((command_&0x08) || get_drive().get_motor_on()) goto type3_test_delay;
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
		WAIT_FOR_EVENT(int(Event::IndexHole) | int(Event::Token));
		if(new_event_type == int(Event::Token)) {
			if(!distance_into_section_ && get_latest_token().type == Token::ID) {set_data_mode(DataMode::Reading); distance_into_section_++; }
			else if(distance_into_section_ && distance_into_section_ < 7 && get_latest_token().type == Token::Byte) {
				if(status_.data_request) {
					update_status([] (Status &status) {
						status.lost_data = true;
					});
					goto wait_for_command;
				}
				header_[distance_into_section_ - 1] = data_ = get_latest_token().byte_value;
				track_ = header_[0];
				update_status([] (Status &status) {
					status.data_request = true;
				});
				++distance_into_section_;

				if(distance_into_section_ == 7) {
					distance_into_section_ = 0;

					if(get_crc_generator().get_value()) {
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
		WAIT_FOR_EVENT(int(Event::Token) | int(Event::IndexHole));
		if(index_hole_count_) {
			goto wait_for_command;
		}
		if(status_.data_request) {
			update_status([] (Status &status) {
				status.lost_data = true;
			});
			goto wait_for_command;
		}
		data_ = get_latest_token().byte_value;
		update_status([] (Status &status) {
			status.data_request = true;
		});
		goto read_track_read_byte;

	begin_write_track:
		update_status([] (Status &status) {
			status.data_request = false;
			status.lost_data = false;
		});

		if(get_drive().get_is_read_only()) {
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

		WAIT_FOR_EVENT(Event1770::IndexHoleTarget);
		begin_writing(true);
		index_hole_count_ = 0;

	write_track_write_loop:
		if(get_is_double_density()) {
			switch(data_) {
				case 0xf5:
					write_raw_short(Storage::Encodings::MFM::MFMSync);
					get_crc_generator().set_value(Storage::Encodings::MFM::MFMPostSyncCRCValue);
				break;
				case 0xf6:
					write_raw_short(Storage::Encodings::MFM::MFMIndexSync);
				break;
				case 0xff:
					write_crc();
				break;
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
						uint16_t(
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
					get_crc_generator().reset();
					get_crc_generator().add(data_);
				break;
				case 0xfc:
					write_raw_short(Storage::Encodings::MFM::FMIndexAddressMark);
				break;
				case 0xf7:
					write_crc();
				break;
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
	const Status old_status = status_;

	if(delegate_) {
		updater(status_);
		const bool did_change =
			(status_.busy != old_status.busy) ||
			(status_.data_request != old_status.data_request) ||
			(status_.interrupt_request != old_status.interrupt_request);
		if(did_change) delegate_->wd1770_did_change_output(this);
	} else updater(status_);

	if(status_.busy != old_status.busy) update_clocking_observer();
}

void WD1770::set_head_load_request(bool) {}
void WD1770::set_motor_on(bool) {}

void WD1770::set_head_loaded(bool head_loaded) {
	head_is_loaded_ = head_loaded;
	if(head_loaded) posit_event(int(Event1770::HeadLoad));
}

bool WD1770::get_head_loaded() const {
	return head_is_loaded_;
}

ClockingHint::Preference WD1770::preferred_clocking() const {
	if(status_.busy) return ClockingHint::Preference::RealTime;
	return Storage::Disk::MFMController::preferred_clocking();
}
