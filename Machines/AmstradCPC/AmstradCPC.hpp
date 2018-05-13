//
//  AmstradCPC.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 30/07/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef AmstradCPC_hpp
#define AmstradCPC_hpp

namespace AmstradCPC {

/*!
	Models an Amstrad CPC.
*/
class Machine {
	public:
		virtual ~Machine();

		/// Creates and returns an Amstrad CPC.
		static Machine *AmstradCPC();
};

}

#endif /* AmstradCPC_hpp */
