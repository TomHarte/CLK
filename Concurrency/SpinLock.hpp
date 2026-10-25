//
//  SpinLock.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <atomic>

namespace Concurrency {

/*!
	A basic spin lock. Applies a release/acquire memory barrier.

	Standard warnings apply: having revealed nothing to the scheduler, a holder of this lock might sleep
	and block other eligble work.
*/
class SpinLock {
public:
	void lock() {
		while(flag_.test_and_set(std::memory_order::acquire)) {
			while(flag_.test(std::memory_order::relaxed));
		}
	}

	void unlock() {
		flag_.clear(std::memory_order::release);
	}

private:
	std::atomic_flag flag_;
};

}
