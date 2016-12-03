//
//  MFM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "MFM.hpp"

#include "../PCMTrack.hpp"
#include "../../../NumberTheory/CRC.hpp"

using namespace Storage::Encodings::MFM;

class MFMEncoder: public Encoder {
	public:
		MFMEncoder(std::vector<uint8_t> &target) : Encoder(target) {}

		void add_byte(uint8_t input) {
			uint16_t spread_value =
				(uint16_t)(
					((input & 0x01) << 0) |
					((input & 0x02) << 1) |
					((input & 0x04) << 2) |
					((input & 0x08) << 3) |
					((input & 0x10) << 4) |
					((input & 0x20) << 5) |
					((input & 0x40) << 6) |
					((input & 0x80) << 7)
				);
			uint16_t or_bits = (uint16_t)((spread_value << 1) | (spread_value >> 1) | (output_ << 15));
			output_ = spread_value | ((~or_bits) & 0xaaaa);
			output_short(output_);
		}

		void add_index_address_mark() {
			output_short(output_ = MFMIndexAddressMark);
			add_byte(MFMIndexAddressByte);
		}

		void add_ID_address_mark() {
			output_short(output_ = MFMAddressMark);
			add_byte(MFMIDAddressByte);
		}

		void add_data_address_mark() {
			output_short(output_ = MFMAddressMark);
			add_byte(MFMDataAddressByte);
		}

		void add_deleted_data_address_mark() {
			output_short(output_ = MFMAddressMark);
			add_byte(MFMDeletedDataAddressByte);
		}

	private:
		uint16_t output_;
};

class FMEncoder: public Encoder {
	// encodes each 16-bit part as clock, data, clock, data [...]
	public:
		FMEncoder(std::vector<uint8_t> &target) : Encoder(target) {}

		void add_byte(uint8_t input) {
			output_short(
				(uint16_t)(
					((input & 0x01) << 0) |
					((input & 0x02) << 1) |
					((input & 0x04) << 2) |
					((input & 0x08) << 3) |
					((input & 0x10) << 4) |
					((input & 0x20) << 5) |
					((input & 0x40) << 6) |
					((input & 0x80) << 7) |
					0xaaaa
				));
		}

		void add_index_address_mark()			{	output_short(FMIndexAddressMark);		}
		void add_ID_address_mark()				{	output_short(FMIDAddressMark);			}
		void add_data_address_mark()			{	output_short(FMDataAddressMark);		}
		void add_deleted_data_address_mark()	{	output_short(FMDeletedDataAddressMark);	}
};

static uint8_t logarithmic_size_for_size(size_t size)
{
	switch(size)
	{
		default:	return 0;
		case 256:	return 1;
		case 512:	return 2;
		case 1024:	return 3;
		case 2048:	return 4;
		case 4196:	return 5;
	}
}

template<class T> std::shared_ptr<Storage::Disk::Track>
	GetTrackWithSectors(
		const std::vector<Sector> &sectors,
		size_t post_index_address_mark_bytes, uint8_t post_index_address_mark_value,
		size_t pre_address_mark_bytes, size_t post_address_mark_bytes,
		size_t pre_data_mark_bytes, size_t post_data_bytes,
		size_t inter_sector_gap,
		size_t expected_track_bytes)
{
	Storage::Disk::PCMSegment segment;
	segment.data.reserve(expected_track_bytes);
	T shifter(segment.data);
	NumberTheory::CRC16 crc_generator(0x1021, 0xffff);

	// output the index mark
	shifter.add_index_address_mark();

	// add the post-index mark
	for(int c = 0; c < post_index_address_mark_bytes; c++) shifter.add_byte(post_index_address_mark_value);

	// add sectors
	for(const Sector &sector : sectors)
	{
		// gap
		for(int c = 0; c < pre_address_mark_bytes; c++) shifter.add_byte(0x00);

		// sector header
		shifter.add_ID_address_mark();
		shifter.add_byte(sector.track);
		shifter.add_byte(sector.side);
		shifter.add_byte(sector.sector);
		uint8_t size = logarithmic_size_for_size(sector.data.size());
		shifter.add_byte(size);

		// header CRC
		crc_generator.reset();
		crc_generator.add(sector.track);
		crc_generator.add(sector.side);
		crc_generator.add(sector.sector);
		crc_generator.add(size);
		uint16_t crc_value = crc_generator.get_value();
		shifter.add_byte(crc_value >> 8);
		shifter.add_byte(crc_value & 0xff);

		// gap
		for(int c = 0; c < post_address_mark_bytes; c++) shifter.add_byte(0x4e);
		for(int c = 0; c < pre_data_mark_bytes; c++) shifter.add_byte(0x00);

		// data
		shifter.add_data_address_mark();
		crc_generator.reset();
		for(size_t c = 0; c < sector.data.size(); c++)
		{
			shifter.add_byte(sector.data[c]);
			crc_generator.add(sector.data[c]);
		}

		// data CRC
		crc_value = crc_generator.get_value();
		shifter.add_byte(crc_value >> 8);
		shifter.add_byte(crc_value & 0xff);

		// gap
		for(int c = 0; c < post_data_bytes; c++) shifter.add_byte(0x00);
		for(int c = 0; c < inter_sector_gap; c++) shifter.add_byte(0x4e);
	}

	while(segment.data.size() < expected_track_bytes) shifter.add_byte(0x00);

	segment.number_of_bits = (unsigned int)(segment.data.size() * 8);
	return std::shared_ptr<Storage::Disk::Track>(new Storage::Disk::PCMTrack(std::move(segment)));
}

Encoder::Encoder(std::vector<uint8_t> &target) :
	target_(target)
{}

void Encoder::output_short(uint16_t value)
{
	target_.push_back(value >> 8);
	target_.push_back(value & 0xff);
}

std::shared_ptr<Storage::Disk::Track> Storage::Encodings::MFM::GetFMTrackWithSectors(const std::vector<Sector> &sectors)
{
	return GetTrackWithSectors<FMEncoder>(
		sectors,
		16, 0x00,
		6, 0,
		17, 14,
		0,
		6250);	// i.e. 250kbps (including clocks) * 60 = 15000kpm, at 300 rpm => 50 kbits/rotation => 6250 bytes/rotation
}

std::shared_ptr<Storage::Disk::Track> Storage::Encodings::MFM::GetMFMTrackWithSectors(const std::vector<Sector> &sectors)
{
	return GetTrackWithSectors<MFMEncoder>(
		sectors,
		50, 0x4e,
		12, 22,
		12, 18,
		32,
		12500);	// unintelligently: double the single-density bytes/rotation (or: 500kps @ 300 rpm)
}

std::unique_ptr<Encoder> Storage::Encodings::MFM::GetMFMEncoder(std::vector<uint8_t> &target)
{
	return std::unique_ptr<Encoder>(new MFMEncoder(target));
}

std::unique_ptr<Encoder> Storage::Encodings::MFM::GetFMEncoder(std::vector<uint8_t> &target)
{
	return std::unique_ptr<Encoder>(new FMEncoder(target));
}
