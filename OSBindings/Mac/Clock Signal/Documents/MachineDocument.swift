//
//  MachineDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa
import AudioToolbox

class MachineDocument: NSDocument, CSOpenGLViewDelegate, CSOpenGLViewResponderDelegate, NSWindowDelegate {

	@IBOutlet weak var openGLView: CSOpenGLView! {
		didSet {
			openGLView.delegate = self
			openGLView.responderDelegate = self
		}
	}

	@IBOutlet weak var optionsPanel: NSPanel!
	@IBAction func showOptions(sender: AnyObject!) {
		optionsPanel?.setIsVisible(true)
	}

	lazy var audioQueue = AudioQueue()

	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)

		// bind the content aspect ratio to remain 4:3 from now on
		aController.window?.contentAspectRatio = NSSize(width: 4.0, height: 3.0)
	}

	var intendedCyclesPerSecond: Int64 = 0
	private var cycleCountError: Int64 = 0
	private var lastTime: CVTimeStamp?
	private var skippedFrames = 0
//	private var frameSkip = 0
	final func openGLView(view: CSOpenGLView, didUpdateToTime time: CVTimeStamp, didSkipPreviousUpdate : Bool, frequency : Double) {
//		frameSkip = frameSkip + 1
//		let modFrameSkip = frameSkip % 10
//		if modFrameSkip == 0 {
//			runForNumberOfCycles(Int32(intendedCyclesPerSecond / 60))
//		}
		if let lastTime = lastTime {
			// perform (time passed in seconds) * (intended cycles per second), converting and
			// maintaining an error count to deal with underflow
			let videoTimeScale64 = Int64(time.videoTimeScale)
			let videoTimeCount = ((time.videoTime - lastTime.videoTime) * intendedCyclesPerSecond) + cycleCountError
			cycleCountError = videoTimeCount % videoTimeScale64
			var numberOfCycles = videoTimeCount / videoTimeScale64

			// if the emulation has fallen behind then silently limit the request;
			// some actions — e.g. the host computer waking after sleep — may give us a
			// prohibitive backlog
			if didSkipPreviousUpdate {
				skippedFrames++
			} else {
				skippedFrames = 0
			}

			if skippedFrames > 4 {
				numberOfCycles = min(numberOfCycles, Int64(Double(intendedCyclesPerSecond) * frequency))
			}
			runForNumberOfCycles(Int32(numberOfCycles))
		}
		lastTime = time
	}

	func openGLView(view: CSOpenGLView, drawViewOnlyIfDirty onlyIfDirty: Bool) {}
	func runForNumberOfCycles(numberOfCycles: Int32) {}

	// MARK: CSOpenGLViewResponderDelegate
	func keyDown(event: NSEvent) {}
	func keyUp(event: NSEvent) {}
	func flagsChanged(newModifiers: NSEvent) {}
}
