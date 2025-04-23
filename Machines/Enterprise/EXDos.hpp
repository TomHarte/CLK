//
//  EXDos.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#pragma once

#include "Components/1770/1770.hpp"
#include "Activity/Observer.hpp"

namespace Enterprise {

class EXDos final : public WD::WD1770 {
public:
	EXDos();

	void set_disk(std::shared_ptr<Storage::Disk::Disk>, const size_t drive);

	void set_control_register(uint8_t);
	uint8_t get_control_register();

	void set_activity_observer(Activity::Observer *);

private:
	bool disk_did_change_ = false;
	void set_motor_on(bool) override;
};

}
