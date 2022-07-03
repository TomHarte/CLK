//
//  SoftwareScanTarget.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 02/07/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#ifndef SoftwareScanTarget_hpp
#define SoftwareScanTarget_hpp

#include "../ScanTarget.hpp"

#include <array>

namespace Outputs {
namespace Display {
namespace Software {

/*!
	Provides a ScanTarget that does all intermediate processing on the CPU,
	and uses the FrameProducer interface to output results.
*/
class ScanTarget: public Outputs::Display::ScanTarget {

	private:
		// The following are all overridden from Outputs::Display::ScanTarget;
		// some notes on their meaning to this specific scan target are given below.

		void set_modals(Modals) override;
		Scan *begin_scan() override { return &current_scan_; }

		uint8_t *begin_data(size_t, size_t required_alignment) override {
			// Achieve required alignment.
			sample_buffer_pointer_ += (required_alignment - sample_buffer_pointer_) & (required_alignment - 1);

			// Return target.
			return &sample_buffer_[sample_buffer_pointer_];
		}
		void end_data(size_t actual_length) override {
			sample_buffer_pointer_ += actual_length;
		}

		//
		void submit() final;

		template <InputDataType, DisplayType, ColourSpace> void process();

		// Temporaries; each scan is rasterised synchronously and upon
		// completion, so the storage here is a lot simpler than for
		// the GPU-powered scan targets.
		Scan current_scan_;
		std::array<uint8_t, 2048> sample_buffer_;
		size_t sample_buffer_pointer_ = 0;

};

}
}
}

#endif /* SoftwareScanTarget_hpp */
