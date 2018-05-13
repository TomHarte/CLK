//
//  NSBundle+DataResource.h
//  Clock Signal
//
//  Created by Thomas Harte on 02/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface NSBundle (DataResource)

- (nullable NSData *)dataForResource:(nullable NSString *)resource withExtension:(nullable NSString *)extension subdirectory:(nullable NSString *)subdirectory;

@end
