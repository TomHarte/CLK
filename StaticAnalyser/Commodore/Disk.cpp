//
//  Disk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disk.hpp"
#include "../../Storage/Disk/DiskDrive.hpp"
#include "../../Storage/Disk/Encodings/CommodoreGCR.hpp"

#include <limits>
#include <vector>
#include <array>

using namespace StaticAnalyser::Commodore;

class CommodoreGCRParser: public Storage::Disk::Drive {
	public:
		CommodoreGCRParser() : Storage::Disk::Drive(4000000, 4, 300), shift_register_(0) {}

		struct Sector
		{
			uint8_t sector, track;
			std::array<uint8_t, 256> data;
			bool header_checksum_matched;
			bool data_checksum_matched;
		};

		std::unique_ptr<Sector> get_sector(uint8_t track, uint8_t sector)
		{
			return nullptr;
		}

		std::unique_ptr<Sector> get_sector(uint8_t sector)
		{
			std::unique_ptr<Sector> first_sector = get_next_sector();
			if(!first_sector) return first_sector;
			if(first_sector->sector == sector) return first_sector;

			while(1)
			{
				std::unique_ptr<Sector> next_sector = get_next_sector();
				if(next_sector->sector == first_sector->sector) return nullptr;
				if(next_sector->sector == sector) return next_sector;
			}
		}

		std::unique_ptr<Sector> get_next_sector()
		{
			std::unique_ptr<Sector> sector(new Sector);
			index_count_ = 0;

			while(index_count_ < 2)
			{
				// look for a sector header
				while(1)
				{
					if(proceed_to_next_block() == 0x08) break;
					if(index_count_ >= 2) return nullptr;
				}

				// get sector details, skip if this looks malformed
				uint8_t checksum = (uint8_t)get_next_byte();
				sector->sector = (uint8_t)get_next_byte();
				sector->track = (uint8_t)get_next_byte();
				uint8_t disk_id[2];
				disk_id[0] = (uint8_t)get_next_byte();
				disk_id[1] = (uint8_t)get_next_byte();
				if(checksum != (sector->sector ^ sector->track ^ disk_id[0] ^ disk_id[1])) continue;

				// look for the following data
				while(1)
				{
					if(proceed_to_next_block() == 0x07) break;
					if(index_count_ >= 2) return nullptr;
				}

				checksum = 0;
				for(size_t c = 0; c < 256; c++)
				{
					sector->data[c] = (uint8_t)get_next_byte();
					checksum ^= sector->data[c];
				}

				if(checksum == get_next_byte()) return sector;
			}

			return nullptr;
		}

	private:
		unsigned int shift_register_;
		int index_count_;
		int bit_count_;

		void process_input_bit(int value, unsigned int cycles_since_index_hole)
		{
			shift_register_ = ((shift_register_ << 1) | (unsigned int)value) & 0x3ff;
			bit_count_++;
		}

		unsigned int proceed_to_next_block()
		{
			// find GCR lead-in
			proceed_to_shift_value(0x3ff);
			if(shift_register_ != 0x3ff) return 0xff;

			// find end of lead-in
			while(shift_register_ == 0x3ff && index_count_ < 2)
			{
				run_for_cycles(1);
			}

			// continue for a further nine bits
			bit_count_ = 0;
			while(bit_count_ < 9 && index_count_ < 2)
			{
				run_for_cycles(1);
			}

			return Storage::Encodings::CommodoreGCR::decoding_from_dectet(shift_register_);
		}

		unsigned int get_next_byte()
		{
			bit_count_ = 0;
			while(bit_count_ < 10) run_for_cycles(1);
			return Storage::Encodings::CommodoreGCR::decoding_from_dectet(shift_register_);
		}

		void proceed_to_shift_value(unsigned int shift_value)
		{
			index_count_ = 0;
			while(shift_register_ != shift_value && index_count_ < 2)
			{
				run_for_cycles(1);
			}
		}

		void process_index_hole()
		{
			index_count_++;
		}

};

std::list<File> StaticAnalyser::Commodore::GetFiles(const std::shared_ptr<Storage::Disk::Disk> &disk)
{
	std::list<File> files;
	CommodoreGCRParser parser;
	parser.set_disk(disk);

	// find any sector whatsoever to establish the current track
	std::unique_ptr<CommodoreGCRParser::Sector> sector;

	// attempt to grab a sector from track 0
	while(!parser.get_is_track_zero()) parser.step(-1);
	parser.set_expected_bit_length(Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(0));
	sector = parser.get_next_sector();
	if(!sector) return files;

	// step out to track 18 (== 36)
	for(int c = 0; c < 36; c++) parser.step(1);

	// assemble disk contents, starting with sector 1
	parser.set_expected_bit_length(Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(1));
	std::vector<uint8_t> directory;
	sector = parser.get_sector(1);
	while(sector)
	{
		directory.insert(directory.end(), sector->data.begin(), sector->data.end());
		uint8_t next_track = sector->data[0];
		uint8_t next_sector = sector->data[1];

		if(!next_track) break;
		sector = parser.get_sector(next_sector);

		// TODO: track changes. Allegedly not possible, but definitely happening.
	}

	return files;
}

