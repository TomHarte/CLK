//
//  NSData+dataWithContentsOfGZippedFile.h
//  Clock SignalTests
//
//  Created by Thomas Harte on 08/08/2022.
//  Copyright © 2022 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

NS_ASSUME_NONNULL_BEGIN

@interface NSData (dataWithContentsOfGZippedFile)

/// Performs an in-memory decompression of the named file, returning it as ephemeral data.
+ (instancetype)dataWithContentsOfGZippedFile:(NSString *)path;

@end

NS_ASSUME_NONNULL_END
