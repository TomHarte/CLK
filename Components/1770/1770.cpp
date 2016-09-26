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

WD1770::WD1770() :
	Storage::Disk::Controller(8000000, 1, 300),
	status_(0),
	interesting_event_mask_(Event::Command),
	resume_point_(0),
	delay_time_(0),
	index_hole_count_target_(-1)
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
}

void WD1770::set_register(int address, uint8_t value)
{
	switch(address&3)
	{
		case 0:
			command_ = value;
			posit_event(Event::Command);
			// TODO: is this force interrupt?
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
		default:	return status_;
		case 1:		return track_;
		case 2:		return sector_;
		case 3:		status_ &= ~Flag::DataRequest; return data_;
	}
}

void WD1770::run_for_cycles(unsigned int number_of_cycles)
{
	if(status_ & Flag::MotorOn) Storage::Disk::Controller::run_for_cycles((int)number_of_cycles);

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
		// TODO: MFM
	}

	if(token_type != Token::Byte)
	{
		latest_token_.type = token_type;
		bits_since_token_ = 0;
		posit_event(Event::Token);
		return;
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
	if(index_hole_count_ == 9 && !(status_&Flag::Busy)) status_ &= ~Flag::MotorOn;
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
			if(!distance_into_section_ && latest_token_.type == Token::ID) distance_into_section_++;	\
			else if(distance_into_section_ && distance_into_section_ < 7 && latest_token_.type == Token::Byte)	\
			{	\
				header[distance_into_section_ - 1] = latest_token_.byte_value;	\
				distance_into_section_++;	\
			}	\
		}

#define CONCATENATE(x, y) x ## y
#define INDIRECT_CONCATENATE(x, y) TOKENPASTE(x, y)
#define LINE_LABEL INDIRECT_CONCATENATE(label, __LINE__)

#define SPIN_UP()	\
		status_ |= Flag::MotorOn;	\
		index_hole_count_ = 0;	\
		index_hole_count_target_ = 6;	\
		WAIT_FOR_EVENT(Event::IndexHoleTarget);

void WD1770::posit_event(Event new_event_type)
{
	if(!(interesting_event_mask_ & (int)new_event_type)) return;
	interesting_event_mask_ &= ~new_event_type;

	BEGIN_SECTION()

	// Wait for a new command, branch to the appropriate handler.
	wait_for_command:
		printf("Idle...\n");
		status_ &= ~Flag::Busy;
		index_hole_count_ = 0;
		WAIT_FOR_EVENT(Event::Command);
		printf("Starting %02x\n", command_);
		status_ |= Flag::Busy;
		if(!(command_ & 0x80)) goto begin_type_1;
		if(!(command_ & 0x40)) goto begin_type_2;
		goto begin_type_3;


	/*
		Type 1 entry point.
	*/
	begin_type_1:
		// Set initial flags, skip spin-up if possible.
		status_ &= ~(Flag::DataRequest | Flag::DataRequest | Flag::SeekError);
		set_interrupt_request(false);
		if((command_&0x08) || (status_ & Flag::MotorOn)) goto test_type1_type;

		// Perform spin up.
		SPIN_UP();
		status_ |= Flag::SpinUp;

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
			case 0: time_to_wait = 6;	break;	// 2 on a 1772
			case 1: time_to_wait = 12;	break;	// 3 on a 1772
			case 2: time_to_wait = 20;	break;	// 5 on a 1772
			case 3: time_to_wait = 30;	break;	// 6 on a 1772
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
			set_interrupt_request(true);
			goto wait_for_command;
		}

		index_hole_count_ = 0;

	verify_read_data:
		WAIT_FOR_EVENT(Event::IndexHole | Event::Token);
		READ_ID();

		if(index_hole_count_ == 6)
		{
			set_interrupt_request(true);
			status_ |= Flag::SeekError;
			goto wait_for_command;
		}
		if(distance_into_section_ == 7)
		{
			// TODO: CRC check
			if(header[0] == track_)
			{
				status_ &= ~Flag::CRCError;
				set_interrupt_request(true);
				goto wait_for_command;
			}

			distance_into_section_ = 0;
		}
		goto verify_read_data;


	/*
		Type 2 entry point.
	*/
	begin_type_2:
		status_ &= ~(Flag::DataRequest | Flag::LostData | Flag::RecordNotFound | Flag::WriteProtect | Flag::RecordType);
		set_interrupt_request(false);
		distance_into_section_ = 0;
		if((command_&0x08) || (status_ & Flag::MotorOn)) goto test_type2_delay;

		// Perform spin up.
		SPIN_UP();

	test_type2_delay:
		index_hole_count_ = 0;
		if(!(command_ & 0x04)) goto test_type2_write_protection;
		WAIT_FOR_TIME(30);

	test_type2_write_protection:
		if(command_&0x20) // TODO:: && is_write_protected
		{
			set_interrupt_request(true);
			status_ |= Flag::WriteProtect;
			goto wait_for_command;
		}

	type2_get_header:
		WAIT_FOR_EVENT(Event::IndexHole | Event::Token);
		READ_ID();

		if(index_hole_count_ == 5)
		{
			set_interrupt_request(true);
			status_ |= Flag::RecordNotFound;
			goto wait_for_command;
		}
		if(distance_into_section_ == 7)
		{
			if(header[0] == track_ && header[2] == sector_)
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
			status_ |= (latest_token_.type == Token::DeletedData) ? Flag::RecordType : 0;
			distance_into_section_ = 0;
			goto type2_read_byte;
		}
		goto type2_read_data;

	type2_read_byte:
		WAIT_FOR_EVENT(Event::Token);
		if(latest_token_.type != Token::Byte) goto type2_read_byte;
		if(status_ & Flag::DataRequest) status_ |= Flag::LostData;
		data_ = latest_token_.byte_value;
		status_ |= Flag::DataRequest;
		distance_into_section_++;
		if(distance_into_section_ == 128 << header[3])
		{
			distance_into_section_ = 0;
			goto type2_check_crc;
		}
		goto type2_read_byte;

	type2_check_crc:
		WAIT_FOR_EVENT(Event::Token);
		if(latest_token_.type != Token::Byte) goto type2_read_byte;
		header[distance_into_section_] = latest_token_.byte_value;
		distance_into_section_++;
		if(distance_into_section_ == 2)
		{
			// TODO: check CRC
			if(command_ & 0x10)
			{
				sector_++;
				goto test_type2_write_protection;
			}
			set_interrupt_request(true);
			goto wait_for_command;
		}
		goto type2_check_crc;


	type2_write_data:
		printf("!!!TODO: data portion of sector!!!\n");

	begin_type_3:
		printf("!!!TODO: type 3 commands!!!\n");


	END_SECTION()
}
