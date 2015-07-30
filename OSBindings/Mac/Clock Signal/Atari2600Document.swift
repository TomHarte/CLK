//
//  Atari2600Document.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

import Cocoa

class Atari2600Document: NSDocument, CSCathodeRayViewDelegate {

	override init() {
		super.init()
		// Add your subclass-specific initialization here.
	}

	@IBOutlet weak var openGLView: CSCathodeRayView?
	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)

		openGLView!.delegate = self
		atari2600!.view = openGLView!

		// bind the content aspect ratio to remain 4:3 from now on
		aController.window!.contentAspectRatio = NSSize(width: 4.0, height: 3.0)
	}

	override class func autosavesInPlace() -> Bool {
		return true
	}

	override var windowNibName: String? {
		// Returns the nib file name of the document
		// If you need to use a subclass of NSWindowController or if your document supports multiple NSWindowControllers, you should remove this property and override -makeWindowControllers instead.
		return "Atari2600Document"
	}

	private var atari2600: CSAtari2600? = nil
	override func dataOfType(typeName: String) throws -> NSData {
		// Insert code here to write your document to data of the specified type. If outError != nil, ensure that you create and set an appropriate error when returning nil.
		// You can also choose to override fileWrapperOfType:error:, writeToURL:ofType:error:, or writeToURL:ofType:forSaveOperation:originalContentsURL:error: instead.
		throw NSError(domain: NSOSStatusErrorDomain, code: unimpErr, userInfo: nil)
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
		atari2600 = CSAtari2600()
		atari2600!.setROM(data)
	}

	override func close() {
		super.close()
		openGLView!.invalidate()
	}

	// MARK: CSOpenGLViewDelegate

	private var lastCycleCount: Int64?
	func openGLView(view: CSCathodeRayView, didUpdateToTime time: CVTimeStamp) {

		// TODO: treat time as a delta from old time, work out how many cycles that is plus error

		// this slightly elaborate dance is to avoid overflow
		let intendedCyclesPerSecond: Int64 = 1194720
		let videoTimeScale64 = Int64(time.videoTimeScale)

		let cycleCountLow = ((time.videoTime % videoTimeScale64) * intendedCyclesPerSecond) / videoTimeScale64
		let cycleCountHigh = (time.videoTime / videoTimeScale64) * intendedCyclesPerSecond

		let cycleCount = cycleCountLow + cycleCountHigh
		if let lastCycleCount = lastCycleCount {
			let elapsedTime = cycleCount - lastCycleCount
			atari2600!.runForNumberOfCycles(Int32(elapsedTime))
		}
		lastCycleCount = cycleCount
	}

	// MARK: CSAtari2600Delegate

	func atari2600NeedsRedraw(atari2600: CSAtari2600!) {
		openGLView?.needsDisplay = true
	}
}