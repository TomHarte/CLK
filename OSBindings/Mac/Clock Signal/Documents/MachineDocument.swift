//
//  MachineDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa
import AudioToolbox

class MachineDocument: NSDocument, CSOpenGLViewDelegate, CSOpenGLViewResponderDelegate, CSBestEffortUpdaterDelegate, NSWindowDelegate {

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

	private var audioQueue: CSAudioQueue! = nil
	private lazy var bestEffortUpdater: CSBestEffortUpdater = {
		let updater = CSBestEffortUpdater()
		updater.delegate = self
		return updater
	}()

	override func windowControllerDidLoadNib(aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)

		// establish the output aspect ratio and audio
		let displayAspectRatio = self.aspectRatio()
		aController.window?.contentAspectRatio = displayAspectRatio
		openGLView.performWithGLContext({
			self.machine().setView(self.openGLView, aspectRatio: Float(displayAspectRatio.width / displayAspectRatio.height))
		})

		// establish and provide the audio queue, taking advice as to an appropriate sampling rate
		let maximumSamplingRate = CSAudioQueue.preferredSamplingRate()
		let selectedSamplingRate = self.machine().idealSamplingRateFromRange(NSRange(location: 0, length: NSInteger(maximumSamplingRate)))
		if selectedSamplingRate > 0 {
			audioQueue = CSAudioQueue(samplingRate: Float64(selectedSamplingRate))
			self.machine().audioQueue = self.audioQueue
			self.machine().setAudioSamplingRate(selectedSamplingRate)
		}

		self.bestEffortUpdater.clockRate = self.machine().clockRate
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

	final func bestEffortUpdater(bestEffortUpdater: CSBestEffortUpdater!, runForCycles cycles: UInt, didSkipPreviousUpdate: Bool) {
		runForNumberOfCycles(Int32(cycles))
	}

	// MARK: Utilities for children
	func dataForResource(name : String, ofType type: String, inDirectory directory: String) -> NSData? {
		if let path = NSBundle.mainBundle().pathForResource(name, ofType: type, inDirectory: directory) {
			return NSData(contentsOfFile: path)
		}

		return nil
	}

	// MARK: CSOpenGLViewDelegate
	func runForNumberOfCycles(numberOfCycles: Int32) {
		if actionLock.tryLock() {
			self.machine().runForNumberOfCycles(numberOfCycles)
			actionLock.unlock()
		}
	}

	func openGLView(view: CSOpenGLView, drawViewOnlyIfDirty onlyIfDirty: Bool) {
		bestEffortUpdater.update()
		if drawLock.tryLock() {
			self.machine().drawViewForPixelSize(view.backingSize, onlyIfDirty: onlyIfDirty)
			drawLock.unlock()
		}
	}

	// MARK: NSDocument overrides
	override func dataOfType(typeName: String) throws -> NSData {
		throw NSError(domain: NSOSStatusErrorDomain, code: unimpErr, userInfo: nil)
	}

	// MARK: Key forwarding
	private func withKeyboardMachine(action: (CSKeyboardMachine) -> ()) {
		if let keyboardMachine = self.machine() as? CSKeyboardMachine {
			action(keyboardMachine)
		}
	}

	func windowDidResignKey(notification: NSNotification) {
		self.withKeyboardMachine { $0.clearAllKeys() }
	}

	func keyDown(event: NSEvent) {
		self.withKeyboardMachine { $0.setKey(event.keyCode, isPressed: true) }
	}

	func keyUp(event: NSEvent) {
		self.withKeyboardMachine { $0.setKey(event.keyCode, isPressed: false) }
	}

	func flagsChanged(newModifiers: NSEvent) {
		self.withKeyboardMachine {
			$0.setKey(VK_Shift, isPressed: newModifiers.modifierFlags.contains(.ShiftKeyMask))
			$0.setKey(VK_Control, isPressed: newModifiers.modifierFlags.contains(.ControlKeyMask))
			$0.setKey(VK_Command, isPressed: newModifiers.modifierFlags.contains(.CommandKeyMask))
			$0.setKey(VK_Option, isPressed: newModifiers.modifierFlags.contains(.AlternateKeyMask))
		}
	}
}
