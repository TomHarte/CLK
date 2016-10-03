//
//  CSJoystickMachine.h
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

typedef NS_ENUM(NSInteger, CSJoystickDirection)
{
	CSJoystickDirectionUp,
	CSJoystickDirectionDown,
	CSJoystickDirectionLeft,
	CSJoystickDirectionRight
};

@protocol CSJoystickMachine <NSObject>

- (void)setButtonAtIndex:(NSUInteger)button onPad:(NSUInteger)pad isPressed:(BOOL)isPressed;
- (void)setDirection:(CSJoystickDirection)direction onPad:(NSUInteger)pad isPressed:(BOOL)isPressed;

@end
