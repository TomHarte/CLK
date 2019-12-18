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

#define PADHEX(n) std::hex << std::setfill('0') << std::setw(n)
#define PADDEC(n) std::dec << std::setfill('0') << std::setw(n) 

#ifdef LOG_PREFIX

#define LOG(x) std::cout << LOG_PREFIX << x << std::endl
#define LOGNBR(x) std::cout << LOG_PREFIX << x

#define ERROR(x) std::cerr << LOG_PREFIX << x << std::endl
#define ERRORNBR(x) std::cerr << LOG_PREFIX << x

#else

#define LOG(x) std::cout << x << std::endl
#define LOGNBR(x) std::cout << x

#define ERROR(x) std::cerr << x << std::endl
#define ERRORNBR(x) std::cerr << x

#endif

#endif


#endif /* Log_h */
