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

	lazy var actionLock = NSLock()
	lazy var drawLock = NSLock()
	func machine() -> CSMachine! {
		return nil
	}

	func aspectRatio() -> NSSize {
		return NSSize(width: 4.0, height: 3.0)
	}

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

	var audioQueue : AudioQueue! = nil

	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)

		// establish the output aspect ratio and audio
		let displayAspectRatio = self.aspectRatio()
		aController.window?.contentAspectRatio = displayAspectRatio
		openGLView.performWithGLContext({
			self.machine().setView(self.openGLView, aspectRatio: Float(displayAspectRatio.width / displayAspectRatio.height))
		})

		// establish and provide the audio queue, taking advice as to an appropriate sampling rate
		let maximumSamplingRate = AudioQueue.preferredSamplingRate()
		let selectedSamplingRate = self.machine().idealSamplingRateFromRange(NSRange(location: 0, length: NSInteger(maximumSamplingRate)))
		audioQueue = AudioQueue(samplingRate: Float64(selectedSamplingRate))
		self.machine().audioQueue = self.audioQueue
		self.machine().setAudioSamplingRate(selectedSamplingRate)
	}

	override func close() {
		actionLock.lock()
		drawLock.lock()
		openGLView.invalidate()
		openGLView.openGLContext!.makeCurrentContext()
		actionLock.unlock()
		drawLock.unlock()

		super.close()
	}

	var intendedCyclesPerSecond: Int64 = 0
	private var cycleCountError: Int64 = 0
	private var lastTime: CVTimeStamp?
	private var skippedFrames = 0
	final func openGLView(view: CSOpenGLView, didUpdateToTime time: CVTimeStamp, didSkipPreviousUpdate : Bool, frequency : Double) {
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

			// run for at most three frames up to and until that causes overshoots in the
			// permitted processing window for at least four consecutive frames, in which
			// case limit to one
			numberOfCycles = min(numberOfCycles, Int64(Double(intendedCyclesPerSecond) * frequency * ((skippedFrames > 4) ? 3.0 : 1.0)))
			runForNumberOfCycles(Int32(numberOfCycles))
		}
		lastTime = time
	}

	// MARK: CSOpenGLViewDelegate
	func runForNumberOfCycles(numberOfCycles: Int32) {
		if actionLock.tryLock() {
			self.machine().runForNumberOfCycles(numberOfCycles)
			actionLock.unlock()
		}
	}

	func openGLView(view: CSOpenGLView, drawViewOnlyIfDirty onlyIfDirty: Bool) {
		if drawLock.tryLock() {
			self.machine().drawViewForPixelSize(view.backingSize, onlyIfDirty: onlyIfDirty)
			drawLock.unlock()
		}
	}

	// MARK: CSOpenGLViewResponderDelegate
	func keyDown(event: NSEvent) {}
	func keyUp(event: NSEvent) {}
	func flagsChanged(newModifiers: NSEvent) {}
}
