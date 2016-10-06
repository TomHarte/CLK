//
//  Disk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 13/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disk.hpp"
#include "../../Storage/Disk/DiskController.hpp"
#include "../../Storage/Disk/Encodings/CommodoreGCR.hpp"
#include "Utilities.hpp"

#include <limits>
#include <vector>
#include <array>

using namespace StaticAnalyser::Commodore;

class CommodoreGCRParser: public Storage::Disk::Controller {
	public:
		std::shared_ptr<Storage::Disk::Drive> drive;

		CommodoreGCRParser() : Storage::Disk::Controller(4000000, 1, 300), shift_register_(0), track_(1)
		{
			drive.reset(new Storage::Disk::Drive);
			set_drive(drive);
		}

		struct Sector
		{
			uint8_t sector, track;
			std::array<uint8_t, 256> data;
			bool header_checksum_matched;
			bool data_checksum_matched;
		};

		/*!
			Attempts to read the sector located at @c track and @c sector.

			@returns a sector if one was found; @c nullptr otherwise.
		*/
		std::shared_ptr<Sector> get_sector(uint8_t track, uint8_t sector)
		{
			int difference = (int)track - (int)track_;
			track_ = track;

			if(difference)
			{
				int direction = difference < 0 ? -1 : 1;
				difference *= 2 * direction;

				for(int c = 0; c < difference; c++) step(direction);

				unsigned int zone = 3;
				if(track >= 18) zone = 2;
				else if(track >= 25) zone = 1;
				else if(track >= 31) zone = 0;
				set_expected_bit_length(Storage::Encodings::CommodoreGCR::length_of_a_bit_in_time_zone(zone));
			}

			return get_sector(sector);
		}

	private:
		unsigned int shift_register_;
		int index_count_;
		int bit_count_;
		uint8_t track_;
		std::shared_ptr<Sector> sector_cache_[65536];

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

		std::shared_ptr<Sector> get_sector(uint8_t sector)
		{
			uint16_t sector_address = (uint16_t)((track_ << 8) | sector);
			if(sector_cache_[sector_address]) return sector_cache_[sector_address];

			std::shared_ptr<Sector> first_sector = get_next_sector();
			if(!first_sector) return first_sector;
			if(first_sector->sector == sector) return first_sector;

			while(1)
			{
				std::shared_ptr<Sector> next_sector = get_next_sector();
				if(next_sector->sector == first_sector->sector) return nullptr;
				if(next_sector->sector == sector) return next_sector;
			}
		}

		std::shared_ptr<Sector> get_next_sector()
		{
			std::shared_ptr<Sector> sector(new Sector);
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

				if(checksum == get_next_byte())
				{
					uint16_t sector_address = (uint16_t)((sector->track << 8) | sector->sector);
					sector_cache_[sector_address] = sector;
					return sector;
				}
			}

			return nullptr;
		}
};

std::list<File> StaticAnalyser::Commodore::GetFiles(const std::shared_ptr<Storage::Disk::Disk> &disk)
{
	std::list<File> files;
	CommodoreGCRParser parser;
	parser.drive->set_disk(disk);

	// find any sector whatsoever to establish the current track
	std::shared_ptr<CommodoreGCRParser::Sector> sector;

	// assemble directory
	std::vector<uint8_t> directory;
	uint8_t next_track = 18;
	uint8_t next_sector = 1;
	while(1)
	{
		sector = parser.get_sector(next_track, next_sector);
		if(!sector) break;
		directory.insert(directory.end(), sector->data.begin(), sector->data.end());
		next_track = sector->data[0];
		next_sector = sector->data[1];

		if(!next_track) break;
	}

	// parse directory
	size_t header_pointer = (size_t)-32;
	while(header_pointer+32+31 < directory.size())
	{
		header_pointer += 32;

		File new_file;
		switch(directory[header_pointer + 2] & 7)
		{
			case 0:				// DEL files
			default: continue;	// Unknown file types

			case 1:	new_file.type = File::DataSequence;			break;
			case 2:	new_file.type = File::RelocatableProgram;	break;	// TODO: need a "don't know about relocatable" program?
			case 3:	new_file.type = File::User;					break;
//			case 4:	new_file.type = File::Relative;				break;	// Can't handle REL files yet
		}

		next_track = directory[header_pointer + 3];
		next_sector = directory[header_pointer + 4];

		new_file.raw_name.reserve(16);
		for(size_t c = 0; c < 16; c++)
		{
			new_file.raw_name.push_back(directory[header_pointer + 5 + c]);
		}
		new_file.name = petscii_from_bytes(&new_file.raw_name[0], 16, false);

		size_t number_of_sectors = (size_t)directory[header_pointer + 0x1e] + ((size_t)directory[header_pointer + 0x1f] << 8);
		new_file.data.reserve((number_of_sectors - 1) * 254 + 252);

		bool is_first_sector = true;
		while(next_track)
		{
			sector = parser.get_sector(next_track, next_sector);
			if(!sector) break;

			next_track = sector->data[0];
			next_sector = sector->data[1];

			if(is_first_sector) new_file.starting_address = (uint16_t)sector->data[2] | (uint16_t)(sector->data[3] << 8);
			if(next_track)
				new_file.data.insert(new_file.data.end(), sector->data.begin() + (is_first_sector ? 4 : 2), sector->data.end());
			else
				new_file.data.insert(new_file.data.end(), sector->data.begin() + 2, sector->data.begin() + next_sector);

			is_first_sector = false;
		}

		if(!next_track) files.push_back(new_file);
	}

	return files;
}
