//
//  Atari2600Document.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright © 2015 Thomas Harte. All rights reserved.
//

import Cocoa

class Atari2600Document: NSDocument, CSOpenGLViewDelegate {

	override init() {
	    super.init()
		// Add your subclass-specific initialization here.
	}

	@IBOutlet weak var openGLView: CSOpenGLView?
	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)
		openGLView!.delegate = self
		// Add any code here that needs to be executed once the windowController has loaded the document's window.
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
		// Insert code here to read your document from the given data of the specified type. If outError != nil, ensure that you create and set an appropriate error when returning false.
		// You can also choose to override readFromFileWrapper:ofType:error: or readFromURL:ofType:error: instead.
		// If you override either of these, you should also override -isEntireFileLoaded to return false if the contents are lazily loaded.
		atari2600 = CSAtari2600()
		atari2600?.setROM(data)
	}

	private var lastCycleCount: Int64?
	func openGLView(view: CSOpenGLView!, didUpdateToTime time: CVTimeStamp) {

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
}