//
//  ZX8081.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 08/06/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef Storage_Data_ZX8081_hpp
#define Storage_Data_ZX8081_hpp

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace Storage {
namespace Data {
namespace ZX8081 {

struct File {
	std::vector<uint8_t> data;
	std::wstring name;
	bool isZX81;
};

std::shared_ptr<File> FileFromData(const std::vector<uint8_t> &data);

std::wstring StringFromData(const std::vector<uint8_t> &data, bool is_zx81);
std::vector<uint8_t> DataFromString(const std::wstring &string, bool is_zx81);


}
}
}

#endif /* ZX8081_hpp */
