//
//  BD500.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 14/01/2020.
//  Copyright © 2020 Thomas Harte. All rights reserved.
//

#ifndef BD500_hpp
#define BD500_hpp

#include "../../Components/1770/1770.hpp"
#include "../../Activity/Observer.hpp"
#include "DiskController.hpp"

#include <array>
#include <memory>

namespace Oric {

class BD500: public DiskController {
	public:
		BD500();

		void write(int address, uint8_t value);
		uint8_t read(int address);

		void run_for(const Cycles cycles);

	private:
		void set_head_load_request(bool head_load) final;
		bool is_loading_head_ = false;
};

};

#endif /* BD500_hpp */
