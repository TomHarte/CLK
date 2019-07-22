//
//  NSData+CRC32.m
//  Clock Signal
//
//  Created by Thomas Harte on 22/07/2019.
//  Copyright 2019 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

#include <stdint.h>

@interface NSData (CRC32)

@property(nonnull, nonatomic, readonly) NSNumber *crc32;

@end
