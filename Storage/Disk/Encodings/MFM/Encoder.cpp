//
//  MFM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "Encoder.hpp"

#include "Constants.hpp"
#include "Storage/Disk/Track/PCMTrack.hpp"
#include "Numeric/CRC.hpp"
#include "Numeric/BitSpread.hpp"

#include <cassert>
#include <set>

using namespace Storage::Encodings::MFM;

namespace {

std::vector<const Sector *> sector_pointers(const std::vector<Sector> &sectors) {
	std::vector<const Sector *> pointers;
	for(const Sector &sector: sectors) {
		pointers.push_back(&sector);
	}
	return pointers;
}

template <Density density> struct Defaults;
template <> struct Defaults<Density::Single> {
	static constexpr size_t expected_track_bytes = 6250;

	static constexpr size_t post_index_address_mark_bytes = 26;
	static constexpr uint8_t post_index_address_mark_value = 0xff;

	static constexpr size_t pre_address_mark_bytes = 6;
	static constexpr size_t post_address_address_mark_bytes = 11;
	static constexpr uint8_t post_address_address_mark_value = 0xff;

	static constexpr size_t pre_data_mark_bytes = 6;
	static constexpr size_t post_data_bytes = 27;
	static constexpr uint8_t post_data_value = 0xff;
};
template <> struct Defaults<Density::Double> {
	static constexpr size_t expected_track_bytes = 12500;

	static constexpr size_t post_index_address_mark_bytes = 50;
	static constexpr uint8_t post_index_address_mark_value = 0x4e;

	static constexpr size_t pre_address_mark_bytes = 12;
	static constexpr size_t post_address_address_mark_bytes = 22;
	static constexpr uint8_t post_address_address_mark_value = 0x4e;

	static constexpr size_t pre_data_mark_bytes = 12;
	static constexpr size_t post_data_bytes = 54;
	static constexpr uint8_t post_data_value = 0xff;
};
template <> struct Defaults<Density::High> {
	static constexpr size_t expected_track_bytes = 25000;

	static constexpr size_t post_index_address_mark_bytes = 50;
	static constexpr uint8_t post_index_address_mark_value = 0x4e;

	static constexpr size_t pre_address_mark_bytes = 12;
	static constexpr size_t post_address_address_mark_bytes = 22;
	static constexpr uint8_t post_address_address_mark_value = 0x4e;

	static constexpr size_t pre_data_mark_bytes = 12;
	static constexpr size_t post_data_bytes = 54;
	static constexpr uint8_t post_data_value = 0xff;
};

}

enum class SurfaceItem {
	Mark,
	Data
};

class MFMEncoder: public Encoder {
public:
	MFMEncoder(std::vector<bool> &target, std::vector<bool> *fuzzy_target = nullptr) : Encoder(target, fuzzy_target) {}
	virtual ~MFMEncoder() = default;

	void add_byte(uint8_t input, uint8_t fuzzy_mask = 0) final {
		crc_generator_.add(input);
		const uint16_t spread_value = Numeric::spread_bits(input);
		const uint16_t spread_mask = Numeric::spread_bits(fuzzy_mask);
		const uint16_t or_bits = uint16_t((spread_value << 1) | (spread_value >> 1) | (last_output_ << 15));
		const uint16_t output = spread_value | ((~or_bits) & 0xaaaa);

		output_short(output, spread_mask);
	}

	void add_index_address_mark() final {
		for(int c = 0; c < 3; c++) output_short(MFMIndexSync);
		add_byte(IndexAddressByte);
	}

	void add_ID_address_mark() final {
		output_sync();
		add_byte(IDAddressByte);
	}

	void add_data_address_mark() final {
		output_sync();
		add_byte(DataAddressByte);
	}

	void add_deleted_data_address_mark() final {
		output_sync();
		add_byte(DeletedDataAddressByte);
	}

	size_t item_size(SurfaceItem item) {
		switch(item) {
			case SurfaceItem::Mark: return 8;	// Three syncs plus the mark type.
			case SurfaceItem::Data: return 2;	// Just a single encoded byte.
			default: assert(false);
		}
		return 0;	// Should be impossible to reach in debug builds.
	}

private:
	uint16_t last_output_;
	void output_short(uint16_t value, uint16_t fuzzy_mask = 0) final {
		last_output_ = value;
		Encoder::output_short(value, fuzzy_mask);
	}

	void output_sync() {
		for(int c = 0; c < 3; c++) output_short(MFMSync);
		crc_generator_.set_value(MFMPostSyncCRCValue);
	}
};

class FMEncoder: public Encoder {
// encodes each 16-bit part as clock, data, clock, data [...]
public:
	FMEncoder(std::vector<bool> &target, std::vector<bool> *fuzzy_target = nullptr) : Encoder(target, fuzzy_target) {}

	void add_byte(uint8_t input, uint8_t fuzzy_mask = 0) final {
		crc_generator_.add(input);
		output_short(
			Numeric::spread_bits(input) | 0xaaaa,
			Numeric::spread_bits(fuzzy_mask)
		);
	}

	void add_index_address_mark() final {
		crc_generator_.reset();
		crc_generator_.add(IndexAddressByte);
		output_short(FMIndexAddressMark);
	}

	void add_ID_address_mark() final {
		crc_generator_.reset();
		crc_generator_.add(IDAddressByte);
		output_short(FMIDAddressMark);
	}

	void add_data_address_mark() final {
		crc_generator_.reset();
		crc_generator_.add(DataAddressByte);
		output_short(FMDataAddressMark);
	}

	void add_deleted_data_address_mark() final {
		crc_generator_.reset();
		crc_generator_.add(DeletedDataAddressByte);
		output_short(FMDeletedDataAddressMark);
	}

	size_t item_size(SurfaceItem) {
		// Marks are just slightly-invalid bytes, so everything is the same length.
		return 2;
	}
};

template<class T> std::unique_ptr<Storage::Disk::Track>
		GTrackWithSectors(
			const std::vector<const Sector *> &sectors,
			std::size_t post_index_address_mark_bytes, uint8_t post_index_address_mark_value,
			std::size_t pre_address_mark_bytes,
			std::size_t post_address_mark_bytes, uint8_t post_address_mark_value,
			std::size_t pre_data_mark_bytes,
			std::size_t post_data_bytes, uint8_t post_data_value,
			std::size_t expected_track_bytes) {
	Storage::Disk::PCMSegment segment;
	segment.data.reserve(expected_track_bytes * 8);
	T shifter(segment.data);

	// Make a pre-estimate of output size, in case any of the idealised gaps
	// provided need to be shortened.
	const size_t data_size = shifter.item_size(SurfaceItem::Data);
	const size_t mark_size = shifter.item_size(SurfaceItem::Mark);
	const size_t max_size = (expected_track_bytes + (expected_track_bytes / 10)) * 8;

	size_t total_sector_bytes = 0;
	for(const auto sector : sectors) {
		total_sector_bytes += size_t(128 << sector->size) + 2;
	}

	// Seek appropriate gap sizes, if the defaults don't allow all data to fit.
	while(true) {
		const size_t size =
			mark_size +
			post_index_address_mark_bytes * data_size +
			total_sector_bytes * data_size +
			sectors.size() * (
				(pre_address_mark_bytes + 6 + post_address_mark_bytes + pre_data_mark_bytes + post_data_bytes) * data_size + 2 * mark_size
			);

		// If this track already fits, do nothing.
		if(size*8 < max_size) break;

		// If all gaps are already zero, do nothing.
		if(!post_index_address_mark_bytes && !pre_address_mark_bytes && !post_address_mark_bytes && !pre_data_mark_bytes && !post_data_bytes)
			break;

		// Very simple solution: try halving all gaps.
		post_index_address_mark_bytes >>= 1;
		pre_address_mark_bytes >>= 1;
		post_address_mark_bytes >>= 1;
		pre_data_mark_bytes >>= 1;
		post_data_bytes >>= 1;
	}

	// Output the index mark.
	shifter.add_index_address_mark();

	// Add the post-index mark.
	for(std::size_t c = 0; c < post_index_address_mark_bytes; c++) shifter.add_byte(post_index_address_mark_value);

	// Add sectors.
	for(const Sector *sector : sectors) {
		// Gap.
		for(std::size_t c = 0; c < pre_address_mark_bytes; c++) shifter.add_byte(0x00);

		// Sector header.
		shifter.add_ID_address_mark();
		shifter.add_byte(sector->address.track);
		shifter.add_byte(sector->address.side);
		shifter.add_byte(sector->address.sector);
		shifter.add_byte(sector->size);
		shifter.add_crc(sector->has_header_crc_error);

		// Gap.
		for(std::size_t c = 0; c < post_address_mark_bytes; c++) shifter.add_byte(post_address_mark_value);
		for(std::size_t c = 0; c < pre_data_mark_bytes; c++) shifter.add_byte(0x00);

		// Data, if attached.
		if(!sector->samples.empty()) {
			if(sector->is_deleted)
				shifter.add_deleted_data_address_mark();
			else
				shifter.add_data_address_mark();

			std::size_t c = 0;
			std::size_t declared_length = size_t(128 << sector->size);
			if(sector->samples.size() > 1) {
				// For each byte, mark as fuzzy any bits that differ. Which isn't exactly the
				// same thing as obeying the multiple samples, as it discards the implied
				// probabilities of different values.
				for(c = 0; c < sector->samples[0].size() && c < declared_length; c++) {
					auto sample_iterator = sector->samples.begin();
					uint8_t value = (*sample_iterator)[c], fuzzy_mask = 0;

					++sample_iterator;
					while(sample_iterator != sector->samples.end()) {
						// Mark as fuzzy any bits that differ here from the
						// canonical value, and zero them out in the original.
						// That might cause them to retrigger, but who cares?
						fuzzy_mask |= value ^ (*sample_iterator)[c];
						value &= ~fuzzy_mask;

						++sample_iterator;
					}
					shifter.add_byte(sector->samples[0][c], fuzzy_mask);
				}
			} else {
				for(c = 0; c < sector->samples[0].size() && c < declared_length; c++) {
					shifter.add_byte(sector->samples[0][c]);
				}
			}
			for(; c < declared_length; c++) {
				shifter.add_byte(0x00);
			}
			shifter.add_crc(sector->has_data_crc_error);
		}

		// Gap.
		for(std::size_t c = 0; c < post_data_bytes; c++) shifter.add_byte(post_data_value);
	}

	while(segment.data.size() < expected_track_bytes*8) shifter.add_byte(0x00);

	// Allow the amount of data written to be up to 10% more than the expected size. Which is generous.
	if(segment.data.size() > max_size) segment.data.resize(max_size);

	return std::make_unique<Storage::Disk::PCMTrack>(std::move(segment));
}

Encoder::Encoder(std::vector<bool> &target, std::vector<bool> *fuzzy_target) :
	target_(&target), fuzzy_target_(fuzzy_target) {}

void Encoder::reset_target(std::vector<bool> &target, std::vector<bool> *fuzzy_target) {
	target_ = &target;
	fuzzy_target_ = fuzzy_target;
}

void Encoder::output_short(uint16_t value, uint16_t fuzzy_mask) {
	const bool write_fuzzy_bits = fuzzy_mask;

	if(write_fuzzy_bits) {
		assert(fuzzy_target_);

		// Zero-fill the bits to date, to cover any shorts written without fuzzy bits,
		// and make sure the value has a 0 anywhere it should be fuzzy.
		fuzzy_target_->resize(target_->size());
		value &= ~fuzzy_mask;
	}

	uint16_t mask = 0x8000;
	while(mask) {
		target_->push_back(value & mask);
		if(write_fuzzy_bits) fuzzy_target_->push_back(fuzzy_mask & mask);
		mask >>= 1;
	}
}

void Encoder::add_crc(bool incorrectly) {
	const uint16_t crc_value = crc_generator_.get_value();
	add_byte(crc_value >> 8);
	add_byte((crc_value & 0xff) ^ (incorrectly ? 1 : 0));
}

namespace {
template <Density density>
std::unique_ptr<Storage::Disk::Track> TTrackWithSectors(
	const std::vector<const Sector *> &sectors,
	std::optional<std::size_t> sector_gap_length,
	std::optional<uint8_t> sector_gap_filler_byte
) {
	using EncoderT = std::conditional_t<density == Density::Single, FMEncoder, MFMEncoder>;
	return GTrackWithSectors<EncoderT>(
		sectors,
		Defaults<density>::post_index_address_mark_bytes,
		Defaults<density>::post_index_address_mark_value,
		Defaults<density>::pre_address_mark_bytes,
		Defaults<density>::post_address_address_mark_bytes,
		sector_gap_filler_byte ? *sector_gap_filler_byte : Defaults<density>::post_address_address_mark_value,
		Defaults<density>::pre_data_mark_bytes,
		sector_gap_length ? *sector_gap_length : Defaults<density>::post_data_bytes,
		Defaults<density>::post_data_value,
		Defaults<density>::expected_track_bytes
	);
}

}

std::unique_ptr<Storage::Disk::Track> Storage::Encodings::MFM::TrackWithSectors(
	Density density,
	const std::vector<Sector> &sectors,
	std::optional<std::size_t> sector_gap_length,
	std::optional<uint8_t> sector_gap_filler_byte
) {
	return TrackWithSectors(
		density,
		sector_pointers(sectors),
		sector_gap_length,
		sector_gap_filler_byte
	);
}

std::unique_ptr<Storage::Disk::Track> Storage::Encodings::MFM::TrackWithSectors(
	Density density,
	const std::vector<const Sector *> &sectors,
	std::optional<std::size_t> sector_gap_length,
	std::optional<uint8_t> sector_gap_filler_byte
) {
	switch(density) {
		default:
		case Density::Single:	return TTrackWithSectors<Density::Single>(sectors, sector_gap_length, sector_gap_filler_byte);
		case Density::Double:	return TTrackWithSectors<Density::Double>(sectors, sector_gap_length, sector_gap_filler_byte);
		case Density::High:		return TTrackWithSectors<Density::High>(sectors, sector_gap_length, sector_gap_filler_byte);
	}
}

std::unique_ptr<Encoder> Storage::Encodings::MFM::GetMFMEncoder(std::vector<bool> &target, std::vector<bool> *fuzzy_target) {
	return std::make_unique<MFMEncoder>(target, fuzzy_target);
}

std::unique_ptr<Encoder> Storage::Encodings::MFM::GetFMEncoder(std::vector<bool> &target, std::vector<bool> *fuzzy_target) {
	return std::make_unique<FMEncoder>(target, fuzzy_target);
}
