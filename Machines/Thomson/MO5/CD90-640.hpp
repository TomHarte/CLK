//
//  CD90-640.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/04/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include "Components/1770/1770.hpp"
#include "Outputs/Log.hpp"

#include <string>

namespace Thomson {

class CD90_640 final : public WD::WD1770 {
public:
	CD90_640();

	void set_disk(std::shared_ptr<Storage::Disk::Disk>, size_t drive);
	const Storage::Disk::Disk *disk(const std::string &);
	void set_activity_observer(Activity::Observer *);

	template <int address>
	uint8_t read() {
		const uint8_t result = [&] {
			if(address & 8) {
				return control();
			} else {
				return WD::WD1770::read(address);
			}
		} ();
		Logger::info().append("Read %02x <- %04x", result, address);
		return result;
	}

	template <int address>
	void write(const uint8_t value) {
		Logger::info().append("Write %02x -> %04x", value, address);
		if(address & 8) {
			set_control(value);
		} else {
			WD::WD1770::write(address, value);
		}
	}

private:
	uint8_t control();
	void set_control(uint8_t);
	uint8_t control_;

	using Logger = Log::Logger<Log::Source::Floppy>;
};

}
