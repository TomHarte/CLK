//
//  CharacterMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 11/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef Machines_AmstradCPC_CharacterMapper_hpp
#define Machines_AmstradCPC_CharacterMapper_hpp

#include "../Typer.hpp"

namespace AmstradCPC {

class CharacterMapper: public ::Utility::CharacterMapper {
	public:
		uint16_t *sequence_for_character(char character);
};

}

#endif /* CharacterMapper_hpp */
