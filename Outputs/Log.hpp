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

#define LOG(x)		while(false) {}
#define LOGNBR(x)	while(false) {}

#define ERROR(x)	while(false) {}
#define ERRORNBR(x)	while(false) {}

#else

#include <iostream>
#include <ios>
#include <iomanip>

#define PADHEX(n) std::hex << std::setw(n) << std::right << std::setfill('0')
#define PADDEC(n) std::dec << std::setw(n) << std::right << std::setfill('0')

#define LOG(x) std::cout << x << std::endl
#define LOGNBR(x) std::cout << x

#define ERROR(x) std::cerr << x << std::endl
#define ERRORNBR(x) std::cerr << x

#endif


#endif /* Log_h */
