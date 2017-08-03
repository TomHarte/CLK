//
//  CharacterMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Machines_Commodore_Vic20_CharacterMapper_hpp
#define Machines_Commodore_Vic20_CharacterMapper_hpp

#include "../../Typer.hpp"

namespace Commodore {
namespace Vic20 {

class CharacterMapper: public ::Utility::CharacterMapper {
	public:
		uint16_t *sequence_for_character(char character);
};

}
}

#endif /* CharacterMapper_hpp */
