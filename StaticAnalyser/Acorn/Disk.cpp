//
//  Disk.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "Disk.hpp"
#include "../../Storage/Disk/DiskController.hpp"
#include "../../Storage/Disk/Encodings/MFM.hpp"
#include "../../NumberTheory/CRC.hpp"
#include <algorithm>

using namespace StaticAnalyser::Acorn;

class FMParser: public Storage::Disk::Controller {
	public:
		std::shared_ptr<Storage::Disk::Drive> drive;

		FMParser(bool is_mfm) :
			Storage::Disk::Controller(4000000, 1, 300),
			crc_generator_(0x1021, 0xffff),
			shift_register_(0), track_(0), is_mfm_(is_mfm)
		{
			Storage::Time bit_length;
			bit_length.length = 1;
			bit_length.clock_rate = is_mfm ? 500000 : 250000;	// i.e. 250 kbps (including clocks)
			set_expected_bit_length(bit_length);

			drive.reset(new Storage::Disk::Drive);
			set_drive(drive);
			set_motor_on(true);
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
				difference *= direction;

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
		NumberTheory::CRC16 crc_generator_;
		bool is_mfm_;

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
			uint8_t byte = (uint8_t)(
				((shift_register_&0x0001) >> 0) |
				((shift_register_&0x0004) >> 1) |
				((shift_register_&0x0010) >> 2) |
				((shift_register_&0x0040) >> 3) |
				((shift_register_&0x0100) >> 4) |
				((shift_register_&0x0400) >> 5) |
				((shift_register_&0x1000) >> 6) |
				((shift_register_&0x4000) >> 7));
			crc_generator_.add(byte);
			return byte;
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
					if(is_mfm_)
					{
						if(shift_register_ == Storage::Encodings::MFM::MFMAddressMark)
						{
							uint8_t mark = get_next_byte();
							if(mark == Storage::Encodings::MFM::MFMIDAddressByte) break;
						}
					}
					else
					{
						if(shift_register_ == Storage::Encodings::MFM::FMIDAddressMark) break;
					}
					if(index_count_ >= 2) return nullptr;
				}

				crc_generator_.reset();
				sector->track = get_next_byte();
				sector->side = get_next_byte();
				sector->sector = get_next_byte();
				uint8_t size = get_next_byte();
				uint16_t header_crc = crc_generator_.get_value();
				if((header_crc >> 8) != get_next_byte()) continue;
				if((header_crc & 0xff) != get_next_byte()) continue;

				// look for data mark
				while(1)
				{
					run_for_cycles(1);
					if(is_mfm_)
					{
						if(shift_register_ == Storage::Encodings::MFM::MFMAddressMark)
						{
							uint8_t mark = get_next_byte();
							if(mark == Storage::Encodings::MFM::MFMDataAddressByte) break;
							if(mark == Storage::Encodings::MFM::MFMIDAddressByte) return nullptr;
						}
					}
					else
					{
						if(shift_register_ == Storage::Encodings::MFM::FMDataAddressMark) break;
						if(shift_register_ == Storage::Encodings::MFM::FMIDAddressMark) return nullptr;
					}
					if(index_count_ >= 2) return nullptr;
				}

				size_t data_size = (size_t)(128 << size);
				sector->data.reserve(data_size);
				crc_generator_.reset();
				for(size_t c = 0; c < data_size; c++)
				{
					sector->data.push_back(get_next_byte());
				}
				uint16_t data_crc = crc_generator_.get_value();
				if((data_crc >> 8) != get_next_byte()) continue;
				if((data_crc & 0xff) != get_next_byte()) continue;

				return sector;
			}

			return nullptr;
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

std::unique_ptr<Catalogue> StaticAnalyser::Acorn::GetDFSCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk)
{
	// c.f. http://beebwiki.mdfs.net/Acorn_DFS_disc_format
	std::unique_ptr<Catalogue> catalogue(new Catalogue);
	FMParser parser(false);
	parser.drive->set_disk(disk);

	std::shared_ptr<Storage::Encodings::MFM::Sector> names = parser.get_sector(0, 0);
	std::shared_ptr<Storage::Encodings::MFM::Sector> details = parser.get_sector(0, 1);

	if(!names || !details) return nullptr;
	if(names->data.size() != 256 || details->data.size() != 256) return nullptr;

	uint8_t final_file_offset = details->data[5];
	if(final_file_offset&7) return nullptr;

	char disk_name[13];
	snprintf(disk_name, 13, "%.8s%.4s", &names->data[0], &details->data[0]);
	catalogue->name = disk_name;

	switch((details->data[6] >> 4)&3)
	{
		case 0: catalogue->bootOption = Catalogue::BootOption::None;		break;
		case 1: catalogue->bootOption = Catalogue::BootOption::LoadBOOT;	break;
		case 2: catalogue->bootOption = Catalogue::BootOption::RunBOOT;		break;
		case 3: catalogue->bootOption = Catalogue::BootOption::ExecBOOT;	break;
	}

	// DFS files are stored contiguously, and listed in descending order of distance from track 0.
	// So iterating backwards implies the least amount of seeking.
	for(size_t file_offset = final_file_offset - 8; file_offset > 0; file_offset -= 8)
	{
		File new_file;
		char name[10];
		snprintf(name, 10, "%c.%.7s", names->data[file_offset + 7] & 0x7f, &names->data[file_offset]);
		new_file.name = name;
		new_file.load_address = (uint32_t)(details->data[file_offset] | (details->data[file_offset+1] << 8) | ((details->data[file_offset+6]&0x0c) << 14));
		new_file.execution_address = (uint32_t)(details->data[file_offset+2] | (details->data[file_offset+3] << 8) | ((details->data[file_offset+6]&0xc0) << 10));
		new_file.is_protected = !!(names->data[file_offset + 7] & 0x80);

		long data_length = (long)(details->data[file_offset+4] | (details->data[file_offset+5] << 8) | ((details->data[file_offset+6]&0x30) << 12));
		int start_sector = details->data[file_offset+7] | ((details->data[file_offset+6]&0x03) << 8);
		new_file.data.reserve((size_t)data_length);

		if(start_sector < 2) continue;
		while(data_length > 0)
		{
			uint8_t sector = (uint8_t)(start_sector % 10);
			uint8_t track = (uint8_t)(start_sector / 10);
			start_sector++;

			std::shared_ptr<Storage::Encodings::MFM::Sector> next_sector = parser.get_sector(track, sector);
			if(!next_sector) break;

			long length_from_sector = std::min(data_length, 256l);
			new_file.data.insert(new_file.data.end(), next_sector->data.begin(), next_sector->data.begin() + length_from_sector);
			data_length -= length_from_sector;
		}
		if(!data_length) catalogue->files.push_front(new_file);
	}

	return catalogue;
}
std::unique_ptr<Catalogue> StaticAnalyser::Acorn::GetADFSCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk)
{
	std::unique_ptr<Catalogue> catalogue(new Catalogue);
	FMParser parser(true);
	parser.drive->set_disk(disk);

	std::shared_ptr<Storage::Encodings::MFM::Sector> free_space_map_second_half = parser.get_sector(0, 1);
	if(!free_space_map_second_half) return nullptr;

	std::vector<uint8_t> root_directory;
	root_directory.reserve(5 * 256);
	for(uint8_t c = 2; c < 7; c++)
	{
		std::shared_ptr<Storage::Encodings::MFM::Sector> sector = parser.get_sector(0, c);
		if(!sector) return nullptr;
		root_directory.insert(root_directory.end(), sector->data.begin(), sector->data.end());
	}

	// Quick sanity checks.
	if(root_directory[0x4cb]) return nullptr;
	if(root_directory[1] != 'H' || root_directory[2] != 'u' || root_directory[3] != 'g' || root_directory[4] != 'o') return nullptr;
	if(root_directory[0x4FB] != 'H' || root_directory[0x4FC] != 'u' || root_directory[0x4FD] != 'g' || root_directory[0x4FE] != 'o') return nullptr;

	switch(free_space_map_second_half->data[0xfd])
	{
		default: catalogue->bootOption = Catalogue::BootOption::None;		break;
		case 1: catalogue->bootOption = Catalogue::BootOption::LoadBOOT;	break;
		case 2: catalogue->bootOption = Catalogue::BootOption::RunBOOT;		break;
		case 3: catalogue->bootOption = Catalogue::BootOption::ExecBOOT;	break;
	}

	return catalogue;
}
