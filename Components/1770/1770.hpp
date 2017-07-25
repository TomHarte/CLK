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
#include "../ClockReceiver.hpp"

namespace WD {

/*!
	Provides an emulation of various Western Digital drive controllers, including the
	WD1770, WD1772, FDC1773 and FDC1793.
*/
class WD1770: public ClockReceiver<WD1770>, public Storage::Disk::Controller {
	public:
		enum Personality {
			P1770,	// implies automatic motor-on management, with Type 2 commands offering a spin-up disable
			P1772,	// as per the 1770, with different stepping rates
			P1773,	// implements the side number-testing logic of the 1793; omits spin-up/loading logic
			P1793	// implies Type 2 commands use side number testing logic; spin-up/loading is by HLD and HLT
		};

		/*!
			Constructs an instance of the drive controller that behaves according to personality @c p.
			@param p The type of controller to emulate.
		*/
		WD1770(Personality p);

		/// Sets the value of the double-density input; when @c is_double_density is @c true, reads and writes double-density format data.
		void set_is_double_density(bool is_double_density);

		/// Writes @c value to the register at @c address. Only the low two bits of the address are decoded.
		void set_register(int address, uint8_t value);

		/// Fetches the value of the register @c address. Only the low two bits of the address are decoded.
		uint8_t get_register(int address);

		/// Runs the controller for @c number_of_cycles cycles.
		void run_for(const Cycles &cycles);

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

		/// @returns The current value of the IRQ line output.
		inline bool get_interrupt_request_line()		{	return status_.interrupt_request;	}

		/// @returns The current value of the DRQ line output.
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
		unsigned int delay_time_;

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
