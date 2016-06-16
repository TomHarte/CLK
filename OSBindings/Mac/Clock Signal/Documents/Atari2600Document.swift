//
//  Atari2600Document.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

import Cocoa

class Atari2600Document: MachineDocument {

	private var atari2600 = CSAtari2600()
	override func machine() -> CSMachine? {
		return atari2600
	}

	// MARK: NSDocument overrides
	override init() {
		super.init()
		self.bestEffortUpdater.clockRate = 1194720
	}

	override class func autosavesInPlace() -> Bool {
		return true
	}

	override var windowNibName: String? {
		return "Atari2600Document"
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
		atari2600.setROM(data)
	}

	// MARK: CSOpenGLViewResponderDelegate

	private func inputForKey(event: NSEvent) -> Atari2600DigitalInput? {
		switch event.keyCode {
			case 123:	return Atari2600DigitalInputJoy1Left
			case 126:	return Atari2600DigitalInputJoy1Up
			case 124:	return Atari2600DigitalInputJoy1Right
			case 125:	return Atari2600DigitalInputJoy1Down
			case 0:		return Atari2600DigitalInputJoy1Fire
			default: print("\(event.keyCode)"); return nil
		}
	}

	override func keyDown(event: NSEvent) {
		super.keyDown(event)

		if let input = inputForKey(event) {
			atari2600.setState(true, forDigitalInput: input)
		}

		if event.keyCode == 36 {
			atari2600.setResetLineEnabled(true)
		}
	}

	override func keyUp(event: NSEvent) {
		super.keyUp(event)

		if let input = inputForKey(event) {
			atari2600.setState(false, forDigitalInput: input)
		}

		if event.keyCode == 36 {
			atari2600.setResetLineEnabled(false)
		}
	}
}