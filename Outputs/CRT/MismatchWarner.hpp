//
//  MismatchWarner.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 06/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

#include "CRT.hpp"

namespace Outputs::CRT {

/*!
	Provides a CRT delegate that will will observe sync mismatches and, when an appropriate threshold is crossed,
	ask its receiver to try a different display frequency.
*/
template <typename Receiver>
class CRTFrequencyMismatchWarner: public Outputs::CRT::Delegate {
public:
	CRTFrequencyMismatchWarner(Receiver &receiver) : receiver_(receiver) {}

	void crt_did_end_batch_of_frames(
		Outputs::CRT::CRT &,
		const int number_of_frames,
		const int number_of_unexpected_vertical_syncs
	) final {
		auto &record = frame_records_[frame_record_pointer_ & (NumberOfFrameRecords - 1)];
		record.number_of_frames = number_of_frames;
		record.number_of_unexpected_vertical_syncs = number_of_unexpected_vertical_syncs;
		++frame_record_pointer_;
		check_for_mismatch();
	}

	void reset() {
		frame_records_ = std::array<FrameRecord, NumberOfFrameRecords>{};
	}

private:
	Receiver &receiver_;
	struct FrameRecord {
		int number_of_frames = 0;
		int number_of_unexpected_vertical_syncs = 0;
	};

	void check_for_mismatch() {
		if(frame_record_pointer_ * 2 >= NumberOfFrameRecords * 3) {
			int total_number_of_frames = 0;
			int total_number_of_unexpected_vertical_syncs = 0;
			for(const auto &record: frame_records_) {
				total_number_of_frames += record.number_of_frames;
				total_number_of_unexpected_vertical_syncs += record.number_of_unexpected_vertical_syncs;
			}

			if(total_number_of_unexpected_vertical_syncs >= total_number_of_frames >> 1) {
				reset();
				receiver_.register_crt_frequency_mismatch();
			}
		}
	}

	static constexpr int NumberOfFrameRecords = 4;
	static_assert(!(NumberOfFrameRecords & (NumberOfFrameRecords - 1)));
	int frame_record_pointer_ = 0;
	std::array<FrameRecord, NumberOfFrameRecords> frame_records_;
};

}
