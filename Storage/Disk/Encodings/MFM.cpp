//
//  MFM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "MFM.hpp"

#import "../PCMTrack.hpp"
#import "../../../NumberTheory/CRC.hpp"

using namespace Storage::Encodings::MFM;

template <class T> class Shifter {
	public:
		virtual void add_byte(uint8_t input) = 0;
		virtual void add_index_address_mark() = 0;
		virtual void add_ID_address_mark() = 0;
		virtual void add_data_address_mark() = 0;
		virtual void add_deleted_data_address_mark() = 0;

	protected:
		/*!
			Intended to be overridden by subclasses; should write value out as PCM data,
			MSB first.
		*/
		void output_short(uint16_t value);
};

template <class T> class MFMShifter: public Shifter<T> {
	public:
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
			static_cast<T *>(this)->output_short(output_);
		}

		void add_index_address_mark() {
			static_cast<T *>(this)->output_short(output_ = 0x5224);
			add_byte(0xfc);
		}

		void add_ID_address_mark() {
			static_cast<T *>(this)->output_short(output_ = 0x4489);
			add_byte(0xfe);
		}

		void add_data_address_mark() {
			static_cast<T *>(this)->output_short(output_ = 0x4489);
			add_byte(0xfb);
		}

		void add_deleted_data_address_mark() {
			static_cast<T *>(this)->output_short(output_ = 0x4489);
			add_byte(0xf8);
		}

	private:
		uint16_t output_;
};

template <class T> class FMShifter: public Shifter<T> {
	// encodes each 16-bit part as clock, data, clock, data [...]
	public:
		void add_byte(uint8_t input) {
			static_cast<T *>(this)->output_short(
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

		void add_index_address_mark() {
			// data 0xfc, with clock 0xd7 => 1111 1100 with clock 1101 0111 => 1111 0111 0111 1010
			static_cast<T *>(this)->output_short(0xf77a);
		}

		void add_ID_address_mark() {
			// data 0xfe, with clock 0xc7 => 1111 1110 with clock 1100 0111 => 1111 0101 0111 1110
			static_cast<T *>(this)->output_short(0xf57e);
		}

		void add_data_address_mark() {
			// data 0xfb, with clock 0xc7 => 1111 1011 with clock 1100 0111 => 1111 0101 0110 1111
			static_cast<T *>(this)->output_short(0xf56f);
		}

		void add_deleted_data_address_mark() {
			// data 0xf8, with clock 0xc7 => 1111 1000 with clock 1100 0111 => 1111 0101 0110 1010
			static_cast<T *>(this)->output_short(0xf56a);
		}
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
		size_t inter_sector_gap)
{
	T shifter;
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

	// TODO: total size check

	Storage::Disk::PCMSegment segment;
	return std::shared_ptr<Storage::Disk::Track>(new Storage::Disk::PCMTrack(std::move(segment)));
}

struct VectorReceiver {
	void output_short(uint16_t value) {
		data.push_back(value >> 8);
		data.push_back(value & 0xff);
	}
	std::vector<uint8_t> data;
};

std::shared_ptr<Storage::Disk::Track> Storage::Encodings::MFM::GetFMTrackWithSectors(const std::vector<Sector> &sectors)
{
	struct VectorShifter: public FMShifter<VectorShifter>, VectorReceiver {
		using VectorReceiver::output_short;
	};
	return GetTrackWithSectors<VectorShifter>(
		sectors,
		16, 0x00,
		6, 0,
		17, 14,
		0);
}

std::shared_ptr<Storage::Disk::Track> Storage::Encodings::MFM::GetMFMTrackWithSectors(const std::vector<Sector> &sectors)
{
	struct VectorShifter: public MFMShifter<VectorShifter>, VectorReceiver {
		using VectorReceiver::output_short;
	};
	return GetTrackWithSectors<VectorShifter>(
		sectors,
		50, 0x4e,
		12, 22,
		12, 18,
		32);
}
