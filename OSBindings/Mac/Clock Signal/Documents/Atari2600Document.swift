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
	override var machine: CSMachine! {
		get {
			return atari2600
		}
	}
	override var name: String! {
		get {
			return "atari2600"
		}
	}

	// MARK: NSDocument overrides
	override class func autosavesInPlace() -> Bool {
		return true
	}

	override var windowNibName: String? {
		return "Atari2600Document"
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
		atari2600.setROM(data)
	}

	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)

		// push whatever settings the switches have in the NIB into the emulation
		pushSwitchValues()

		// show the options window but ensure the OpenGL view is key
		showOptions(self)
		self.openGLView.window?.makeKeyWindow()
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

	// MARK: Options
	@IBOutlet var resetButton: NSButton!
	@IBOutlet var selectButton: NSButton!
	@IBOutlet var colourButton: NSButton!
	@IBOutlet var leftPlayerDifficultyButton: NSButton!
	@IBOutlet var rightPlayerDifficultyButton: NSButton!

	@IBAction func optionDidChange(sender: AnyObject!) {
		pushSwitchValues()
	}

	private func pushSwitchValues() {
		atari2600.colourButton = colourButton.state == NSOnState
		atari2600.leftPlayerDifficultyButton = leftPlayerDifficultyButton.state == NSOnState
		atari2600.rightPlayerDifficultyButton = rightPlayerDifficultyButton.state == NSOnState
	}

	@IBAction func optionWasPressed(sender: NSButton!) {
		if sender == resetButton {
			atari2600.pressResetButton()
		} else {
			atari2600.pressSelectButton()
		}
	}
}