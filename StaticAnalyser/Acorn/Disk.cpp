//
//  Disk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disk.hpp"
#include "../../Storage/Disk/DiskDrive.hpp"
#include "../../Storage/Disk/Encodings/MFM.hpp"

using namespace StaticAnalyser::Acorn;

class FMParser: public Storage::Disk::Drive {
	public:
		FMParser() : Storage::Disk::Drive(4000000, 1, 300), shift_register_(0), track_(0)
		{
			// Make sure this drive really is at track '1'.
			while(!get_is_track_zero()) step(-1);

			Storage::Time bit_length;
			bit_length.length = 1;
			bit_length.clock_rate = 250000;	// i.e. 250 kbps
			set_expected_bit_length(bit_length);
		}

		/*!
			Attempts to read the sector located at @c track and @c sector.

			@returns a sector if one was found; @c nullptr otherwise.
		*/
		std::shared_ptr<Storage::Encodings::MFM::Sector> get_sector(uint8_t track, uint8_t sector)
		{
			int difference = (int)track - (int)track_;
			track_ = track;

			if(difference)
			{
				int direction = difference < 0 ? -1 : 1;
				difference *= 2 * direction;

				for(int c = 0; c < difference; c++) step(direction);
			}

			return get_sector(sector);
		}

	private:
		unsigned int shift_register_;
		int index_count_;
		uint8_t track_;
		int bit_count_;
		std::shared_ptr<Storage::Encodings::MFM::Sector> sector_cache_[65536];

		void process_input_bit(int value, unsigned int cycles_since_index_hole)
		{
			shift_register_ = ((shift_register_ << 1) | (unsigned int)value) & 0xffff;
			bit_count_++;
		}

		void process_index_hole()
		{
			index_count_++;
		}

		uint8_t get_next_byte()
		{
			bit_count_ = 0;
			while(bit_count_ < 16) run_for_cycles(1);
			return (uint8_t)(
				((shift_register_&0x0001) >> 0) |
				((shift_register_&0x0004) >> 1) |
				((shift_register_&0x0010) >> 2) |
				((shift_register_&0x0040) >> 3) |
				((shift_register_&0x0100) >> 4) |
				((shift_register_&0x0400) >> 5) |
				((shift_register_&0x1000) >> 6) |
				((shift_register_&0x4000) >> 7));
		}

		std::shared_ptr<Storage::Encodings::MFM::Sector> get_next_sector()
		{
			std::shared_ptr<Storage::Encodings::MFM::Sector> sector(new Storage::Encodings::MFM::Sector);
			index_count_ = 0;

			while(index_count_ < 2)
			{
				// look for an ID address mark
				while(1)
				{
					run_for_cycles(1);
					if(shift_register_ == Storage::Encodings::MFM::FMIDAddressMark) break;
					if(index_count_ >= 2) return nullptr;
				}

				sector->track = get_next_byte();
				sector->side = get_next_byte();
				sector->sector = get_next_byte();
				uint8_t size = get_next_byte();

				// look for data mark
				while(1)
				{
					run_for_cycles(1);
					if(shift_register_ == Storage::Encodings::MFM::FMDataAddressMark) break;
					if(shift_register_ == Storage::Encodings::MFM::FMIDAddressMark) return nullptr;
					if(index_count_ >= 2) return nullptr;
				}

				size_t data_size = (size_t)(128 << size);
				sector->data.reserve(data_size);
				for(size_t c = 0; c < data_size; c++)
				{
					sector->data.push_back(get_next_byte());
				}

				return sector;
			}

			return sector;
		}

		std::shared_ptr<Storage::Encodings::MFM::Sector> get_sector(uint8_t sector)
		{
//			uint16_t sector_address = (uint16_t)((track_ << 8) | sector);
//			if(sector_cache_[sector_address]) return sector_cache_[sector_address];

			std::shared_ptr<Storage::Encodings::MFM::Sector> first_sector = get_next_sector();
			if(!first_sector) return first_sector;
			if(first_sector->sector == sector) return first_sector;

			while(1)
			{
				std::shared_ptr<Storage::Encodings::MFM::Sector> next_sector = get_next_sector();
				if(next_sector->sector == first_sector->sector) return nullptr;
				if(next_sector->sector == sector) return next_sector;
			}
		}
};

std::list<File> StaticAnalyser::Acorn::GetDFSFiles(const std::shared_ptr<Storage::Disk::Disk> &disk)
{
	std::list<File> files;
	FMParser parser;
	parser.set_disk(disk);

	parser.get_sector(0, 0);

	return files;
}
