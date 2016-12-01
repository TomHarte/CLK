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
	busy(false)
{}

WD1770::WD1770(Personality p) :
	Storage::Disk::Controller(8000000, 16, 300),
	interesting_event_mask_(Event::Command),
	resume_point_(0),
	delay_time_(0),
	index_hole_count_target_(-1),
	is_awaiting_marker_value_(false),
	is_reading_data_(false),
	delegate_(nullptr),
	personality_(p)
{
	set_is_double_density(false);
	posit_event(Event::Command);
}

void WD1770::set_is_double_density(bool is_double_density)
{
	is_double_density_ = is_double_density;
	Storage::Time bit_length;
	bit_length.length = 1;
	bit_length.clock_rate = is_double_density ? 500000 : 250000;
	set_expected_bit_length(bit_length);

	if(!is_double_density) is_awaiting_marker_value_ = false;
}

void WD1770::set_register(int address, uint8_t value)
{
	switch(address&3)
	{
		case 0:
		{
			if((value&0xf0) == 0xd0)
			{
				printf("!!!TODO: force interrupt!!!\n");
				update_status([] (Status &status) {
					status.type = Status::One;
				});
			}
			else
			{
				command_ = value;
				posit_event(Event::Command);
			}
		}
		break;
		case 1:		track_ = value;		break;
		case 2:		sector_ = value;	break;
		case 3:		data_ = value;		break;
	}
}

uint8_t WD1770::get_register(int address)
{
	switch(address&3)
	{
		default:
		{
			uint8_t status =
					(status_.write_protect ? Flag::WriteProtect : 0) |
					(status_.crc_error ? Flag::CRCError : 0) |
					(status_.busy ? Flag::Busy : 0);
			switch(status_.type)
			{
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

			if(!has_motor_on_line())
			{
				// TODO: sample ready line for bit 7
				// TODO: report head loaded if reporting a Type 1 status
			}
			else
			{
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

void WD1770::run_for_cycles(unsigned int number_of_cycles)
{
	Storage::Disk::Controller::run_for_cycles((int)number_of_cycles);

	if(delay_time_)
	{
		if(delay_time_ <= number_of_cycles)
		{
			delay_time_ = 0;
			posit_event(Event::Timer);
		}
		else
		{
			delay_time_ -= number_of_cycles;
		}
	}
}

void WD1770::process_input_bit(int value, unsigned int cycles_since_index_hole)
{
	shift_register_ = (shift_register_ << 1) | value;
	bits_since_token_++;

	Token::Type token_type = Token::Byte;
	if(!is_reading_data_)
	{
		if(!is_double_density_)
		{
			switch(shift_register_ & 0xffff)
			{
				case Storage::Encodings::MFM::FMIndexAddressMark:
					token_type = Token::Index;
				break;
				case Storage::Encodings::MFM::FMIDAddressMark:
					token_type = Token::ID;
				break;
				case Storage::Encodings::MFM::FMDataAddressMark:
					token_type = Token::Data;
				break;
				case Storage::Encodings::MFM::FMDeletedDataAddressMark:
					token_type = Token::DeletedData;
				break;
				default:
				break;
			}
		}
		else
		{
			switch(shift_register_ & 0xffff)
			{
				case Storage::Encodings::MFM::MFMIndexAddressMark:
					bits_since_token_ = 0;
					is_awaiting_marker_value_ = true;
				return;
				case Storage::Encodings::MFM::MFMAddressMark:
					bits_since_token_ = 0;
					is_awaiting_marker_value_ = true;
				return;
				default:
				break;
			}
		}

		if(token_type != Token::Byte)
		{
			latest_token_.type = token_type;
			bits_since_token_ = 0;
			posit_event(Event::Token);
			return;
		}
	}

	if(bits_since_token_ == 16)
	{
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

		if(is_awaiting_marker_value_ && is_double_density_)
		{
			is_awaiting_marker_value_ = false;
			switch(latest_token_.byte_value)
			{
				case Storage::Encodings::MFM::MFMIndexAddressByte:
					latest_token_.type = Token::Index;
				break;
				case Storage::Encodings::MFM::MFMIDAddressByte:
					latest_token_.type = Token::ID;
				break;
				case Storage::Encodings::MFM::MFMDataAddressByte:
					latest_token_.type = Token::Data;
				break;
				case Storage::Encodings::MFM::MFMDeletedDataAddressByte:
					latest_token_.type = Token::DeletedData;
				break;
				default: break;
			}
		}

		posit_event(Event::Token);
		return;
	}
}

void WD1770::process_index_hole()
{
	index_hole_count_++;
	posit_event(Event::IndexHole);
	if(index_hole_count_target_ == index_hole_count_)
	{
		posit_event(Event::IndexHoleTarget);
		index_hole_count_target_ = -1;
	}

	// motor power-down
	if(index_hole_count_ == 9 && !status_.busy && has_motor_on_line())
	{
		set_motor_on(false);
	}
}

//     +------+----------+-------------------------+
//     !	    !	       !	   BITS 	 !
//     ! TYPE ! COMMAND  !  7  6	5  4  3  2  1  0 !
//     +------+----------+-------------------------+
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
//     +------+----------+-------------------------+

#define WAIT_FOR_EVENT(mask)	resume_point_ = __LINE__; interesting_event_mask_ = mask; return; case __LINE__:
#define WAIT_FOR_TIME(ms)		resume_point_ = __LINE__; interesting_event_mask_ = Event::Timer; delay_time_ = ms * 8000; if(delay_time_) return; case __LINE__:
#define BEGIN_SECTION()	switch(resume_point_) { default:
#define END_SECTION()	0; }

#define READ_ID()	\
		if(new_event_type == Event::Token)	\
		{	\
			if(!distance_into_section_ && latest_token_.type == Token::ID) {is_reading_data_ = true; distance_into_section_++; }	\
			else if(distance_into_section_ && distance_into_section_ < 7 && latest_token_.type == Token::Byte)	\
			{	\
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

void WD1770::posit_event(Event new_event_type)
{
	if(!(interesting_event_mask_ & (int)new_event_type)) return;
	interesting_event_mask_ &= ~new_event_type;

	Status new_status;
	BEGIN_SECTION()

	// Wait for a new command, branch to the appropriate handler.
	wait_for_command:
		printf("Idle...\n");
		is_reading_data_ = false;
		index_hole_count_ = 0;

		if(!has_motor_on_line())
		{
			// TODO: ???
			set_motor_on(false);
		}

		update_status([] (Status &status) {
			status.busy = false;
		});

		WAIT_FOR_EVENT(Event::Command);

		update_status([] (Status &status) {
			status.busy = true;
		});

		printf("Starting %02x\n", command_);

		if(!has_motor_on_line())
		{
			// TODO: set HDL, wait for HDT
			set_motor_on(true);
		}

		if(!(command_ & 0x80)) goto begin_type_1;
		if(!(command_ & 0x40)) goto begin_type_2;
		goto begin_type_3;


	/*
		Type 1 entry point.
	*/
	begin_type_1:
		// Set initial flags, skip spin-up if possible.
		update_status([] (Status &status) {
			status.type = Status::One;
			status.seek_error = false;
			status.crc_error = false;
			status.data_request = false;
		});

		if((command_&0x08) || get_motor_on() || !has_motor_on_line()) goto test_type1_type;

		// Perform spin up.
		SPIN_UP();

	test_type1_type:
		// Set step direction if this is a step in or out.
		if((command_ >> 5) == 2) step_direction_ = 1;
		if((command_ >> 5) == 3) step_direction_ = 0;
		if((command_ >> 5) != 0) goto perform_step_command;

		// This is now definitely either a seek or a restore; if it's a restore then set track to 0xff and data to 0x00.
		if(!(command_ & 0x10))
		{
			track_ = 0xff;
			data_ = 0;
		}

	perform_seek_or_restore_command:
		if(track_ == data_) goto verify;
		step_direction_ = (data_ > track_);

	adjust_track:
		if(step_direction_) track_++; else track_--;

	perform_step:
		if(!step_direction_ && get_is_track_zero())
		{
			track_ = 0;
			goto verify;
		}
		step(step_direction_ ? 1 : -1);
		int time_to_wait;
		switch(command_ & 3)
		{
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
		if(!(command_ & 0x04))
		{
			goto wait_for_command;
		}

		index_hole_count_ = 0;
		distance_into_section_ = 0;

	verify_read_data:
		WAIT_FOR_EVENT(Event::IndexHole | Event::Token);
		READ_ID();

		if(index_hole_count_ == 6)
		{
			update_status([] (Status &status) {
				status.seek_error = true;
			});
			goto wait_for_command;
		}
		if(distance_into_section_ == 7)
		{
			is_reading_data_ = false;
			// TODO: CRC check
			if(header_[0] == track_)
			{
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

		if((command_&0x08) || get_motor_on() || !has_motor_on_line()) goto test_type2_delay;

		// Perform spin up.
		SPIN_UP();

	test_type2_delay:
		index_hole_count_ = 0;
		if(!(command_ & 0x04)) goto test_type2_write_protection;
		WAIT_FOR_TIME(30);

	test_type2_write_protection:
		if(command_&0x20) // TODO:: && is_write_protected
		{
			update_status([] (Status &status) {
				status.write_protect = true;
			});
			goto wait_for_command;
		}

	type2_get_header:
		WAIT_FOR_EVENT(Event::IndexHole | Event::Token);
		READ_ID();

		if(index_hole_count_ == 5)
		{
			update_status([] (Status &status) {
				status.record_not_found = true;
			});
			goto wait_for_command;
		}
		if(distance_into_section_ == 7)
		{
			is_reading_data_ = false;
			if(header_[0] == track_ && header_[2] == sector_ &&
				(has_motor_on_line() || !(command_&0x02) || ((command_&0x08) >> 3) == header_[1]))
			{
				// TODO: test CRC
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
		if(latest_token_.type == Token::Data || latest_token_.type == Token::DeletedData)
		{
			update_status([this] (Status &status) {
				status.record_type = (latest_token_.type == Token::DeletedData);
			});
			distance_into_section_ = 0;
			is_reading_data_ = true;
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
		if(distance_into_section_ == 128 << header_[3])
		{
			distance_into_section_ = 0;
			goto type2_check_crc;
		}
		goto type2_read_byte;

	type2_check_crc:
		WAIT_FOR_EVENT(Event::Token);
		if(latest_token_.type != Token::Byte) goto type2_read_byte;
		header_[distance_into_section_] = latest_token_.byte_value;
		distance_into_section_++;
		if(distance_into_section_ == 2)
		{
			// TODO: check CRC
			if(command_ & 0x10)
			{
				sector_++;
				goto test_type2_write_protection;
			}
			printf("Read sector %d\n", sector_);
			goto wait_for_command;
		}
		goto type2_check_crc;


	type2_write_data:
		printf("!!!TODO: data portion of sector!!!\n");

	begin_type_3:
		update_status([] (Status &status) {
			status.type = Status::Three;
		});
		printf("!!!TODO: type 3 commands!!!\n");


	END_SECTION()
}

void WD1770::update_status(std::function<void(Status &)> updater)
{
	if(delegate_)
	{
		Status old_status = status_;
		updater(status_);
		bool did_change =
			(status_.busy != old_status.busy) ||
			(status_.data_request != old_status.data_request);
		if(did_change) delegate_->wd1770_did_change_output(this);
	}
	else updater(status_);
}
