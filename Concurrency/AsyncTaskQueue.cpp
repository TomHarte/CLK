//
//  AsyncTaskQueue.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#include "AsyncTaskQueue.hpp"

using namespace Concurrency;

AsyncTaskQueue::AsyncTaskQueue() : should_destruct_(false)
{
	thread_.reset(new std::thread([this]() {
		while(!should_destruct_)
		{
			std::function<void(void)> next_function;

			// Take lock, check for a new task
			std::unique_lock<std::mutex> lock(queue_mutex_);
			if(!pending_tasks_.empty())
			{
				next_function = pending_tasks_.front();
				pending_tasks_.pop_front();
			}

			if(next_function)
			{
				// If there is a task, release lock and perform it
				lock.unlock();
				next_function();
			}
			else
			{
				// If there isn't a task, atomically block on the processing condition and release the lock
				// until there's something pending (and then release it again via scope)
				processing_condition_.wait(lock);
			}
		}
	}));
}

AsyncTaskQueue::~AsyncTaskQueue()
{
	should_destruct_ = true;
	enqueue([](){});
	thread_->join();
	thread_.reset();
}

void AsyncTaskQueue::enqueue(std::function<void(void)> function)
{
	std::lock_guard<std::mutex> lock(queue_mutex_);
	pending_tasks_.push_back(function);
	processing_condition_.notify_all();
}

void AsyncTaskQueue::flush()
{
	std::shared_ptr<std::mutex> flush_mutex(new std::mutex);
	std::shared_ptr<std::condition_variable> flush_condition(new std::condition_variable);
	std::unique_lock<std::mutex> lock(*flush_mutex);
	enqueue([=] () {
		std::unique_lock<std::mutex> inner_lock(*flush_mutex);
		flush_condition->notify_all();
	});
	flush_condition->wait(lock);
}
