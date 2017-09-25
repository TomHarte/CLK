//
//  Parser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Parser_hpp
#define Parser_hpp

#include "Sector.hpp"
#include "../../Controller/DiskController.hpp"
#include "../../../../NumberTheory/CRC.hpp"

namespace Storage {
namespace Encodings {
namespace MFM {

class Parser: public Storage::Disk::Controller {
	public:
		Parser(bool is_mfm, const std::shared_ptr<Storage::Disk::Disk> &disk);
		Parser(bool is_mfm, const std::shared_ptr<Storage::Disk::Track> &track);

		/*!
			Attempts to read the sector located at @c track and @c sector.

			@returns a sector if one was found; @c nullptr otherwise.
		*/
		std::shared_ptr<Storage::Encodings::MFM::Sector> get_sector(uint8_t head, uint8_t track, uint8_t sector);

		/*!
			Attempts to read the track at @c track, starting from the index hole.

			Decodes data bits only; clocks are omitted. Synchronisation values begin a new
			byte. If a synchronisation value begins partway through a byte then
			synchronisation-contributing bits will appear both in the preceding byte and
			in the next.

			@returns a vector of data found.
		*/
		std::vector<uint8_t> get_track(uint8_t track);

	private:
		Parser(bool is_mfm);

		std::shared_ptr<Storage::Disk::Drive> drive_;
		unsigned int shift_register_;
		int index_count_;
		uint8_t track_, head_;
		int bit_count_;
		NumberTheory::CRC16 crc_generator_;
		bool is_mfm_;

		void seek_to_track(uint8_t track);
		void process_input_bit(int value);
		void process_index_hole();
		uint8_t get_next_byte();

		uint8_t get_byte_for_shift_value(uint16_t value);

		std::shared_ptr<Storage::Encodings::MFM::Sector> get_next_sector();
		std::shared_ptr<Storage::Encodings::MFM::Sector> get_sector(uint8_t sector);
		std::vector<uint8_t> get_track();

		std::map<int, std::shared_ptr<Storage::Encodings::MFM::Sector>> sectors_by_index_;
		std::set<int> decoded_tracks_;
		int get_index(uint8_t head, uint8_t track, uint8_t sector);
};

}
}
}

#endif /* Parser_hpp */
