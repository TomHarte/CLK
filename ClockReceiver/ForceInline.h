//
//  ForceInline.h
//  Clock Signal
//
//  Created by Thomas Harte on 01/08/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

#ifndef ForceInline_h
#define ForceInline_h

#ifdef __GNUC__
#define forceinline __attribute__((always_inline)) inline
#elif _MSC_VER
#define forceinline __forceinline
#endif

#endif /* ForceInline_h */
