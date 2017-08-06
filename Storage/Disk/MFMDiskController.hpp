//
//  MFMDiskController.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef MFMDiskController_hpp
#define MFMDiskController_hpp

#include "DiskController.hpp"
#include "../../NumberTheory/CRC.hpp"

namespace Storage {
namespace Disk {

/*!
	Extends Controller with a built-in shift register and FM/MFM decoding logic,
	being able to post event messages to subclasses.
*/
class MFMController: public Controller {
	public:
		MFMController(int clock_rate, int clock_rate_multiplier, int revolutions_per_minute);

	protected:
		void set_is_double_density(bool);
		bool get_is_double_density();

		enum DataMode {
			Scanning,
			Reading,
			Writing
		};
		void set_data_mode(DataMode);

		struct Token {
			enum Type {
				Index, ID, Data, DeletedData, Sync, Byte
			} type;
			uint8_t byte_value;
		};
		Token get_latest_token();

		// Events
		enum class Event: int {
			Command			= (1 << 0),	// Indicates receipt of a new command.
			Token			= (1 << 1),	// Indicates recognition of a new token in the flux stream. Use get_latest_token() for more details.
			IndexHole		= (1 << 2),	// Indicates the passing of a physical index hole.
			HeadLoad		= (1 << 3),	// Indicates the head has been loaded.
			DataWritten		= (1 << 4),	// Indicates that all queued bits have been written
		};
		virtual void posit_event(Event type) = 0;

	private:
		// Storage::Disk::Controller
		virtual void process_input_bit(int value, unsigned int cycles_since_index_hole);
		virtual void process_index_hole();
		virtual void process_write_completed();

		// PLL input state
		int bits_since_token_;
		int shift_register_;
		bool is_awaiting_marker_value_;

		// input configuration
		bool is_double_density_;
		DataMode data_mode_;

		// output
		Token latest_token_;

		// CRC generator
		NumberTheory::CRC16 crc_generator_;
};

}
}

#endif /* MFMDiskController_hpp */
