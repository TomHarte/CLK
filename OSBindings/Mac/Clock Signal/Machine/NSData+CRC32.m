//
//  NSData+CRC32.m
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2019.
//  Copyright 2019 Thomas Harte. All rights reserved.
//

#import "NSData+CRC32.h"

#include <zlib.h>

@implementation NSData (StdVector)

- (NSNumber *)crc32 {
	return @(crc32(crc32(0, Z_NULL, 0), self.bytes, (uInt)self.length));
}

@end
