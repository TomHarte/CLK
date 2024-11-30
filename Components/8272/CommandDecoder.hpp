//
//  CommandDecoder.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include <cstddef>
#include <vector>

namespace Intel::i8272 {

enum class Command {
	ReadData = 0x06,
	ReadDeletedData = 0x0c,

	WriteData = 0x05,
	WriteDeletedData = 0x09,

	ReadTrack = 0x02,
	ReadID = 0x0a,
	FormatTrack = 0x0d,

	ScanLow = 0x11,
	ScanLowOrEqual = 0x19,
	ScanHighOrEqual = 0x1d,

	Recalibrate = 0x07,
	Seek = 0x0f,

	SenseInterruptStatus = 0x08,
	Specify = 0x03,
	SenseDriveStatus = 0x04,

	Invalid = 0x00,
};

class CommandDecoder {
public:
	/// Add a byte to the current command.
	void push_back(uint8_t byte) {
		command_.push_back(byte);
	}

	/// Reset decoding.
	void clear() {
		command_.clear();
	}

	/// @returns @c true if an entire command has been received; @c false if further bytes are needed.
	bool has_command() const {
		if(!command_.size()) {
			return false;
		}

		static constexpr std::size_t required_lengths[32] = {
			0,	0,	9,	3,	2,	9,	9,	2,
			1,	9,	2,	0,	9,	6,	0,	3,
			0,	9,	0,	0,	0,	0,	0,	0,
			0,	9,	0,	0,	0,	9,	0,	0,
		};

		return command_.size() >= required_lengths[command_[0] & 0x1f];
	}

	/// @returns The command requested. Valid only if @c has_command() is @c true.
	Command command() const {
		const auto command = Command(command_[0] & 0x1f);

		switch(command) {
			case Command::ReadData:		case Command::ReadDeletedData:
			case Command::WriteData:	case Command::WriteDeletedData:
			case Command::ReadTrack:	case Command::ReadID:
			case Command::FormatTrack:
			case Command::ScanLow:		case Command::ScanLowOrEqual:
			case Command::ScanHighOrEqual:
			case Command::Recalibrate:	case Command::Seek:
			case Command::SenseInterruptStatus:
			case Command::Specify:		case Command::SenseDriveStatus:
				return command;

			default: return Command::Invalid;
		}
	}

	//
	// Commands that specify geometry; i.e.
	//
	//	* ReadData;
	//	* ReadDeletedData;
	//	* WriteData;
	//	* WriteDeletedData;
	//	* ReadTrack;
	//	* ScanEqual;
	//	* ScanLowOrEqual;
	//	* ScanHighOrEqual.
	//

	/// @returns @c true if this command specifies geometry, in which case geomtry() is well-defined.
	/// @c false otherwise.
	bool has_geometry() const	{	return command_.size() == 9;	}
	struct Geometry {
		uint8_t cylinder, head, sector, size, end_of_track;
	};
	Geometry geometry() const {
		Geometry result;
		result.cylinder = command_[2];
		result.head = command_[3];
		result.sector = command_[4];
		result.size = command_[5];
		result.end_of_track = command_[6];
		return result;
	}

	//
	// Commands that imply data access; i.e.
	//
	//	* ReadData;
	//	* ReadDeletedData;
	//	* WriteData;
	//	* WriteDeletedData;
	//	* ReadTrack;
	//	* ReadID;
	//	* FormatTrack;
	//	* ScanLow;
	//	* ScanLowOrEqual;
	//	* ScanHighOrEqual.
	//

	/// @returns @c true if this command involves reading or writing data, in which case target() will be valid.
	/// @c false otherwise.
	bool is_access() const {
		switch(command()) {
			case Command::ReadData:		case Command::ReadDeletedData:
			case Command::WriteData:	case Command::WriteDeletedData:
			case Command::ReadTrack:	case Command::ReadID:
			case Command::FormatTrack:
			case Command::ScanLow:		case Command::ScanLowOrEqual:
			case Command::ScanHighOrEqual:
				return true;

			default:
				return false;
		}
	}
	struct AccessTarget {
		uint8_t drive, head;
		bool mfm, skip_deleted;
	};
	AccessTarget target() const {
		AccessTarget result;
		result.drive = command_[1] & 0x03;
		result.head = (command_[1] >> 2) & 0x01;
		result.mfm = command_[0] & 0x40;
		result.skip_deleted = command_[0] & 0x20;
		return result;
	}
	uint8_t drive_head() const {
		return command_[1] & 7;
	}

	//
	// Command::FormatTrack
	//

	struct FormatSpecs {
		uint8_t bytes_per_sector;
		uint8_t sectors_per_track;
		uint8_t gap3_length;
		uint8_t filler;
	};
	FormatSpecs format_specs() const {
		FormatSpecs result;
		result.bytes_per_sector = command_[2];
		result.sectors_per_track = command_[3];
		result.gap3_length = command_[4];
		result.filler = command_[5];
		return result;
	}

	//
	// Command::Seek
	//

	/// @returns The desired target track.
	uint8_t seek_target() const {
		return command_[2];
	}

	//
	// Command::Specify
	//

	struct SpecifySpecs {
		// The below are all in milliseconds.
		uint8_t step_rate_time;
		uint8_t head_unload_time;
		uint8_t head_load_time;
		bool use_dma;
	};
	SpecifySpecs specify_specs() const {
		SpecifySpecs result;
		result.step_rate_time = 16 - (command_[1] >> 4);				// i.e. 1 to 16ms
		result.head_unload_time = uint8_t((command_[1] & 0x0f) << 4);	// i.e. 16 to 240ms
		result.head_load_time = command_[2] & ~1;						// i.e. 2 to 254 ms in increments of 2ms
		result.use_dma = !(command_[2] & 1);
		return result;
	}

private:
	std::vector<uint8_t> command_;
};

}
