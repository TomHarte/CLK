//
//  NSString+NSString_StringView.m
//  Clock Signal
//
//  Created by Thomas Harte on 07/10/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#import "NSString+StringView.h"


@implementation NSString (StringView)

+ (NSString *)stringWithView:(std::string_view)view {
	return [[NSString alloc] initWithBytes:view.data() length:view.size() encoding:NSUTF8StringEncoding];
}

+ (NSString *)stringWithViewNoCopy:(std::string_view)view {
	// Yuckiness here:
	//
	// NSString is Objective-C's **immutable** string. So it is guaranteed not to mutate the data passed to it
	// But Objective-C is not C++ so doesn't carry the same concept of const correctness. Hence the need for
	// a const_cast.
	//
	// i.e. the const_cast is me providing to the compiler the knowledge that reusing the data from a string_view
	// inside an NSString does not treat it as non-const.
	return [[NSString alloc]
		initWithBytesNoCopy:const_cast<char *>(view.data())
		length:view.size()
		encoding:NSUTF8StringEncoding
		freeWhenDone:NO
	];
}

@end
