//
//  6560.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef _560_hpp
#define _560_hpp

#include "../../Outputs/CRT/CRT.hpp"

namespace MOS {

class MOS6560 {
	public:
		MOS6560();
		Outputs::CRT::CRT *get_crt() { return _crt.get(); }

		uint16_t get_address();
		void set_graphics_value(uint8_t value);

		void set_register(int address, uint8_t value);

	private:
		std::unique_ptr<Outputs::CRT::CRT> _crt;

		bool _interlaced, _wide_characters;
		uint8_t _first_column_location, _first_row_location;
		uint8_t _number_of_columns, _number_of_rows;
		uint16_t _character_cell_start_address, _video_matrix_start_address;
};

}

#endif /* _560_hpp */
