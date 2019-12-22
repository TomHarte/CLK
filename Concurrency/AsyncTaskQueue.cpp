//
//  AsyncTaskQueue.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#include "AsyncTaskQueue.hpp"

using namespace Concurrency;

AsyncTaskQueue::AsyncTaskQueue()
#ifndef __APPLE__
	: should_destruct_(false)
#endif
{
#ifdef __APPLE__
	serial_dispatch_queue_ = dispatch_queue_create("com.thomasharte.clocksignal.asyntaskqueue", DISPATCH_QUEUE_SERIAL);
#else
	thread_.reset(new std::thread([this]() {
		while(!should_destruct_) {
			std::function<void(void)> next_function;

			// Take lock, check for a new task
			std::unique_lock<std::mutex> lock(queue_mutex_);
			if(!pending_tasks_.empty()) {
				next_function = pending_tasks_.front();
				pending_tasks_.pop_front();
			}

			if(next_function) {
				// If there is a task, release lock and perform it
				lock.unlock();
				next_function();
			} else {
				// If there isn't a task, atomically block on the processing condition and release the lock
				// until there's something pending (and then release it again via scope)
				processing_condition_.wait(lock);
			}
		}
	}));
#endif
}

AsyncTaskQueue::~AsyncTaskQueue() {
#ifdef __APPLE__
	flush();
	dispatch_release(serial_dispatch_queue_);
	serial_dispatch_queue_ = nullptr;
#else
	should_destruct_ = true;
	enqueue([](){});
	thread_->join();
	thread_.reset();
#endif
}

void AsyncTaskQueue::enqueue(std::function<void(void)> function) {
#ifdef __APPLE__
	dispatch_async(serial_dispatch_queue_, ^{function();});
#else
	std::lock_guard<std::mutex> lock(queue_mutex_);
	pending_tasks_.push_back(function);
	processing_condition_.notify_all();
#endif
}

void AsyncTaskQueue::flush() {
#ifdef __APPLE__
	dispatch_sync(serial_dispatch_queue_, ^{});
#else
	auto flush_mutex = std::make_shared<std::mutex>();
	auto flush_condition = std::make_shared<std::condition_variable>();
	std::unique_lock<std::mutex> lock(*flush_mutex);
	enqueue([=] () {
		std::unique_lock<std::mutex> inner_lock(*flush_mutex);
		flush_condition->notify_all();
	});
	flush_condition->wait(lock);
#endif
}

DeferringAsyncTaskQueue::~DeferringAsyncTaskQueue() {
	perform();
	flush();
}

void DeferringAsyncTaskQueue::defer(std::function<void(void)> function) {
	if(!deferred_tasks_) {
		deferred_tasks_.reset(new std::list<std::function<void(void)>>);
	}
	deferred_tasks_->push_back(function);
}

void DeferringAsyncTaskQueue::perform() {
	if(!deferred_tasks_) return;
	std::shared_ptr<std::list<std::function<void(void)>>> deferred_tasks = deferred_tasks_;
	deferred_tasks_.reset();
	enqueue([deferred_tasks] {
		for(const auto &function : *deferred_tasks) {
			function();
		}
	});
}
