//
//  NSBundle+DataResource.m
//  Clock Signal
//
//  Created by Thomas Harte on 02/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#import "NSBundle+DataResource.h"

@implementation NSBundle (DataResource)

- (NSData *)dataForResource:(NSString *)resource withExtension:(NSString *)extension subdirectory:(NSString *)subdirectory {
	NSURL *url = [self URLForResource:resource withExtension:extension subdirectory:subdirectory];
	if(!url) return nil;
	return [NSData dataWithContentsOfURL:url];
}

@end
