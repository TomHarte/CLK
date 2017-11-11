//
//  CPCDSK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef CPCDSK_hpp
#define CPCDSK_hpp

#include "../DiskImage.hpp"
#include "../../../FileHolder.hpp"
#include "../../Encodings/MFM/Sector.hpp"

#include <vector>

namespace Storage {
namespace Disk {

/*!
	Provies a @c Disk containing an Amstrad CPC-stype disk image — some arrangement of sectors with status bits.
*/
class CPCDSK: public DiskImage {
	public:
		/*!
			Construct an @c AcornADF containing content from the file with name @c file_name.

			@throws ErrorCantOpen if this file can't be opened.
			@throws ErrorNotAcornADF if the file doesn't appear to contain an Acorn .ADF format image.
		*/
		CPCDSK(const char *file_name);

		enum {
			ErrorNotCPCDSK,
		};

		// implemented to satisfy @c Disk
		int get_head_position_count() override;
		int get_head_count() override;
		bool get_is_read_only() override;

		void set_tracks(const std::map<Track::Address, std::shared_ptr<Track>> &tracks) override;
		std::shared_ptr<::Storage::Disk::Track> get_track_at_position(::Storage::Disk::Track::Address address) override;

	private:
		struct Track {
			uint8_t track;
			uint8_t side;
			enum class DataRate {
				Unknown, SingleOrDoubleDensity, HighDensity, ExtendedDensity
			} data_rate;
			enum class DataEncoding {
				Unknown, FM, MFM
			} data_encoding;
			uint8_t sector_length;
			uint8_t gap3_length;
			uint8_t filler_byte;

			struct Sector: public ::Storage::Encodings::MFM::Sector {
				uint8_t fdc_status1;
				uint8_t fdc_status2;
			};

			std::vector<Sector> sectors;
		};
		std::string file_name_;
		std::vector<std::unique_ptr<Track>> tracks_;
		std::size_t index_for_track(::Storage::Disk::Track::Address address);

		int head_count_;
		int head_position_count_;
		bool is_extended_;
		bool is_read_only_;
};

}
}


#endif /* CPCDSK_hpp */
