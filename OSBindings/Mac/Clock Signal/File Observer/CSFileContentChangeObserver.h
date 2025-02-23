//
//  CSFileObserver.h
//  Clock Signal
//
//  Created by Thomas Harte on 22/02/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@interface CSFileContentChangeObserver: NSObject

- (nullable instancetype)initWithURL:(nonnull NSURL *)url handler:(nonnull dispatch_block_t)handler;

@end
