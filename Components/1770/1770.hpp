//
//  1770.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef _770_hpp
#define _770_hpp

#include "../../Storage/Disk/DiskController.hpp"
#include "../../NumberTheory/CRC.hpp"

namespace WD {

class WD1770: public Storage::Disk::Controller {
	public:
		enum Personality {
			P1770,	// implies automatic motor-on management with Type 2 commands offering a spin-up disable
			P1772,	// as per the 1770, with different stepping rates
			P1773,	// implements the side number-testing logic of the 1793; omits spin-up/loading logic
			P1793	// implies Type 2 commands use side number testing logic; spin-up/loading is by HLD and HLT
		};
		WD1770(Personality p);

		void set_is_double_density(bool is_double_density);
		void set_register(int address, uint8_t value);
		uint8_t get_register(int address);

		void run_for_cycles(unsigned int number_of_cycles);

		enum Flag: uint8_t {
			NotReady		= 0x80,
			MotorOn			= 0x80,
			WriteProtect	= 0x40,
			RecordType		= 0x20,
			SpinUp			= 0x20,
			HeadLoaded		= 0x20,
			RecordNotFound	= 0x10,
			SeekError		= 0x10,
			CRCError		= 0x08,
			LostData		= 0x04,
			TrackZero		= 0x04,
			DataRequest		= 0x02,
			Index			= 0x02,
			Busy			= 0x01
		};

		inline bool get_interrupt_request_line()		{	return status_.interrupt_request;	}
		inline bool get_data_request_line()				{	return status_.data_request;		}
		class Delegate {
			public:
				virtual void wd1770_did_change_output(WD1770 *wd1770) = 0;
		};
		inline void set_delegate(Delegate *delegate)	{	delegate_ = delegate;			}

	protected:
		virtual void set_head_load_request(bool head_load);
		void set_head_loaded(bool head_loaded);

	private:
		Personality personality_;
		inline bool has_motor_on_line() { return (personality_ != P1793 ) && (personality_ != P1773); }
		inline bool has_head_load_line() { return (personality_ == P1793 ); }

		struct Status {
			Status();
			bool write_protect;
			bool record_type;
			bool spin_up;
			bool record_not_found;
			bool crc_error;
			bool seek_error;
			bool lost_data;
			bool data_request;
			bool interrupt_request;
			bool busy;
			enum {
				One, Two, Three
			} type;
		} status_;
		uint8_t track_;
		uint8_t sector_;
		uint8_t data_;
		uint8_t command_;

		int index_hole_count_;
		int index_hole_count_target_;
		int bits_since_token_;
		int distance_into_section_;
		bool is_awaiting_marker_value_;

		int step_direction_;
		void update_status(std::function<void(Status &)> updater);

		// Tokeniser
		enum DataMode {
			Scanning,
			Reading,
			Writing
		} data_mode_;
		bool is_double_density_;
		int shift_register_;
		struct Token {
			enum Type {
				Index, ID, Data, DeletedData, Sync, Byte
			} type;
			uint8_t byte_value;
		} latest_token_;

		// Events
		enum Event: int {
			Command			= (1 << 0),	// Indicates receipt of a new command.
			Token			= (1 << 1),	// Indicates recognition of a new token in the flux stream. Interrogate latest_token_ for details.
			IndexHole		= (1 << 2),	// Indicates the passing of a physical index hole.
			HeadLoad		= (1 << 3),	// Indicates the head has been loaded (1973 only).
			DataWritten		= (1 << 4),	// Indicates that all queued bits have been written

			Timer			= (1 << 5),	// Indicates that the delay_time_-powered timer has timed out.
			IndexHoleTarget	= (1 << 6)	// Indicates that index_hole_count_ has reached index_hole_count_target_.
		};
		void posit_event(Event type);
		int interesting_event_mask_;
		int resume_point_;
		int delay_time_;

		// Output
		int last_bit_;
		void write_bit(int bit);
		void write_byte(uint8_t byte);
		void write_raw_short(uint16_t value);

		// ID buffer
		uint8_t header_[6];

		// CRC generator
		NumberTheory::CRC16 crc_generator_;

		// 1793 head-loading logic
		bool head_is_loaded_;

		// delegate
		Delegate *delegate_;

		// Storage::Disk::Controller
		virtual void process_input_bit(int value, unsigned int cycles_since_index_hole);
		virtual void process_index_hole();
		virtual void process_write_completed();
};

}

#endif /* _770_hpp */
