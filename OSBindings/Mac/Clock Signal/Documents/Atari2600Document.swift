//
//  Atari2600Document.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

import Cocoa

class Atari2600Document: MachineDocument {

	// MARK: NSDocument overrides
	override init() {
		super.init()
		self.intendedCyclesPerSecond = 1194720
	}

	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)
		atari2600.view = openGLView
	}

	override class func autosavesInPlace() -> Bool {
		return true
	}

	override var windowNibName: String? {
		// Returns the nib file name of the document
		// If you need to use a subclass of NSWindowController or if your document supports multiple NSWindowControllers, you should remove this property and override -makeWindowControllers instead.
		return "Atari2600Document"
	}

	private var atari2600: CSAtari2600! = nil
	override func dataOfType(typeName: String) throws -> NSData {
		// Insert code here to write your document to data of the specified type. If outError != nil, ensure that you create and set an appropriate error when returning nil.
		// You can also choose to override fileWrapperOfType:error:, writeToURL:ofType:error:, or writeToURL:ofType:forSaveOperation:originalContentsURL:error: instead.
		throw NSError(domain: NSOSStatusErrorDomain, code: unimpErr, userInfo: nil)
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
		atari2600 = CSAtari2600()
		atari2600.setROM(data)
	}

	override func close() {
		super.close()
		openGLView.invalidate()
	}

	// MARK: MachineDocument overrides

	override func runForNumberOfCycles(numberOfCycles: Int32) {
		atari2600.runForNumberOfCycles(numberOfCycles)
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