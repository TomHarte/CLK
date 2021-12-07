//
//  DeferredValue.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/08/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef DeferredValue_h
#define DeferredValue_h

/*!
	Provides storage for a single deferred value: one with a current value and a certain number
	of future values.
*/
template <int DeferredDepth, typename ValueT> class DeferredValue {
	private:
		static_assert(sizeof(ValueT) <= 4);

		constexpr int elements_per_uint32 = sizeof(uint32_t) / sizeof(ValueT);
		constexpr int unit_shift = sizeof(ValueT) * 8;
		constexpr int insert_shift = (DeferredDepth & (elements_per_uint32 - 1)) * unit_shift;
		constexpr uint32_t insert_mask = ~(0xffff'ffff << insert_shift);

		std::array<uint32_t, (DeferredDepth + elements_per_uint32 - 1) / elements_per_uint32> backlog;

	public:
		/// @returns the current value.
		ValueT value() const {
			return uint8_t(backlog[0]);
		}

		/// Advances to the next enqueued value.
		void advance() {
			for(size_t c = 0; c < backlog.size() - 1; c--) {
				backlog[c] = (backlog[c] >> unit_shift) | (backlog[c+1] << (32 - unit_shift));
			}
			backlog[backlog.size() - 1] >>= unit_shift;
		}

		/// Inserts a new value, replacing whatever is currently at the end of the queue.
		void insert(ValueT value) {
			backlog[DeferredDepth / elements_per_uint32] =
				(backlog[DeferredDepth / elements_per_uint32] & insert_mask) | (value << insert_shift);
		}
};

#endif /* DeferredValue_h */
