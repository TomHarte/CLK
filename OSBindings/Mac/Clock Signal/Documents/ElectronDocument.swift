//
//  ElectronDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation

class ElectronDocument: MachineDocument {

	private var electron = CSElectron()
	override init() {
		super.init()
		self.intendedCyclesPerSecond = 2000000

		if let osPath = NSBundle.mainBundle().pathForResource("os", ofType: "rom") {
			electron.setOSROM(NSData(contentsOfFile: osPath)!)
		}
		if let basicPath = NSBundle.mainBundle().pathForResource("basic", ofType: "rom") {
			electron.setBASICROM(NSData(contentsOfFile: basicPath)!)
		}
	}

	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)
		electron.view = openGLView
//		openGLView.frameBounds = CGRectMake(0.1, 0.1, 0.8, 0.8)
	}

	override var windowNibName: String? {
		return "ElectronDocument"
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
	}

	// MARK: CSOpenGLViewDelegate
	override func runForNumberOfCycles(numberOfCycles: Int32) {
		electron.runForNumberOfCycles(numberOfCycles)
	}

	// MARK: CSOpenGLViewResponderDelegate
//	func keyDown(event: NSEvent) {}
//	func keyUp(event: NSEvent) {}
//	func flagsChanged(newModifiers: NSEvent) {}

}
