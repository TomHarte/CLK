//
//  DisplayMetrics.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef DisplayMetrics_hpp
#define DisplayMetrics_hpp

#include "ScanTarget.hpp"

#include <array>
#include <chrono>

namespace Outputs {
namespace Display {

/*!
	A class to derive various metrics about the input to a ScanTarget,
	based purely on empirical observation. In particular it is intended
	to allow for host-client frame synchronisation.
*/
class Metrics {
	public:
		/// Notifies Metrics of a beam event.
		void announce_event(ScanTarget::Event event);

		/// Notifies Metrics that the size of the output buffer has changed.
		void announce_did_resize();

		/// Provides Metrics with a new data point for output speed estimation.
		void announce_draw_status(size_t lines, std::chrono::high_resolution_clock::duration duration, bool complete);

		/// @returns @c true if Metrics thinks a lower output buffer resolution is desirable in the abstract; @c false otherwise.
		bool should_lower_resolution() const;

		/// @returns An estimate of the number of lines being produced per frame, excluding vertical sync.
		float visible_lines_per_frame_estimate() const;

		/// @returns The number of lines since vertical retrace ended.
		int current_line() const;

	private:
		int lines_this_frame_ = 0;
		std::array<int, 20> line_total_history_;
		size_t line_total_history_pointer_ = 0;
		void add_line_total(int);

		int frames_hit_ = 0;
		int frames_missed_ = 0;
};

}
}

#endif /* DisplayMetrics_hpp */
