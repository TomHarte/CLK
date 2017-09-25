//
//  SegmentParser.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/09/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#include "SegmentParser.hpp"
#include "Shifter.hpp"

using namespace Storage::Encodings::MFM;

std::map<Storage::Encodings::MFM::Sector::Address, Storage::Encodings::MFM::Sector> Storage::Encodings::MFM::SectorsFromSegment(const Storage::Disk::PCMSegment &&segment) {
	std::map<Sector::Address, Sector> result;
	Shifter shifter;

	std::unique_ptr<Storage::Encodings::MFM::Sector> new_sector;
	bool is_reading = false;
	size_t position = 0;
	size_t size = 0;

	for(unsigned int bit = 0; bit < segment.number_of_bits; ++bit) {
		shifter.add_input_bit(segment.bit(bit));
		switch(shifter.get_token()) {
			case Shifter::Token::None:
			case Shifter::Token::Sync:
			case Shifter::Token::Index:
			break;

			case Shifter::Token::ID:
				new_sector.reset(new Storage::Encodings::MFM::Sector);
				is_reading = true;
				position = 0;
			break;

			case Shifter::Token::Data:
			case Shifter::Token::DeletedData:
				if(new_sector) {
					is_reading = true;
					new_sector->is_deleted = (shifter.get_token() == Shifter::Token::DeletedData);
				}
			break;

			case Shifter::Token::Byte:
				if(is_reading) {
					switch(position) {
						case 0:	new_sector->address.track = shifter.get_byte(); ++position; break;
						case 1:	new_sector->address.side = shifter.get_byte(); ++position; break;
						case 2:	new_sector->address.sector = shifter.get_byte(); ++position; break;
						case 3:
							new_sector->size = shifter.get_byte();
							size = (size_t)(128 << new_sector->size);
							++position;
							is_reading = false;
						break;
						default:
							new_sector->data.push_back(shifter.get_byte());
							++position;
							if(position == size + 4) {
								result.insert(std::make_pair(new_sector->address, std::move(*new_sector)));
								is_reading = false;
								new_sector.reset();
							}
						break;
					}
				}
			break;
		}
	}

	return result;
}
