//
//  Parser.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#pragma once

#include "Constants.hpp"
#include "Sector.hpp"
#include "SegmentParser.hpp"
#include "Storage/Disk/Track/Track.hpp"
#include "Storage/Disk/Drive.hpp"

#include <optional>

namespace Storage::Encodings::MFM {

/*!
	Provides a mechanism for collecting sectors from a disk.
*/
class Parser {
public:
	/// Creates a parser that will only attempt to interpret the underlying disk as being of @c density.
	Parser(Density density, const std::shared_ptr<Storage::Disk::Disk> &disk);

	/// Creates a parser that will automatically try all available FM and MFM densities to try to extract sectors.
	Parser(const std::shared_ptr<Storage::Disk::Disk> &disk);

	/*!
		Seeks to the physical track at @c head and @c track. Searches on it for a sector
		with logical address @c sector.

		@returns a sector if one was found; @c nullptr otherwise.
	*/
	const Storage::Encodings::MFM::Sector *sector(int head, int track, uint8_t sector);


	/*!
		Seeks to the physical track at @c head and @c track. Searches on it for any sector.

		@returns a sector if one was found; @c nullptr otherwise.
	*/
	const Storage::Encodings::MFM::Sector *any_sector(int head, int track);

	// TODO: set_sector.

private:
	std::shared_ptr<Storage::Disk::Disk> disk_;
	std::optional<Density> density_;

	using SectorByIDMap = std::unordered_map<int, Sector>;

	void install_track(const Storage::Disk::Track::Address &address);
	static SectorMap parse_track(const Storage::Disk::Track &track, Density density);
	static void append(const SectorMap &source, SectorByIDMap &destination);

	// Maps from a track address, i.e. head and position, to a map from
	// sector IDs to sectors.
	using TrackMap =
		std::unordered_map<
			Storage::Disk::Track::Address,
			SectorByIDMap
		>;
	TrackMap sectors_by_address_by_track_;
};

}
