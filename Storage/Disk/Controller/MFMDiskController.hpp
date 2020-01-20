//
//  MFMDiskController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef MFMDiskController_hpp
#define MFMDiskController_hpp

#include "DiskController.hpp"
#include "../../../Numeric/CRC.hpp"
#include "../../../ClockReceiver/ClockReceiver.hpp"
#include "../Encodings/MFM/Shifter.hpp"

namespace Storage {
namespace Disk {

/*!
	Extends Controller with a built-in shift register and FM/MFM decoding logic,
	being able to post event messages to subclasses.
*/
class MFMController: public Controller {
	public:
		MFMController(Cycles clock_rate);

	protected:
		/// Indicates whether the controller should try to decode double-density MFM content, or single-density FM content.
		void set_is_double_density(bool);

		/// @returns @c true if currently decoding MFM content; @c false otherwise.
		bool get_is_double_density();

		enum DataMode {
			/// When the controller is scanning it will obey all synchronisation marks found, even if in the middle of data.
			Scanning,
			/// When the controller is reading it will ignore synchronisation marks and simply return a new token every sixteen PLL clocks.
			Reading,
			/// When the controller is writing, it will replace the underlying data with that which has been enqueued, posting Event::DataWritten when the queue is empty.
			Writing
		};
		/// Sets the current data mode.
		void set_data_mode(DataMode);

		/*!
			Describes a token found in the incoming PLL bit stream. Tokens can be one of:

				Index: the bit pattern usually encoded at the start of a track to denote the position of the index hole;
				ID: the pattern that begins an ID section, i.e. a sector header, announcing sector number, track number, etc.
				Data: the pattern that begins a data section, i.e. sector contents.
				DeletedData: the pattern that begins a deleted data section, i.e. deleted sector contents.

				Sync: MFM only; the same synchronisation mark is used in MFM to denote the bottom three of the four types
				of token listed above; this class combines notification of that mark and the distinct index sync mark.
				Both are followed by a byte to indicate type. When scanning an MFM stream, subclasses will receive an
				announcement of sync followed by an announcement of one of the above four types of token.

				Byte: reports reading of an ordinary byte, with expected timing bits.

			When the data mode is set to 'reading', only Byte tokens are returned; detection of the other kinds of token
			is suppressed. Controllers will likely want to switch data mode when receiving ID and sector contents, as
			spurious sync signals can otherwise be found in ordinary data, causing framing errors.
		*/
		struct Token {
			enum Type {
				Index, ID, Data, DeletedData, Sync, Byte
			} type;
			uint8_t byte_value;
		};
		/// @returns The most-recently read token from the surface of the disk.
		Token get_latest_token();

		/// @returns The controller's CRC generator. This is automatically fed during reading.
		CRC::CCITT &get_crc_generator();

		// Events
		enum class Event: int {
			Token			= (1 << 0),	// Indicates recognition of a new token in the flux stream. Use get_latest_token() for more details.
			IndexHole		= (1 << 1),	// Indicates the passing of a physical index hole.
			DataWritten		= (1 << 2),	// Indicates that all queued bits have been written
		};

		/*!
			Subclasses should implement this. It is called every time a new @c Event is discovered in the incoming data stream.
			Therefore it is called to announce when:

				(i) a new token is discovered in the incoming stream: an index, ID, data or deleted data, a sync mark or a new byte of data.
				(ii) the index hole passes; or
				(iii) the queue of data to be written has been exhausted.
		*/
		virtual void posit_event(int type) = 0;

		/*!
			Encodes @c bit according to the current single/double density mode and adds it
			to the controller's write buffer.
		*/
		void write_bit(int bit);

		/*!
			Encodes @c byte according to the current single/double density mode and adds it
			to the controller's write buffer.
		*/
		void write_byte(uint8_t byte);

		/*!
			Serialises @c value into the controller's write buffer without adjustment.
		*/
		void write_raw_short(uint16_t value);

		/*!
			Gets the current value of the CRC generator and makes two calls to @c write_byte, to
			write first its higher-value byte and then its lower.
		*/
		void write_crc();

		/*!
			Calls @c write_byte with @c value, @c quantity times.
		*/
		void write_n_bytes(int quantity, uint8_t value);

		/*!
			Writes everything that should per the spec appear prior to the address contained
			in an ID mark (i.e. proper gaps and the ID mark) and appropriate seeds the CRC generator.
		*/
		void write_id_joiner();

		/*!
			Writes at most what should, per the spec, appear after the ID's CRC, up to and
			including the mark that indicates the beginning of data, appropriately seeding
			the CRC generator; if @c skip_first_gap is set then the initial gap after the
			CRC isn't written.
		*/
		void write_id_data_joiner(bool is_deleted, bool skip_first_gap);

		/*!
			Writes the gap expected after a sector's data CRC and before the beginning of the
			next ID joiner.
		*/
		void write_post_data_gap();

		/*!
			Writes everything that should, per the spec, following the index hole and prior
			to any sectors.
		*/
		void write_start_of_track();

	private:
		// Storage::Disk::Controller
		virtual void process_input_bit(int value);
		virtual void process_index_hole();
		virtual void process_write_completed();

		// Reading state.
		Token latest_token_;
		Encodings::MFM::Shifter shifter_;

		// input configuration
		bool is_double_density_;
		DataMode data_mode_ = DataMode::Scanning;

		// writing
		int last_bit_;

		// CRC generator
		CRC::CCITT crc_generator_;
};

}
}

#endif /* MFMDiskController_hpp */
