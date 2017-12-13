//
//  MSX.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 24/11/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef MSX_hpp
#define MSX_hpp

namespace MSX {

class Machine {
	public:
		virtual ~Machine();
		static Machine *MSX();
};

}

#endif /* MSX_hpp */
