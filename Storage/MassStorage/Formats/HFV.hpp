//
//  HFV.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include "Storage/MassStorage/MassStorageDevice.hpp"
#include "Storage/FileHolder.hpp"
#include "Storage/MassStorage/Encodings/MacintoshVolume.hpp"

#include <vector>
#include <map>

namespace Storage::MassStorage {

/*!
	Provides a @c MassStorageDevice containing an HFV image, which is a sector dump of
	the HFS volume of a Macintosh drive that is not the correct size to be a floppy disk.
*/
class HFV: public MassStorageDevice, public Encodings::Macintosh::Volume {
public:
	/*!
		Constructs an HFV with the contents of the file named @c file_name.
		Raises an exception if the file name doesn't appear to identify a valid
		Macintosh mass storage image.
	*/
	HFV(const std::string &file_name);

private:
	mutable FileHolder file_;
	Encodings::Macintosh::Mapper mapper_;

	/* MassStorageDevices overrides. */
	size_t get_block_size() const final;
	size_t get_number_of_blocks() const final;
	std::vector<uint8_t> get_block(size_t) const final;
	void set_block(size_t, const std::vector<uint8_t> &) final;

	/* Encodings::Macintosh::Volume overrides. */
	void set_drive_type(Encodings::Macintosh::DriveType) final;

	std::map<size_t, std::vector<uint8_t>> writes_;
};

}
