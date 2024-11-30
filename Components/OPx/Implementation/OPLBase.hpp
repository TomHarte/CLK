//
//  OPLBase.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#pragma once

#include "../../../Outputs/Speaker/Implementation/BufferSource.hpp"
#include "../../../Concurrency/AsyncTaskQueue.hpp"

namespace Yamaha::OPL {

template <typename Child, bool stereo> class OPLBase: public ::Outputs::Speaker::BufferSource<Child, stereo> {
public:
	void write(const uint16_t address, const uint8_t value) {
		if(address & 1) {
			static_cast<Child *>(this)->write_register(selected_register_, value);
		} else {
			selected_register_ = value;
		}
	}

protected:
	OPLBase(Concurrency::AsyncTaskQueue<false> &task_queue) : task_queue_(task_queue) {}

	Concurrency::AsyncTaskQueue<false> &task_queue_;

private:
	uint8_t selected_register_ = 0;
};

}
