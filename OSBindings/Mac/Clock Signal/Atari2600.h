//
//  Atari2600.h
//  Clock Signal
//
//  Created by Thomas Harte on 14/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

@class CSAtari2600;
@protocol CSAtari2600Delegate
- (void)atari2600NeedsRedraw:(CSAtari2600 *)atari2600;
@end

@interface CSAtari2600 : NSObject

@property (nonatomic, weak) id <CSAtari2600Delegate> delegate;

- (void)runForNumberOfCycles:(int)cycles;
- (void)setROM:(NSData *)rom;

- (void)draw;

@end
