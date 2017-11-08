//
//  KeyCodes.h
//  Clock Signal
//
//  Emancipated from Carbon's HIToolbox by Thomas Harte on 11/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

#ifndef KeyCodes_h
#define KeyCodes_h

/*
	Carbon somehow still manages to get into the unit test target; I can't figure out how. So I've
	renamed these contants from their origina kVK prefixes to just VK.
*/

/*
 *  Summary:
 *    Virtual keycodes
 *
 *  Discussion:
 *    These constants are the virtual keycodes defined originally in
 *    Inside Mac Volume V, pg. V-191. They identify physical keys on a
 *    keyboard. Those constants with "ANSI" in the name are labeled
 *    according to the key position on an ANSI-standard US keyboard.
 *    For example, kVK_ANSI_A indicates the virtual keycode for the key
 *    with the letter 'A' in the US keyboard layout. Other keyboard
 *    layouts may have the 'A' key label on a different physical key;
 *    in this case, pressing 'A' will generate a different virtual
 *    keycode.
 */
enum: uint16_t {
  VK_ANSI_A                    = 0x00,
  VK_ANSI_S                    = 0x01,
  VK_ANSI_D                    = 0x02,
  VK_ANSI_F                    = 0x03,
  VK_ANSI_H                    = 0x04,
  VK_ANSI_G                    = 0x05,
  VK_ANSI_Z                    = 0x06,
  VK_ANSI_X                    = 0x07,
  VK_ANSI_C                    = 0x08,
  VK_ANSI_V                    = 0x09,
  VK_ANSI_B                    = 0x0B,
  VK_ANSI_Q                    = 0x0C,
  VK_ANSI_W                    = 0x0D,
  VK_ANSI_E                    = 0x0E,
  VK_ANSI_R                    = 0x0F,
  VK_ANSI_Y                    = 0x10,
  VK_ANSI_T                    = 0x11,
  VK_ANSI_1                    = 0x12,
  VK_ANSI_2                    = 0x13,
  VK_ANSI_3                    = 0x14,
  VK_ANSI_4                    = 0x15,
  VK_ANSI_6                    = 0x16,
  VK_ANSI_5                    = 0x17,
  VK_ANSI_Equal                = 0x18,
  VK_ANSI_9                    = 0x19,
  VK_ANSI_7                    = 0x1A,
  VK_ANSI_Minus                = 0x1B,
  VK_ANSI_8                    = 0x1C,
  VK_ANSI_0                    = 0x1D,
  VK_ANSI_RightBracket         = 0x1E,
  VK_ANSI_O                    = 0x1F,
  VK_ANSI_U                    = 0x20,
  VK_ANSI_LeftBracket          = 0x21,
  VK_ANSI_I                    = 0x22,
  VK_ANSI_P                    = 0x23,
  VK_ANSI_L                    = 0x25,
  VK_ANSI_J                    = 0x26,
  VK_ANSI_Quote                = 0x27,
  VK_ANSI_K                    = 0x28,
  VK_ANSI_Semicolon            = 0x29,
  VK_ANSI_Backslash            = 0x2A,
  VK_ANSI_Comma                = 0x2B,
  VK_ANSI_Slash                = 0x2C,
  VK_ANSI_N                    = 0x2D,
  VK_ANSI_M                    = 0x2E,
  VK_ANSI_Period               = 0x2F,
  VK_ANSI_Grave                = 0x32,
  VK_ANSI_KeypadDecimal        = 0x41,
  VK_ANSI_KeypadMultiply       = 0x43,
  VK_ANSI_KeypadPlus           = 0x45,
  VK_ANSI_KeypadClear          = 0x47,
  VK_ANSI_KeypadDivide         = 0x4B,
  VK_ANSI_KeypadEnter          = 0x4C,
  VK_ANSI_KeypadMinus          = 0x4E,
  VK_ANSI_KeypadEquals         = 0x51,
  VK_ANSI_Keypad0              = 0x52,
  VK_ANSI_Keypad1              = 0x53,
  VK_ANSI_Keypad2              = 0x54,
  VK_ANSI_Keypad3              = 0x55,
  VK_ANSI_Keypad4              = 0x56,
  VK_ANSI_Keypad5              = 0x57,
  VK_ANSI_Keypad6              = 0x58,
  VK_ANSI_Keypad7              = 0x59,
  VK_ANSI_Keypad8              = 0x5B,
  VK_ANSI_Keypad9              = 0x5C
};

/* keycodes for keys that are independent of keyboard layout*/
enum: uint16_t {
  VK_Return                    = 0x24,
  VK_Tab                       = 0x30,
  VK_Space                     = 0x31,
  VK_Delete                    = 0x33,
  VK_Escape                    = 0x35,
  VK_Command                   = 0x37,
  VK_Shift                     = 0x38,
  VK_CapsLock                  = 0x39,
  VK_Option                    = 0x3A,
  VK_Control                   = 0x3B,
  VK_RightShift                = 0x3C,
  VK_RightOption               = 0x3D,
  VK_RightControl              = 0x3E,
  VK_Function                  = 0x3F,
  VK_F17                       = 0x40,
  VK_VolumeUp                  = 0x48,
  VK_VolumeDown                = 0x49,
  VK_Mute                      = 0x4A,
  VK_F18                       = 0x4F,
  VK_F19                       = 0x50,
  VK_F20                       = 0x5A,
  VK_F5                        = 0x60,
  VK_F6                        = 0x61,
  VK_F7                        = 0x62,
  VK_F3                        = 0x63,
  VK_F8                        = 0x64,
  VK_F9                        = 0x65,
  VK_F11                       = 0x67,
  VK_F13                       = 0x69,
  VK_F16                       = 0x6A,
  VK_F14                       = 0x6B,
  VK_F10                       = 0x6D,
  VK_F12                       = 0x6F,
  VK_F15                       = 0x71,
  VK_Help                      = 0x72,
  VK_Home                      = 0x73,
  VK_PageUp                    = 0x74,
  VK_ForwardDelete             = 0x75,
  VK_F4                        = 0x76,
  VK_End                       = 0x77,
  VK_F2                        = 0x78,
  VK_PageDown                  = 0x79,
  VK_F1                        = 0x7A,
  VK_LeftArrow                 = 0x7B,
  VK_RightArrow                = 0x7C,
  VK_DownArrow                 = 0x7D,
  VK_UpArrow                   = 0x7E
};

#endif /* KeyCodes_h */
