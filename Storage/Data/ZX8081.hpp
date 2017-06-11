//
//  ZX8081.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/06/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Storage_Data_ZX8081_hpp
#define Storage_Data_ZX8081_hpp

#include <string>
#include <vector>
#include <cstdint>

namespace Storage {
namespace Data {
namespace ZX8081 {

struct File {
	std::vector<uint8_t> data;
	std::string name;
	bool isZX81;
};

std::shared_ptr<File> FileFromData(const std::vector<uint8_t> &data);

}
}
}

#endif /* ZX8081_hpp */
