//
//  AsyncTaskQueue.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef AsyncTaskQueue_hpp
#define AsyncTaskQueue_hpp

#include <memory>
#include <thread>
#include <list>
#include <condition_variable>

namespace Concurrency {

class AsyncTaskQueue {

	public:
		AsyncTaskQueue();
		~AsyncTaskQueue();

		void enqueue(std::function<void(void)> function);
		void synchronise();

	private:
		std::unique_ptr<std::thread> thread_;

		std::mutex queue_mutex_;
		std::list<std::function<void(void)>> pending_tasks_;
		std::condition_variable processing_condition_;
		bool should_destruct_;
};

}

#endif /* Concurrency_hpp */
