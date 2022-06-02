//
//  AsyncTaskQueue.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#ifndef AsyncTaskQueue_hpp
#define AsyncTaskQueue_hpp

#include <atomic>
#include <condition_variable>
#include <functional>
#include <list>
#include <memory>
#include <thread>

#if defined(__APPLE__) && !defined(IGNORE_APPLE)
#include <dispatch/dispatch.h>
#define USE_GCD
#endif

namespace Concurrency {

/*!
	An async task queue allows a caller to enqueue void(void) functions. Those functions are guaranteed
	to be performed serially and asynchronously from the caller. A caller may also request to flush,
	causing it to block until all previously-enqueued functions are complete.
*/
class AsyncTaskQueue {
	public:
		AsyncTaskQueue();
		virtual ~AsyncTaskQueue();

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
#ifdef USE_GCD
		dispatch_queue_t serial_dispatch_queue_;
#else
		std::unique_ptr<std::thread> thread_;

		std::mutex queue_mutex_;
		std::list<std::function<void(void)>> pending_tasks_;
		std::condition_variable processing_condition_;
		std::atomic_bool should_destruct_;
#endif
};

/*!
	A deferring async task queue is one that accepts a list of functions to be performed but defers
	any action until told to perform. It performs them by enquing a single asynchronous task that will
	perform the deferred tasks in order.

	It therefore offers similar semantics to an asynchronous task queue, but allows for management of
	synchronisation costs, since neither defer nor perform make any effort to be thread safe.
*/
class DeferringAsyncTaskQueue: public AsyncTaskQueue {
	public:
		~DeferringAsyncTaskQueue();

		/*!
			Adds a function to the deferral list.

			This is not thread safe; it should be serialised with other calls to itself and to perform.
		*/
		void defer(std::function<void(void)> function);

		/*!
			Enqueues a function that will perform all currently deferred functions, in the
			order that they were deferred.

			This is not thread safe; it should be serialised with other calls to itself and to defer.
		*/
		void perform();

		/*!
			Blocks the caller until all previously-enqueud functions have completed.
		*/
		void flush();

	private:
		// TODO: this is a shared_ptr because of the issues capturing moveables in C++11;
		// switch to a unique_ptr if/when adapting to C++14
		std::shared_ptr<std::list<std::function<void(void)>>> deferred_tasks_;
};

}

#endif /* Concurrency_hpp */
