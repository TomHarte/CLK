//
//  ForceInline.h
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

#ifndef ForceInline_hpp
#define ForceInline_hpp

#ifndef NDEBUG

#define forceinline inline

#else

#ifdef __GNUC__
#define forceinline __attribute__((always_inline)) inline
#elif _MSC_VER
#define forceinline __forceinline
#endif

#endif

#endif /* ForceInline_h */
