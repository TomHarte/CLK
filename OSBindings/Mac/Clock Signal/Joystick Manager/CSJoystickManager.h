//
//  CSJoystickManager.h
//  Clock Signal
//
//  Created by Thomas Harte on 19/07/2018.
//  Copyright © 2018 Thomas Harte. All rights reserved.
//

#import <Foundation/Foundation.h>

/*!
	Models a single joystick button.
	Buttons have an index and are either currently pressed, or not.
*/
@interface CSJoystickButton: NSObject
@property(nonatomic, readonly) NSInteger index;
@property(nonatomic, readonly) bool isPressed;
@end

typedef NS_ENUM(NSInteger, CSJoystickAxisType) {
	CSJoystickAxisTypeX,
	CSJoystickAxisTypeY,
	CSJoystickAxisTypeZ,
};

/*!
	Models a joystick axis.
	Axes have a nominated type and a continuous value between 0 and 1.
*/
@interface CSJoystickAxis: NSObject
@property(nonatomic, readonly) CSJoystickAxisType type;
/// The current position of this axis in the range [0, 1].
@property(nonatomic, readonly) float position;
@end

typedef NS_OPTIONS(NSInteger, CSJoystickHatDirection) {
	CSJoystickHatDirectionUp = 1 << 0,
	CSJoystickHatDirectionDown = 1 << 1,
	CSJoystickHatDirectionLeft = 1 << 2,
	CSJoystickHatDirectionRight = 1 << 3,
};

/*!
	Models a joystick hat.
	A hat is a digital directional input, so e.g. this is how thumbpads are represented.
*/
@interface CSJoystickHat: NSObject
@property(nonatomic, readonly) CSJoystickHatDirection direction;
@end

/*!
	Models a joystick.

	A joystick is a collection of buttons, axes and hats, each of which holds a current
	state. The holder must use @c update to cause this joystick to read a fresh copy
	of its state.
*/
@interface CSJoystick: NSObject
@property(nonatomic, readonly) NSArray<CSJoystickButton *> *buttons;
@property(nonatomic, readonly) NSArray<CSJoystickAxis *> *axes;
@property(nonatomic, readonly) NSArray<CSJoystickHat *> *hats;

- (void)update;
@end

/*!
	The joystick manager watches for joystick connections and disconnections and
	offers a list of joysticks currently attached.

	Be warned: this means using Apple's IOKit directly to watch for Bluetooth and
	USB HID devices. So to use this code, make sure you have USB and Bluetooth
	enabled for the app's sandbox.
*/
@interface CSJoystickManager : NSObject
@property(nonatomic, readonly) NSArray<CSJoystick *> *joysticks;

/// Updates all joysticks.
- (void)update;
@end
