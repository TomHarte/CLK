//
//  DisplayMetrics.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#include "DisplayMetrics.hpp"

using namespace Outputs::Display;

void Metrics::announce_event(ScanTarget::Event event) {
}

void Metrics::announce_did_resize() {
}

void Metrics::announce_draw_status(size_t lines, std::chrono::high_resolution_clock::duration duration, bool complete) {
}
