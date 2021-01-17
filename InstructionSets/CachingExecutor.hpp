//
//  CachingExecutor.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/01/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef CachingExecutor_hpp
#define CachingExecutor_hpp

namespace InstructionSet {

template <typename Executor, typename Action, typename AddressType, AddressType MaxAddress> class CachingExecutor {
	public:

	protected:
		AddressType program_counter_;

	private:
		Action actions_[100];	// TODO: just a test declaration; these actually need to be bucketed by page, etc.
};

}

#endif /* CachingExecutor_hpp */
