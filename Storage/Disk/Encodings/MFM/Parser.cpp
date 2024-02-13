//
//  Parser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#include "Parser.hpp"

#include "Constants.hpp"
#include "../../Track/TrackSerialiser.hpp"
#include "SegmentParser.hpp"

using namespace Storage::Encodings::MFM;

Parser::Parser(const std::shared_ptr<Storage::Disk::Disk> &disk) :
		disk_(disk) {}

Parser::Parser(Density density, const std::shared_ptr<Storage::Disk::Disk> &disk) :
		disk_(disk), density_(density) {}

void Parser::install_track(const Storage::Disk::Track::Address &address) {
	if(sectors_by_address_by_track_.find(address) != sectors_by_address_by_track_.end()) {
		return;
	}

	const auto track = disk_->get_track_at_position(address);
	if(!track) {
		return;
	}

	std::map<int, Storage::Encodings::MFM::Sector> sectors_by_id;
	if(density_) {
		append(parse_track(*track, *density_), sectors_by_id);
	} else {
		// Just try all three in succession.
		append(parse_track(*track, Density::Single), sectors_by_id);
		append(parse_track(*track, Density::Double), sectors_by_id);
		append(parse_track(*track, Density::High), sectors_by_id);
	}

	sectors_by_address_by_track_.emplace(address, std::move(sectors_by_id));
}

SectorMap Parser::parse_track(const Storage::Disk::Track &track, Density density) {
	return sectors_from_segment(
		Storage::Disk::track_serialisation(track, bit_length(density)),
		density);
}

void Parser::append(const SectorMap &source, std::map<int, Sector> &destination) {
	for(const auto &sector : source) {
		destination.emplace(sector.second.address.sector, std::move(sector.second));
	}
}

const Sector *Parser::sector(int head, int track, uint8_t sector) {
	const Disk::Track::Address address(head, Storage::Disk::HeadPosition(track));
	install_track(address);

	const auto sectors = sectors_by_address_by_track_.find(address);
	if(sectors == sectors_by_address_by_track_.end()) {
		return nullptr;
	}

	const auto stored_sector = sectors->second.find(sector);
	if(stored_sector == sectors->second.end()) {
		return nullptr;
	}

	return &stored_sector->second;
}
