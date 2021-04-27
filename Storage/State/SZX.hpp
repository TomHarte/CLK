//
//  SZX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 26/04/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef Storage_State_SZX_hpp
#define Storage_State_SZX_hpp

#include "../../Analyser/Static/StaticAnalyser.hpp"

namespace Storage {
namespace State {

struct SZX {
	static std::unique_ptr<Analyser::Static::Target> load(const std::string &file_name);
};

}
}

#endif /* Storage_State_SZX_hpp */
