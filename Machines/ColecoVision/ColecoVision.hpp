//
//  ColecoVision.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 23/02/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef ColecoVision_hpp
#define ColecoVision_hpp

namespace Coleco {
namespace Vision {

class Machine {
	public:
		virtual ~Machine();
		static Machine *ColecoVision();
};

}
}

#endif /* ColecoVision_hpp */
