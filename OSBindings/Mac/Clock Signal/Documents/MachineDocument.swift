//
//  MachineDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa
import AudioToolbox

class MachineDocument: NSDocument, CSOpenGLViewDelegate, CSOpenGLViewResponderDelegate {

	@IBOutlet weak var openGLView: CSOpenGLView! {
		didSet {
			openGLView.delegate = self
			openGLView.responderDelegate = self
		}
	}

	lazy var audioQueue = AudioQueue()

	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)

		// bind the content aspect ratio to remain 4:3 from now on
		aController.window?.contentAspectRatio = NSSize(width: 4.0, height: 3.0)
	}

	var intendedCyclesPerSecond: Int64 = 0
	private var lastCycleCount: Int64?
	final func openGLView(view: CSOpenGLView, didUpdateToTime time: CVTimeStamp) {
		// TODO: treat time as a delta from old time, work out how many cycles that is plus error

		// this slightly elaborate dance is to avoid overflow
		let videoTimeScale64 = Int64(time.videoTimeScale)

		let cycleCountLow = ((time.videoTime % videoTimeScale64) * intendedCyclesPerSecond) / videoTimeScale64
		let cycleCountHigh = (time.videoTime / videoTimeScale64) * intendedCyclesPerSecond

		let cycleCount = cycleCountLow + cycleCountHigh
		if let lastCycleCount = lastCycleCount {
			let elapsedTime = cycleCount - lastCycleCount
			// if the emulation has fallen too far behind then silently limit the request;
			// some actions — e.g. the host computer waking after sleep — may give us a
			// prohibitive backlog
			runForNumberOfCycles(min(Int32(elapsedTime), Int32(intendedCyclesPerSecond / 25)))
		}
		lastCycleCount = cycleCount
	}

	func openGLView(view: CSOpenGLView, drawViewOnlyIfDirty onlyIfDirty: Bool) {}
	func runForNumberOfCycles(numberOfCycles: Int32) {}

	// MARK: CSOpenGLViewResponderDelegate
	func keyDown(event: NSEvent) {}
	func keyUp(event: NSEvent) {}
	func flagsChanged(newModifiers: NSEvent) {}
}
