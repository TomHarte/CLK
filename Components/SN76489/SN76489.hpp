//
//  SN76489.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef SN76489_hpp
#define SN76489_hpp

#include "../../Outputs/Speaker/Implementation/SampleSource.hpp"
#include "../../Concurrency/AsyncTaskQueue.hpp"

namespace TI {

class SN76489: public Outputs::Speaker::SampleSource {
	public:
		/// Creates a new SN76489.
		SN76489(Concurrency::DeferringAsyncTaskQueue &task_queue);

		/// Writes a new value to the SN76489.
		void write(uint8_t value);

	private:
		Concurrency::DeferringAsyncTaskQueue &task_queue_;
};

}

#endif /* SN76489_hpp */
