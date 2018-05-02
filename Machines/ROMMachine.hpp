//
//  ROMMachine.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 07/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef ROMMachine_hpp
#define ROMMachine_hpp

#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ROMMachine {

typedef std::function<std::vector<std::unique_ptr<std::vector<uint8_t>>>(const std::string &machine, const std::vector<std::string> &names)> ROMFetcher;

struct Machine {
	/*!
		Provides the machine with a way to obtain such ROMs as it needs.
	*/
	virtual bool set_rom_fetcher(const ROMFetcher &rom_with_name) { return true; }
};

}

#endif /* ROMMachine_h */
