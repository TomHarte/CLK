//
//  NSData+dataWithContentsOfGZippedFile.m
//  Clock SignalTests
//
//  Created by Thomas Harte on 08/08/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#import "NSData+dataWithContentsOfGZippedFile.h"

#include <zlib.h>

@implementation NSData (dataWithContentsOfGZippedFile)

+ (instancetype)dataWithContentsOfGZippedFile:(NSString *)path {
	gzFile compressedFile = gzopen([path UTF8String], "rb");
	if(!compressedFile) {
		return nil;
	}

	NSMutableData *data = [[NSMutableData alloc] init];

	uint8_t buffer[64 * 1024];
	while(true) {
		int length = gzread(compressedFile, buffer, sizeof(buffer));
		if(!length) break;
		[data appendBytes:buffer length:length];
		if(length != sizeof(buffer)) break;
	}

	gzclose(compressedFile);
	return data;
}

@end
