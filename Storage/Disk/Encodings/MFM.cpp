//
//  MFM.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "MFM.hpp"

#import "../PCMTrack.hpp"

using namespace Storage::Encodings::MFM;

std::shared_ptr<Storage::Disk::Track> Storage::Encodings::MFM::GetMFMTrackWithSectors(const std::vector<Sector> &sectors)
{
	class MFMVectorShifter: public MFMShifter<MFMVectorShifter> {
		void output_short(uint16_t value) {
			data.push_back(value & 0xff);
			data.push_back(value >> 8);
		}

		std::vector<uint8_t> data;
	} shifter;

	// output the index mark
	shifter.add_index_address_mark();

	// add the post-index mark
	for(int c = 0; c < 50; c++) shifter.add_byte(0x4e);

	// add sectors
	for(const Sector &sector : sectors)
	{
		for(int c = 0; c < 12; c++) shifter.add_byte(0x00);

		shifter.add_ID_address_mark();
		shifter.add_byte(sector.track);
		shifter.add_byte(sector.side);
		shifter.add_byte(sector.sector);
		switch(sector.data.size())
		{
			default:	shifter.add_byte(0);	break;
			case 256:	shifter.add_byte(1);	break;
			case 512:	shifter.add_byte(2);	break;
			case 1024:	shifter.add_byte(3);	break;
			case 2048:	shifter.add_byte(4);	break;
			case 4196:	shifter.add_byte(5);	break;
		}
		// TODO: CRC of bytes since the track number

		for(int c = 0; c < 22; c++) shifter.add_byte(0x4e);
		for(int c = 0; c < 12; c++) shifter.add_byte(0x00);

		shifter.add_data_address_mark();
		for(size_t c = 0; c < sector.data.size(); c++) shifter.add_byte(sector.data[c]);
		// TODO: CRC of data

		for(int c = 0; c < 18; c++) shifter.add_byte(0x00);
		for(int c = 0; c < 32; c++) shifter.add_byte(0x4e);
	}

	// TODO: total size check

	Storage::Disk::PCMSegment segment;
	return std::shared_ptr<Storage::Disk::Track>(new Storage::Disk::PCMTrack(std::move(segment)));
}
