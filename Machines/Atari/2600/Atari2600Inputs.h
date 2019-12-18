//
//  Atari2600Inputs.h
//  Clock Signal
//
//  Created by Thomas Harte on 18/08/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

#ifndef Atari2600Inputs_h
#define Atari2600Inputs_h

#ifdef __cplusplus
extern "C" {
#endif

typedef enum  {
	Atari2600DigitalInputJoy1Up,
	Atari2600DigitalInputJoy1Down,
	Atari2600DigitalInputJoy1Left,
	Atari2600DigitalInputJoy1Right,
	Atari2600DigitalInputJoy1Fire,

	Atari2600DigitalInputJoy2Up,
	Atari2600DigitalInputJoy2Down,
	Atari2600DigitalInputJoy2Left,
	Atari2600DigitalInputJoy2Right,
	Atari2600DigitalInputJoy2Fire,
} Atari2600DigitalInput;

typedef enum  {
	Atari2600SwitchReset,
	Atari2600SwitchSelect,
	Atari2600SwitchColour,
	Atari2600SwitchLeftPlayerDifficulty,
	Atari2600SwitchRightPlayerDifficulty
} Atari2600Switch;

#ifdef __cplusplus
}
#endif

#endif /* Atari2600Inputs_h */
