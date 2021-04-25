//
//  Z80.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/04/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Z80_hpp
#define Z80_hpp

#include "../../Analyser/Static/StaticAnalyser.hpp"

namespace Storage {
namespace State {

struct Z80 {
	static std::unique_ptr<Analyser::Static::Target> load(const std::string &file_name);
};

}
}

#endif /* Z80_hpp */
