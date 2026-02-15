//
//  SpinLock.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/02/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

namespace Concurrency {

enum class Barrier {
	Relaxed,
	AcquireRelease,
};

/*!
	A basic spin lock. Applies a memory barrier as per the template.

	Standard warnings apply: having revealed nothing to the scheduler, a holder of this lock might sleep
	and block other eligble work.
*/
template <Barrier type>
class SpinLock {
public:
	void lock() {
		while(flag_.test_and_set(LockMemoryOrder));
	}

	void unlock() {
		flag_.clear(UnlockMemoryOrder);
	}

private:
	static constexpr auto LockMemoryOrder =
		type == Barrier::Relaxed ? std::memory_order_relaxed : std::memory_order_acquire;
	static constexpr auto UnlockMemoryOrder =
		type == Barrier::Relaxed ? std::memory_order_relaxed : std::memory_order_release;

	// Note to self: this is guaranteed to construct in a clear state since C++20.
	std::atomic_flag flag_;
};

}
