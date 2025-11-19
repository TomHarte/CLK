//
//  CommodoreTAP.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/06/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "CommodoreTAP.hpp"
#include <cstdio>
#include <cstring>

using namespace Storage::Tape;

CommodoreTAP::CommodoreTAP(const std::string &file_name) : file_name_(file_name) {
	Storage::FileHolder file(file_name);

	const bool is_c64 = file.check_signature<SignatureType::String>("C64-TAPE-RAW");
	file.seek(0, Whence::SET);
	const bool is_c16 = file.check_signature<SignatureType::String>("C16-TAPE-RAW");
	if(!is_c64 && !is_c16) {
		throw ErrorNotCommodoreTAP;
	}
//	const FileType type = is_c16 ? FileType::C16 : FileType::C64;

	// Get and check the file version.
	const uint8_t version = file.get();
	if(version > 2) {
		throw ErrorNotCommodoreTAP;
	}
	updated_layout_ = version >= 1;
	half_waves_ = version >= 2;

	// Read clock rate-implying bytes.
	platform_ = Platform(file.get());
	const VideoStandard video = VideoStandard(file.get());
	file.seek(1, Whence::CUR);

	const bool double_clock = platform_ != Platform::C16 || !half_waves_;	// TODO: is the platform check correct?

	// Pick clock rate.
	initial_pulse_.length.clock_rate = static_cast<unsigned int>(
		[&] {
			switch(platform_) {
				default:
				case Platform::Vic20:	// It empirically seems like Vic-20 waves are counted with C64 timings?
				case Platform::C64:		return video == VideoStandard::PAL ? 985'248 : 1'022'727;
				case Platform::C16:		return video == VideoStandard::PAL ? 886'722 : 894'886;
			}
		}() * (double_clock ? 2 : 1)
	);
}

std::unique_ptr<FormatSerialiser> CommodoreTAP::format_serialiser() const {
	return std::make_unique<Serialiser>(file_name_, initial_pulse_, half_waves_, updated_layout_);
}

CommodoreTAP::Serialiser::Serialiser(
	const std::string &file_name,
	Pulse initial,
	bool half_waves,
	bool updated_layout) :
		file_(file_name, FileMode::Read),
		current_pulse_(initial),
		half_waves_(half_waves),
		updated_layout_(updated_layout)
{
	reset();
}

void CommodoreTAP::Serialiser::reset() {
	file_.seek(0x14, Whence::SET);
	current_pulse_.type = Pulse::High;	// Implies that the first posted wave will be ::Low.
	is_at_end_ = false;
}

bool CommodoreTAP::Serialiser::is_at_end() const {
	return is_at_end_;
}

Storage::Tape::Pulse CommodoreTAP::Serialiser::next_pulse() {
	if(is_at_end_) {
		return current_pulse_;
	}

	const auto read_next_length = [&]() -> bool {
		uint32_t next_length;
		const uint8_t next_byte = file_.get();
		if(!updated_layout_ || next_byte > 0) {
			next_length = uint32_t(next_byte) << 3;
		} else {
			next_length = file_.get_le<uint32_t, 3>();
		}

		if(file_.eof()) {
			is_at_end_ = true;
			current_pulse_.length.length = current_pulse_.length.clock_rate;
			current_pulse_.type = Pulse::Zero;
			return false;
		} else {
			current_pulse_.length.length = next_length;
			return true;
		}
	};

	if(half_waves_) {
		if(read_next_length()) {
			current_pulse_.type = current_pulse_.type == Pulse::High ? Pulse::Low : Pulse::High;
		}
	} else if(current_pulse_.type == Pulse::High) {
		if(read_next_length()) {
			current_pulse_.type = Pulse::Low;
		}
	} else {
		current_pulse_.type = Pulse::High;
	}

	return current_pulse_;
}

// MARK: - TargetPlatform::Distinguisher

TargetPlatform::Type CommodoreTAP::target_platforms() {
	switch(platform_) {
		default:				return TargetPlatform::Type::Commodore;
		case Platform::C64:		return TargetPlatform::Type::C64;
		case Platform::Vic20:	return TargetPlatform::Type::Vic20;
		case Platform::C16:		return TargetPlatform::Type::Plus4;
	}
}
