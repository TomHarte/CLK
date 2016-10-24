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

#ifdef __APPLE__
#include <dispatch/dispatch.h>
#endif

namespace Concurrency {

/*!
	An async task queue allows a caller to enqueue void(void) functions. Those functions are guaranteed
	to be performed serially and asynchronously from the caller. A caller may also request to synchronise,
	causing it to block until all previously-enqueued functions are complete.
*/
class AsyncTaskQueue {

	public:
		AsyncTaskQueue();
		~AsyncTaskQueue();

		/*!
			Adds @c function to the queue.

			@discussion Functions will be performed serially and asynchronously. This method is safe to
			call from multiple threads.
			@parameter function The function to enqueue.
		*/
		void enqueue(std::function<void(void)> function);

		/*!
			Blocks the caller until all previously-enqueud functions have completed.
		*/
		void flush();

	private:
#ifdef __APPLE__
		dispatch_queue_t serial_dispatch_queue_;
#else
		std::unique_ptr<std::thread> thread_;

		std::mutex queue_mutex_;
		std::list<std::function<void(void)>> pending_tasks_;
		std::condition_variable processing_condition_;
		std::atomic_bool should_destruct_;
#endif
};

}

#endif /* Concurrency_hpp */
