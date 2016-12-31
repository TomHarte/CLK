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

const uint16_t FMIndexAddressMark		= 0xf77a;	// data 0xfc, with clock 0xd7 => 1111 1100 with clock 1101 0111 => 1111 0111 0111 1010
const uint16_t FMIDAddressMark			= 0xf57e;	// data 0xfe, with clock 0xc7 => 1111 1110 with clock 1100 0111 => 1111 0101 0111 1110
const uint16_t FMDataAddressMark		= 0xf56f;	// data 0xfb, with clock 0xc7 => 1111 1011 with clock 1100 0111 => 1111 0101 0110 1111
const uint16_t FMDeletedDataAddressMark	= 0xf56a;	// data 0xf8, with clock 0xc7 => 1111 1000 with clock 1100 0111 => 1111 0101 0110 1010

const uint16_t MFMIndexSync				= 0x5224;
const uint16_t MFMSync					= 0x4489;
const uint8_t MFMIndexAddressByte		= 0xfc;
const uint8_t MFMIDAddressByte			= 0xfe;
const uint8_t MFMDataAddressByte		= 0xfb;
const uint8_t MFMDeletedDataAddressByte	= 0xf8;
const uint16_t MFMPostSyncCRCValue		= 0xcdb4;

struct Sector {
	uint8_t track, side, sector;
	std::vector<uint8_t> data;
};

std::shared_ptr<Storage::Disk::Track> GetMFMTrackWithSectors(const std::vector<Sector> &sectors);
std::shared_ptr<Storage::Disk::Track> GetFMTrackWithSectors(const std::vector<Sector> &sectors);

class Encoder {
	public:
		Encoder(std::vector<uint8_t> &target);
		virtual void add_byte(uint8_t input) = 0;
		virtual void add_index_address_mark() = 0;
		virtual void add_ID_address_mark() = 0;
		virtual void add_data_address_mark() = 0;
		virtual void add_deleted_data_address_mark() = 0;
		virtual void output_short(uint16_t value);
		void add_crc();

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
		std::shared_ptr<Storage::Encodings::MFM::Sector> get_sector(uint8_t track, uint8_t sector);

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

		std::shared_ptr<Storage::Disk::Drive> drive;
		unsigned int shift_register_;
		int index_count_;
		uint8_t track_;
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
};


}
}
}

#endif /* MFM_hpp */
