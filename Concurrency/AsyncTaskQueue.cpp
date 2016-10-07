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

			queue_mutex_.lock();
			if(!pending_tasks_.empty())
			{
				next_function = pending_tasks_.front();
				pending_tasks_.pop_front();
			}
			queue_mutex_.unlock();

			if(next_function)
			{
				next_function();
			}
			else
			{
				std::unique_lock<std::mutex> lock(queue_mutex_);
				processing_condition_.wait(lock);
				lock.unlock();
			}
		}
	}));
}

AsyncTaskQueue::~AsyncTaskQueue()
{
	should_destruct_ = true;
	enqueue([](){});
}

void AsyncTaskQueue::enqueue(std::function<void(void)> function)
{
	queue_mutex_.lock();
	pending_tasks_.push_back(function);
	queue_mutex_.unlock();

	std::lock_guard<std::mutex> lock(queue_mutex_);
	processing_condition_.notify_all();
}

void AsyncTaskQueue::synchronise()
{
	// TODO
//	std::mutex
}
