//
//  2MG.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "../DiskImage.hpp"
#include "../../../MassStorage/MassStorageDevice.hpp"

#include "../../../FileHolder.hpp"

#include <cstddef>
#include <variant>

namespace Storage::Disk {

/*!
	2MG is slightly special because it's just a container format; there's a brief header and then
	the contents are some other file format — either MacintoshIMG or AppleDSK.

	Therefore it supplies a factory method and will actually return one of those.

	TODO: should I generalise on factory methods? Is this likely to occur again?
*/

class Disk2MG {
	public:
		using DiskOrMassStorageDevice = std::variant<std::nullptr_t, DiskImageHolderBase *, Storage::MassStorage::MassStorageDevice *>;
		static DiskOrMassStorageDevice open(const std::string &file_name);
};

}
