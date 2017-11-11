//
//  NSData+StdVector.h
//  Clock Signal
//
//  Created by Thomas Harte on 20/09/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

#include <cstdint>
#include <vector>

@interface NSData (StdVector)

- (std::vector<std::uint8_t>)stdVector8;

@end
