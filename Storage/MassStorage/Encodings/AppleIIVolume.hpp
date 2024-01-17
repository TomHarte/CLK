//
//  AppleIIVolume.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/08/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#pragma once

#include "ApplePartitionMap.hpp"

namespace Storage::MassStorage::Encodings::AppleII {

struct VolumeProvider {
	static constexpr bool HasDriver = false;

	const char *name() const {
		return "ProDOS";
	}

	const char *type() const {
		return "Apple_PRODOS";
	}
};

using Mapper = Storage::MassStorage::Encodings::Apple::PartitionMap<VolumeProvider>;

}
