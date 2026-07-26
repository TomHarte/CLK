//
//  SmallCallable.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/07/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

#include <array>
#include <cstdint>
#include <functional>

namespace Concurrency {

static constexpr size_t SmallCallableSize = 128;	// So, ummm, not _that_ small.

/*!
	An analogue to `std::function<void(void)>` but with a small object optimisation to try to reduce
	(or, hopefully, nearly eliminate) heap allocations.
*/
class alignas(SmallCallableSize) SmallCallable {
private:
	// Keep this buffer as the first data member to ensure it has the same alignment as the class,
	// and therefore that the compatible-alignment test above is valid.
	static constexpr size_t InstanceSize = SmallCallableSize - sizeof(void *) * 2;
	std::array<uint8_t, InstanceSize> instance_;
	static_assert(InstanceSize >= sizeof(void *));

	static void null_invoke(const uint8_t *) {}
	static void null_destroy(uint8_t *) {}

    void (*invoke_)(const uint8_t *) = null_invoke;
    void (*destroy_)(uint8_t *) = null_destroy;

public:
    void operator()() const { invoke_(instance_.data()); }
    ~SmallCallable() { destroy_(instance_.data()); }

	template <typename FuncT>
	requires (
		sizeof(FuncT) <= InstanceSize &&
		alignof(FuncT) <= SmallCallableSize /*&&
		std::is_trivially_copyable<FuncT>*/
	)
    SmallCallable(FuncT &&function) {
        using BaseT = std::decay_t<FuncT>;
		new(instance_.data()) BaseT(std::forward<FuncT>(function));
		destroy_ = [](uint8_t *instance) { reinterpret_cast<BaseT *>(instance)->~BaseT(); };
		invoke_ = [](const uint8_t *instance) { (*reinterpret_cast<const BaseT *>(instance))(); };
    }

    SmallCallable(SmallCallable &&rhs) {
    	// TODO: should use the move constructor of the captured type for moving data,
    	std::copy(rhs.instance_.begin(), rhs.instance_.end(), instance_.begin());
		std::swap(invoke_, rhs.invoke_);
		std::swap(destroy_, rhs.destroy_);
    }
};

static_assert(sizeof(SmallCallable) == SmallCallableSize);

}
