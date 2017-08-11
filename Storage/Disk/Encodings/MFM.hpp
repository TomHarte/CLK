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
#include "../Disk.hpp"
#include "../DiskController.hpp"
#include "../../../NumberTheory/CRC.hpp"

namespace Storage {
namespace Encodings {
namespace MFM {

const uint8_t IndexAddressByte			= 0xfc;
const uint8_t IDAddressByte				= 0xfe;
const uint8_t DataAddressByte			= 0xfb;
const uint8_t DeletedDataAddressByte	= 0xf8;

const uint16_t FMIndexAddressMark		= 0xf77a;	// data 0xfc, with clock 0xd7 => 1111 1100 with clock 1101 0111 => 1111 0111 0111 1010
const uint16_t FMIDAddressMark			= 0xf57e;	// data 0xfe, with clock 0xc7 => 1111 1110 with clock 1100 0111 => 1111 0101 0111 1110
const uint16_t FMDataAddressMark		= 0xf56f;	// data 0xfb, with clock 0xc7 => 1111 1011 with clock 1100 0111 => 1111 0101 0110 1111
const uint16_t FMDeletedDataAddressMark	= 0xf56a;	// data 0xf8, with clock 0xc7 => 1111 1000 with clock 1100 0111 => 1111 0101 0110 1010

const uint16_t MFMIndexSync				= 0x5224;	// data 0xc2, with a missing clock at 0x0080 => 0101 0010 1010 0100 without 1000 0000
const uint16_t MFMSync					= 0x4489;	// data 0xa1, with a missing clock at 0x0020 => 0100 0100 1010 1001 without 0010 0000
const uint16_t MFMPostSyncCRCValue		= 0xcdb4;	// the value the CRC generator should have after encountering three 0xa1s

const uint8_t MFMIndexSyncByteValue		= 0xc2;
const uint8_t MFMSyncByteValue			= 0xa1;

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
std::shared_ptr<Storage::Disk::Track> GetMFMTrackWithSectors(const std::vector<Sector> &sectors, size_t sector_gap_length = DefaultSectorGapLength, uint8_t sector_gap_filler_byte = 0x4e);
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
		void process_input_bit(int value, unsigned int cycles_since_index_hole);
		void process_index_hole();
		uint8_t get_next_byte();

		uint8_t get_byte_for_shift_value(uint16_t value);

		std::shared_ptr<Storage::Encodings::MFM::Sector> get_next_sector();
		std::shared_ptr<Storage::Encodings::MFM::Sector> get_sector(uint8_t sector);
		std::vector<uint8_t> get_track();

		std::map<int, std::shared_ptr<Storage::Encodings::MFM::Sector>> sectors_by_index_;
		int get_index(uint8_t head, uint8_t track, uint8_t sector);
};


}
}
}

#endif /* MFM_hpp */
