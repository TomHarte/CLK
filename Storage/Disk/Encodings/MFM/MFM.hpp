//
//  MFM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Disk_Encodings_MFM_hpp
#define Storage_Disk_Encodings_MFM_hpp

#include <cstdint>
#include <vector>
#include "Constants.hpp"
#include "../../Disk.hpp"
#include "../../Controller/DiskController.hpp"
#include "../../../../NumberTheory/CRC.hpp"

namespace Storage {
namespace Encodings {
namespace MFM {

/*!
	Represents a single [M]FM sector, identified by its track, side and sector records, a blob of data
	and a few extra flags of metadata.
*/
struct Sector {
	uint8_t track, side, sector, size;
	std::vector<uint8_t> data;

	bool has_data_crc_error;
	bool has_header_crc_error;
	bool is_deleted;

	Sector() : track(0), side(0), sector(0), size(0), has_data_crc_error(false), has_header_crc_error(false), is_deleted(false) {}
};

extern const size_t DefaultSectorGapLength;
/*!
	Converts a vector of sectors into a properly-encoded MFM track.

	@param sectors The sectors to write.
	@param sector_gap_length If specified, sets the distance in whole bytes between each ID and its data.
	@param sector_gap_filler_byte If specified, sets the value (unencoded) that is used to populate the gap between each ID and its data.
*/
std::shared_ptr<Storage::Disk::Track> GetMFMTrackWithSectors(const std::vector<Sector> &sectors, size_t sector_gap_length = DefaultSectorGapLength, uint8_t sector_gap_filler_byte = 0x4e);

/*!
	Converts a vector of sectors into a properly-encoded FM track.

	@param sectors The sectors to write.
	@param sector_gap_length If specified, sets the distance in whole bytes between each ID and its data.
	@param sector_gap_filler_byte If specified, sets the value (unencoded) that is used to populate the gap between each ID and its data.
*/
std::shared_ptr<Storage::Disk::Track> GetFMTrackWithSectors(const std::vector<Sector> &sectors, size_t sector_gap_length = DefaultSectorGapLength, uint8_t sector_gap_filler_byte = 0x4e);

class Encoder {
	public:
		Encoder(std::vector<uint8_t> &target);
		virtual void add_byte(uint8_t input) = 0;
		virtual void add_index_address_mark() = 0;
		virtual void add_ID_address_mark() = 0;
		virtual void add_data_address_mark() = 0;
		virtual void add_deleted_data_address_mark() = 0;
		virtual void output_short(uint16_t value);

		/// Outputs the CRC for all data since the last address mask; if @c incorrectly is @c true then outputs an incorrect CRC.
		void add_crc(bool incorrectly);

	protected:
		NumberTheory::CRC16 crc_generator_;

	private:
		std::vector<uint8_t> &target_;
};

std::unique_ptr<Encoder> GetMFMEncoder(std::vector<uint8_t> &target);
std::unique_ptr<Encoder> GetFMEncoder(std::vector<uint8_t> &target);

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

#endif /* MFM_hpp */
