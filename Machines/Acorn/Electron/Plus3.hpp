//
//  Plus3.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "Components/1770/1770.hpp"
#include "Activity/Observer.hpp"

#include <string_view>

namespace Electron {

class Plus3 final : public WD::WD1770 {
public:
	Plus3();

	void set_disk(std::shared_ptr<Storage::Disk::Disk>, size_t drive);
	const Storage::Disk::Disk *disk(std::string_view);
	void set_control_register(uint8_t control);
	void set_activity_observer(Activity::Observer *);

private:
	void set_control_register(uint8_t control, uint8_t changes);
	uint8_t last_control_ = 0;

	void set_motor_on(bool) override;
	std::string drive_name(size_t);
};

}
