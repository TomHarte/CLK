//
//  Parser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Parser_hpp
#define Parser_hpp

#include "Sector.hpp"
#include "../../Track/Track.hpp"
#include "../../Drive.hpp"

namespace Storage {
namespace Encodings {
namespace MFM {

/*!
	Provides a mechanism for collecting sectors from a disk.
*/
class Parser {
	public:
		Parser(bool is_mfm, const std::shared_ptr<Storage::Disk::Disk> &disk);

		/*!
			Seeks to the physical track at @c head and @c track. Searches on it for a sector
			with logical address @c sector.

			@returns a sector if one was found; @c nullptr otherwise.
		*/
		Storage::Encodings::MFM::Sector *get_sector(int head, int track, uint8_t sector);

	private:
		std::shared_ptr<Storage::Disk::Disk> disk_;
		bool is_mfm_ = true;

		void install_sectors_from_track(const Storage::Disk::Track::Address &address);
		std::map<Storage::Disk::Track::Address, std::map<int, Storage::Encodings::MFM::Sector>> sectors_by_address_by_track_;
};

}
}
}

#endif /* Parser_hpp */
