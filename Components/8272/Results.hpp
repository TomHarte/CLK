//
//  Results.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 27/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#pragma once

#include "CommandDecoder.hpp"
#include "Status.hpp"

namespace Intel::i8272 {

class Results {
public:
	/// Serialises the response to Command::Invalid and Command::SenseInterruptStatus when no interrupt source was found.
	void serialise_none() {
		result_ = { 0x80 };
	}

	/// Serialises the response to Command::SenseInterruptStatus for a found drive.
	void serialise(const Status &status, const uint8_t cylinder) {
		result_ = { cylinder, status[0] };
	}

	/// Serialises the seven-byte response to Command::SenseDriveStatus.
	void serialise(const uint8_t flags, const uint8_t drive_side) {
		result_ = { uint8_t(flags | drive_side) };
	}

	/// Serialises the response to:
	///
	///	* Command::ReadData;
	///	* Command::ReadDeletedData;
	///	* Command::WriteData;
	///	* Command::WriteDeletedData;
	///	* Command::ReadID;
	///	* Command::ReadTrack;
	///	* Command::FormatTrack;
	///	* Command::ScanLow; and
	/// * Command::ScanHighOrEqual.
	void serialise(
		const Status &status,
		const uint8_t cylinder,
		const uint8_t head,
		const uint8_t sector,
		const uint8_t size
	) {
		result_ = { size, sector, head, cylinder, status[2], status[1], status[0] };
	}

	/// @returns @c true if all result bytes are exhausted; @c false otherwise.
	bool empty() const 	{	return result_.empty();	}

	/// @returns The next byte of the result.
	uint8_t next() {
		const uint8_t next = result_.back();
		result_.pop_back();
		return next;
	}

private:
	std::vector<uint8_t> result_;
};

}
