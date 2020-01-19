//
//  MFM.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef Storage_Disk_Encodings_MFM_hpp
#define Storage_Disk_Encodings_MFM_hpp

#include <cstdint>
#include <memory>
#include <vector>

#include "Sector.hpp"
#include "../../Track/Track.hpp"
#include "../../../../NumberTheory/CRC.hpp"

namespace Storage {
namespace Encodings {
namespace MFM {

extern const std::size_t DefaultSectorGapLength;
/*!
	Converts a vector of sectors into a properly-encoded MFM track.

	@param sectors The sectors to write.
	@param sector_gap_length If specified, sets the distance in whole bytes between each ID and its data.
	@param sector_gap_filler_byte If specified, sets the value (unencoded) that is used to populate the gap between each ID and its data.
*/
std::shared_ptr<Storage::Disk::Track> GetMFMTrackWithSectors(const std::vector<Sector> &sectors, std::size_t sector_gap_length = DefaultSectorGapLength, uint8_t sector_gap_filler_byte = 0x4e);
std::shared_ptr<Storage::Disk::Track> GetMFMTrackWithSectors(const std::vector<const Sector *> &sectors, std::size_t sector_gap_length = DefaultSectorGapLength, uint8_t sector_gap_filler_byte = 0x4e);

/*!
	Converts a vector of sectors into a properly-encoded FM track.

	@param sectors The sectors to write.
	@param sector_gap_length If specified, sets the distance in whole bytes between each ID and its data.
	@param sector_gap_filler_byte If specified, sets the value (unencoded) that is used to populate the gap between each ID and its data.
*/
std::shared_ptr<Storage::Disk::Track> GetFMTrackWithSectors(const std::vector<Sector> &sectors, std::size_t sector_gap_length = DefaultSectorGapLength, uint8_t sector_gap_filler_byte = 0x4e);
std::shared_ptr<Storage::Disk::Track> GetFMTrackWithSectors(const std::vector<const Sector *> &sectors, std::size_t sector_gap_length = DefaultSectorGapLength, uint8_t sector_gap_filler_byte = 0x4e);

class Encoder {
	public:
		Encoder(std::vector<bool> &target);
		virtual ~Encoder() {}
		virtual void reset_target(std::vector<bool> &target);

		virtual void add_byte(uint8_t input) = 0;
		virtual void add_index_address_mark() = 0;
		virtual void add_ID_address_mark() = 0;
		virtual void add_data_address_mark() = 0;
		virtual void add_deleted_data_address_mark() = 0;
		virtual void output_short(uint16_t value);

		/// Outputs the CRC for all data since the last address mask; if @c incorrectly is @c true then outputs an incorrect CRC.
		void add_crc(bool incorrectly);

	protected:
		CRC::CCITT crc_generator_;

	private:
		std::vector<bool> *target_ = nullptr;
};

std::unique_ptr<Encoder> GetMFMEncoder(std::vector<bool> &target);
std::unique_ptr<Encoder> GetFMEncoder(std::vector<bool> &target);

}
}
}

#endif /* MFM_hpp */
