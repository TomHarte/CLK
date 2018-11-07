//
//  ScanTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/11/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef ScanTarget_hpp
#define ScanTarget_hpp

#include "../ScanTarget.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace Outputs {
namespace Display {
namespace OpenGL {

class ScanTarget: public Outputs::Display::ScanTarget {
	public:
		void set_modals(Modals) override;
		Scan *get_scan() override;
		uint8_t *allocate_write_area(size_t required_length, size_t required_alignment) override;
		void reduce_previous_allocation_to(size_t actual_length) override;
		void submit() override;

	private:
		// Extends the definition of a Scan to include two extra fields,
		// relevant to the way that this scan target processes video.
		struct Scan: public Outputs::Display::ScanTarget::Scan {
			/// Stores the y coordinate that this scan's data is at, within the write area texture.
			uint16_t data_y;
			/// Stores the y coordinate of this continuous composite segment within the conversion buffer.
			uint16_t composite_y;
		};

		template <typename T> struct PointerSet {
			/// A pointer to the final thing currently cleared for submission.
			T submit_pointer;
			/// A pointer to the next thing that should be provided to the caller for data.
			T write_pointer;
			/// A pointer to the first thing not yet submitted for display.
			std::atomic<T> read_pointer;
		};

		// Maintains a buffer of the most recent 3072 scans.
		std::array<Scan, 3072> scan_buffer_;
		PointerSet<size_t> scan_buffer_pointers_;

		// Uses a texture to vend write areas.
		std::vector<uint8_t> write_area_texture_;
		size_t data_type_size_ = 0;
		uint16_t write_area_x_ = 0, last_supplied_x_ = 0;
		PointerSet<uint16_t> write_area_pointers_;

		// Track allocation failures.
		bool allocation_has_failed_ = false;

		// Receives scan target modals.
		Modals modals_;
};

}
}
}

#endif /* ScanTarget_hpp */
