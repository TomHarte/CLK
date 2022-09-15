//
//  2MG.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/11/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#include "2MG.hpp"

#include "MacintoshIMG.hpp"
#include "../../../MassStorage/Encodings/AppleIIVolume.hpp"


#include <cstring>

using namespace Storage::Disk;

namespace {

// TODO: I've boxed myself into a corner on this stuff by not using factories more generally;
// volume to device mappers are not themselves mass storage devices because then their use
// can't currently be private to file types and relevant knowledge would need to be pushed up into
// the static analyser.
//
// So, I guess: go factory, pervasively. And probably stop the strict disk/mass storage/tape
// distinction, given that clearly some platforms just capture volumes abstractly from media.
class MassStorage2MG: public Storage::MassStorage::MassStorageDevice {
	public:
		MassStorage2MG(const std::string &file_name, long start, long size):
			file_(file_name),
			file_start_(start),
			image_size_(size)
		{
			mapper_.set_drive_type(
				Storage::MassStorage::Encodings::Apple::DriveType::SCSI,
				size_t(size / 512)
			);
		}

	private:
		Storage::FileHolder file_;
		long file_start_, image_size_;
		Storage::MassStorage::Encodings::AppleII::Mapper mapper_;

		/* MassStorageDevices overrides. */
		size_t get_block_size() final {
			return 512;
		}
		size_t get_number_of_blocks() final {
			return mapper_.get_number_of_blocks();
		}
		std::vector<uint8_t> get_block(size_t address) final {
			const auto source_address = mapper_.to_source_address(address);

			if(source_address >= 0 && long(source_address*512) < image_size_) {
				const long file_offset = file_start_ + 512 * source_address;
				file_.seek(file_offset, SEEK_SET);
				return mapper_.convert_source_block(source_address, file_.read(get_block_size()));
			} else {
				return mapper_.convert_source_block(source_address);
			}
		}
		void set_block(size_t address, const std::vector<uint8_t> &data) final {
			// TODO.
//			file_.seek(file_start_ + address * 512, SEEK_SET);
//			file_.write(data);
		}
};

}

Disk2MG::DiskOrMassStorageDevice Disk2MG::open(const std::string &file_name) {
	FileHolder file(file_name);

	// Check the signature.
	if(!file.check_signature("2IMG")) throw Error::InvalidFormat;

	// Grab the creator, potential to fix the data size momentarily.
	const auto creator = file.read(4);

	// Grab the header size, version number and image format.
	const uint16_t header_size = file.get16le();
	const uint16_t version = file.get16le();
	const uint32_t format = file.get32le();
	const uint32_t flags = file.get32le();

	// Skip the number of ProDOS blocks; this is surely implicit from the data size?
	file.seek(4, SEEK_CUR);

	// Get the offset and size of the disk image data.
	const uint32_t data_start = file.get32le();
	uint32_t data_size = file.get32le();

	// Correct for the Sweet 16 emulator, which writes broken 2MGs.
	if(!data_size && !memcmp(creator.data(), "WOOF", 4)) {
		data_size = uint32_t(file.stats().st_size - header_size);
	}

	// Skipped:
	//
	//	four bytes, offset to comment
	//	four bytes, length of comment
	//	four bytes, offset to creator-specific data
	//	four bytes, length of creator-specific data
	//
	// (all of which relate to optional appendages).

	// Validate.
	if(header_size < 0x40 || header_size >= file.stats().st_size) {
		throw Error::InvalidFormat;
	}
	if(version > 1) {
		throw Error::InvalidFormat;
	}

	// TODO: based on format, instantiate a suitable disk image.
	switch(format) {
		default: throw Error::InvalidFormat;
		case 0:
			// TODO: DOS 3.3 sector order.
		break;
		case 1:
			// 'ProDOS order', which could still mean Macintosh-style (ie. not ProDOS, but whatever)
			// or Apple II-style. Try them both.
			try {
				return new DiskImageHolder<Storage::Disk::MacintoshIMG>(file_name, MacintoshIMG::FixedType::GCR, data_start, data_size);
			} catch(...) {}

			// TODO: Apple II-style.

			// Try a hard-disk image. For now this assumes: for an Apple IIe or GS.
			return new MassStorage2MG(file_name, data_start, data_size);
		break;
		case 2:
			// TODO: NIB data (yuck!).
		break;
	}

	// So: maybe extend FileHolder to handle hiding the header before instantiating
	// a proper holder? Probably more valid than having each actual disk class do
	// its own range logic?
	(void)flags;

	throw Error::InvalidFormat;
	return nullptr;
}
