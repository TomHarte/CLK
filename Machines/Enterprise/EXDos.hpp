//
//  EXDos.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef EXDos_hpp
#define EXDos_hpp

#include "../../Components/1770/1770.hpp"
#include "../../Activity/Observer.hpp"

namespace Enterprise {

class EXDos final : public WD::WD1770 {
	public:
		EXDos();

		void set_disk(std::shared_ptr<Storage::Disk::Disk> disk, size_t drive);

		void set_control_register(uint8_t control);
		uint8_t get_control_register();

	private:
		uint8_t last_control_ = 0;

		void set_motor_on(bool on) override;
};

}

#endif /* EXDos_hpp */
