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

		void set_modals(Modals) override {}
		Scan *begin_scan() override { return *current_scan_; }
		uint8_t *begin_data(size_t, size_t) override { return nullptr; }

		//
		void submit() final;


		Scan current_scan_;

};

}
}
}

#endif /* SoftwareScanTarget_hpp */
