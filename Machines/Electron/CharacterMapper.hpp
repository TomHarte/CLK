//
//  CharacterMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Machines_Electron_CharacterMapper_hpp
#define Machines_Electron_CharacterMapper_hpp

#include "../Utility/Typer.hpp"

namespace Electron {

class CharacterMapper: public ::Utility::CharacterMapper {
	public:
		uint16_t *sequence_for_character(char character);
};

}

#endif /* Machines_Electron_CharacterMapper_hpp */
