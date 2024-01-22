//
//  DAT.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 31/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "RawSectorDump.hpp"

namespace Storage::MassStorage {

/*!
	Provides a @c MassStorageDevice containing an Acorn ADFS image, which is just a
	sector dump of an ADFS volume. It will be validated for an ADFS catalogue and communicate
	in 256-byte blocks.
*/
class DAT: public RawSectorDump<256> {
	public:
		DAT(const std::string &file_name);
};

}
