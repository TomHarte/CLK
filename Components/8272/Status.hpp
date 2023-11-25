//
//  Status.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Status_hpp
#define Status_hpp

namespace Intel::i8272 {

class Status {
	public:
		uint8_t main() const {
			return main_status_;
		}

		void reset() {
			main_status_ = DataReady;
			status_[0] = status_[1] = status_[2] = 0;
		}

	private:
		uint8_t main_status_;
		uint8_t status_[3];

		enum MainStatus: uint8_t {
			FDD0Seeking = 0x01,
			FDD1Seeking = 0x02,
			FDD2Seeking = 0x04,
			FDD3Seeking = 0x08,

			ReadOrWriteOngoing	= 0x10,
			InNonDMAExecution	= 0x20,
			DataIsToProcessor	= 0x40,
			DataReady			= 0x80,
		};

};

}

#endif /* Status_hpp */
