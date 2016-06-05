//
//  CSKeyboardMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 05/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

@protocol CSKeyboardMachine <NSObject>

- (void)setKey:(uint16_t)key isPressed:(BOOL)isPressed;
- (void)clearAllKeys;

@end
