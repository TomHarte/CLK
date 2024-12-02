//
//  DSK.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/05/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "RawSectorDump.hpp"

namespace Storage::MassStorage {

/*!
	Provides a @c MassStorageDevice containing a Macintosh DSK image, which is just a
	sector dump of an entire HFS drive. It will be validated for an Apple-style partition map and communicate
	in 512-byte blocks.
*/
class DSK: public RawSectorDump<512> {
public:
	DSK(const std::string &file_name);
};

}
