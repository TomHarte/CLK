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
#include "OpenGL.hpp"

#include <array>
#include <atomic>
#include <cstdint>
#include <vector>

namespace Outputs {
namespace Display {
namespace OpenGL {

class ScanTarget: public Outputs::Display::ScanTarget {
	public:
		ScanTarget();
		~ScanTarget();
		void draw();

		// Outputs::Display::ScanTarget overrides.
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

		struct PointerSet {
			// The sizes below might be less hassle as something more natural like ints,
			// but squeezing this struct into 64 bits makes the std::atomics more likely
			// to be lock free; they are under LLVM x86-64.
			int write_area = 0;
			uint16_t scan_buffer = 0;
			uint16_t composite_y = 0;
		};

		/// A pointer to the next thing that should be provided to the caller for data.
		PointerSet write_pointers_;

		/// A pointer to the final thing currently cleared for submission.
		std::atomic<PointerSet> submit_pointers_;

		/// A pointer to the first thing not yet submitted for display.
		std::atomic<PointerSet> read_pointers_;

		// Maintains a buffer of the most recent 3072 scans.
		std::array<Scan, 3072> scan_buffer_;
		GLuint scan_buffer_name_ = 0;

		// Uses a texture to vend write areas.
		std::vector<uint8_t> write_area_texture_;
		size_t data_type_size_ = 0;

		GLuint write_area_texture_name_ = 0;
		bool texture_exists_ = false;

		// Track allocation failures.
		bool allocation_has_failed_ = false;

		// Receives scan target modals.
		Modals modals_;
};

}
}
}

#endif /* ScanTarget_hpp */
