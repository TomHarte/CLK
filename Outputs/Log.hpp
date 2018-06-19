//
//  Log.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/06/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#ifndef Log_h
#define Log_h

#ifdef NDEBUG

#define LOG(x)
#define LOGNBR(x)
#define ERROR(x)
#define ERRORNBR(x)

#else

#include <iostream>

#define LOG(x) std::cout << x << std::endl
#define LOGNBR(x) std::cout << x
#define ERROR(x) std::cerr << x << std::endl
#define ERRORNBR(x) std::cerr << x

#endif


#endif /* Log_h */
