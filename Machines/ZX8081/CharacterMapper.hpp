//
//  CharacterMapper.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef CharacterMapper_hpp
#define CharacterMapper_hpp

#include "../Typer.hpp"

namespace ZX8081 {

class CharacterMapper: public ::Utility::CharacterMapper {
	public:
		CharacterMapper(bool is_zx81);
		uint16_t *sequence_for_character(char character);

	private:
		bool is_zx81_;
};

}

#endif /* CharacterMapper_hpp */
