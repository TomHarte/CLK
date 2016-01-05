//
//  ElectronDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 03/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation

class ElectronDocument: NSDocument, CSCathodeRayViewResponderDelegate, CSCathodeRayViewDelegate {

	override init() {
		super.init()
	}

	@IBOutlet weak var openGLView: CSCathodeRayView!
	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)

		openGLView.delegate = self
		openGLView.responderDelegate = self
//		atari2600!.view = openGLView!

		// bind the content aspect ratio to remain 4:3 from now on
		aController.window!.contentAspectRatio = NSSize(width: 4.0, height: 3.0)
	}

	override var windowNibName: String? {
		return "ElectronDocument"
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
		print("H")
	}

	override func close() {
		super.close()
		openGLView.invalidate()
	}

	// MARK: CSOpenGLViewDelegate
	func openGLView(view: CSCathodeRayView, didUpdateToTime time: CVTimeStamp) {
	}

	// MARK: CSOpenGLViewResponderDelegate
	func keyDown(event: NSEvent) {}
	func keyUp(event: NSEvent) {}
	func flagsChanged(newModifiers: NSEvent) {}

}
