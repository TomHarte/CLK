//
//  Atari2600Document.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

import Cocoa

class Atari2600Document: MachineDocument {

	// MARK: CSOpenGLViewResponderDelegate
/*	fileprivate func inputForKey(_ event: NSEvent) -> Atari2600DigitalInput? {
		switch event.keyCode {
			case 123:	return Atari2600DigitalInputJoy1Left
			case 126:	return Atari2600DigitalInputJoy1Up
			case 124:	return Atari2600DigitalInputJoy1Right
			case 125:	return Atari2600DigitalInputJoy1Down
			case 0:		return Atari2600DigitalInputJoy1Fire
			default:
				Swift.print("\(event.keyCode)")
				return nil
		}
	}

	override func keyDown(_ event: NSEvent) {
		super.keyDown(event)

		if let input = inputForKey(event) {
			atari2600.setState(true, for: input)
		}

		if event.keyCode == 36 {
			atari2600.setResetLineEnabled(true)
		}
	}

	override func keyUp(_ event: NSEvent) {
		super.keyUp(event)

		if let input = inputForKey(event) {
			atari2600.setState(false, for: input)
		}

		if event.keyCode == 36 {
			atari2600.setResetLineEnabled(false)
		}
	}*/
}
