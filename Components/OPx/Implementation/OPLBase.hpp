//
//  OPLBase.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef OPLBase_h
#define OPLBase_h

#include "../../../Outputs/Speaker/Implementation/SampleSource.hpp"
#include "../../../Concurrency/AsyncTaskQueue.hpp"

namespace Yamaha {
namespace OPL {

template <typename Child> class OPLBase: public ::Outputs::Speaker::SampleSource {
	public:
		void write(uint16_t address, uint8_t value) {
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
}

#endif /* OPLBase_h */
