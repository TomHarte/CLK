//
//  NSData+StdVector.m
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import "NSData+StdVector.h"

@implementation NSData (StdVector)

- (std::vector<uint8_t>)stdVector8 {
	uint8_t *bytes8 = (uint8_t *)self.bytes;
	return std::vector<uint8_t>(bytes8, bytes8 + self.length);
}

@end
