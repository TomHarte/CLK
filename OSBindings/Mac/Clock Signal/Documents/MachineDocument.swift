//
//  MachineDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa
import AudioToolbox

class MachineDocument:
	NSDocument,
	NSWindowDelegate,
	CSMachineDelegate,
	CSOpenGLViewDelegate,
	CSOpenGLViewResponderDelegate,
	CSBestEffortUpdaterDelegate,
	CSAudioQueueDelegate
{
	fileprivate let actionLock = NSLock()
	fileprivate let drawLock = NSLock()
	fileprivate let bestEffortLock = NSLock()

	var machine: CSMachine!
	var name: String! {
		get {
			return nil
		}
	}

	func aspectRatio() -> NSSize {
		return NSSize(width: 4.0, height: 3.0)
	}

	@IBOutlet weak var openGLView: CSOpenGLView!
	@IBOutlet var optionsPanel: MachinePanel!
	@IBAction func showOptions(_ sender: AnyObject!) {
		optionsPanel?.setIsVisible(true)
	}

	fileprivate var audioQueue: CSAudioQueue! = nil
	fileprivate var bestEffortUpdater: CSBestEffortUpdater?

	override var windowNibName: NSNib.Name? {
		return NSNib.Name(rawValue: "MachineDocument")
	}

	override func windowControllerDidLoadNib(_ aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)

		// establish the output aspect ratio and audio
		let displayAspectRatio = self.aspectRatio()
		aController.window?.contentAspectRatio = displayAspectRatio
		openGLView.perform(glContext: {
			self.machine.setView(self.openGLView, aspectRatio: Float(displayAspectRatio.width / displayAspectRatio.height))
		})

		self.machine.delegate = self
		self.bestEffortUpdater = CSBestEffortUpdater()

		// callbacks from the OpenGL may come on a different thread, immediately following the .delegate set;
		// hence the full setup of the best-effort updater prior to setting self as a delegate
		self.openGLView.delegate = self
		self.openGLView.responderDelegate = self

		setupClockRate()
		self.optionsPanel?.establishStoredOptions()

		// bring OpenGL view-holding window on top of the options panel
		self.openGLView.window!.makeKeyAndOrderFront(self)

		// start accepting best effort updates
		self.bestEffortUpdater!.delegate = self
	}

	func machineDidChangeClockRate(_ machine: CSMachine!) {
		setupClockRate()
	}

	func machineDidChangeClockIsUnlimited(_ machine: CSMachine!) {
		bestEffortLock.lock()
		self.bestEffortUpdater?.runAsUnlimited = machine.clockIsUnlimited
		bestEffortLock.unlock()
	}

	fileprivate func setupClockRate() {
		// establish and provide the audio queue, taking advice as to an appropriate sampling rate
		let maximumSamplingRate = CSAudioQueue.preferredSamplingRate()
		let selectedSamplingRate = self.machine.idealSamplingRate(from: NSRange(location: 0, length: NSInteger(maximumSamplingRate)))
		if selectedSamplingRate > 0 {
			audioQueue = CSAudioQueue(samplingRate: Float64(selectedSamplingRate))
			audioQueue.delegate = self
			self.machine.audioQueue = self.audioQueue
			self.machine.setAudioSamplingRate(selectedSamplingRate, bufferSize:audioQueue.preferredBufferSize)
		}

		bestEffortLock.lock()
		self.bestEffortUpdater?.clockRate = self.machine.clockRate
		bestEffortLock.unlock()
	}

	override func close() {
		optionsPanel?.setIsVisible(false)
		optionsPanel = nil

		bestEffortLock.lock()
		bestEffortUpdater!.delegate = nil
		bestEffortUpdater!.flush()
		bestEffortUpdater = nil
		bestEffortLock.unlock()

		actionLock.lock()
		drawLock.lock()
		machine = nil
		openGLView.delegate = nil
		openGLView.invalidate()
		actionLock.unlock()
		drawLock.unlock()

		super.close()
	}

	// MARK: configuring
	func configureAs(_ analysis: CSStaticAnalyser) {
		if let machine = CSMachine(analyser: analysis) {
			self.machine = machine
		}

		if let optionsPanelNibName = analysis.optionsPanelNibName {
			Bundle.main.loadNibNamed(NSNib.Name(rawValue: optionsPanelNibName), owner: self, topLevelObjects: nil)
			self.optionsPanel.machine = self.machine
			showOptions(self)
		}
	}

	override func read(from url: URL, ofType typeName: String) throws {
		if let analyser = CSStaticAnalyser(fileAt: url) {
			self.displayName = analyser.displayName
			self.configureAs(analyser)
		} else {
			throw NSError(domain: "MachineDocument", code: -1, userInfo: nil)
		}
	}

	// MARK: the pasteboard
	func paste(_ sender: AnyObject!) {
		let pasteboard = NSPasteboard.general
		if let string = pasteboard.string(forType: .string) {
			self.machine.paste(string)
		}
	}

	// MARK: CSBestEffortUpdaterDelegate
	final func bestEffortUpdater(_ bestEffortUpdater: CSBestEffortUpdater!, runForCycles cycles: UInt, didSkipPreviousUpdate: Bool) {
		runForNumberOfCycles(Int32(cycles))
	}

	func runForNumberOfCycles(_ numberOfCycles: Int32) {
		bestEffortLock.lock()
		if let bestEffortUpdater = bestEffortUpdater {
			bestEffortLock.unlock()
			let cyclesToRunFor = min(numberOfCycles, Int32(bestEffortUpdater.clockRate / 10))
			if actionLock.try() {
				self.machine.runForNumber(ofCycles: cyclesToRunFor)
				actionLock.unlock()
			}
		} else {
			bestEffortLock.unlock()
		}
	}

	// MARK: CSAudioQueueDelegate
	final func audioQueueIsRunningDry(_ audioQueue: CSAudioQueue) {
		bestEffortLock.lock()
		bestEffortUpdater?.update()
		bestEffortLock.unlock()
	}

	// MARK: CSOpenGLViewDelegate
	final func openGLView(_ view: CSOpenGLView, drawViewOnlyIfDirty onlyIfDirty: Bool) {
		bestEffortLock.lock()
		if let bestEffortUpdater = bestEffortUpdater {
			bestEffortLock.unlock()
			bestEffortUpdater.update()
			if drawLock.try() {
				self.machine.drawView(forPixelSize: view.backingSize, onlyIfDirty: onlyIfDirty)
				drawLock.unlock()
			}
		} else {
			bestEffortLock.unlock()
		}
	}

	final func openGLView(_ view: CSOpenGLView, didReceiveFileAt URL: URL) {
		let mediaSet = CSMediaSet(fileAt: URL)
		if let mediaSet = mediaSet {
			mediaSet.apply(to: self.machine)
		}
	}

	// MARK: NSDocument overrides
	override func data(ofType typeName: String) throws -> Data {
		throw NSError(domain: NSOSStatusErrorDomain, code: unimpErr, userInfo: nil)
	}

	// MARK: Input management
	func windowDidResignKey(_ notification: Notification) {
		self.machine.clearAllKeys()
	}

	func keyDown(_ event: NSEvent) {
		self.machine.setKey(event.keyCode, characters: event.characters, isPressed: true)
	}

	func keyUp(_ event: NSEvent) {
		self.machine.setKey(event.keyCode, characters: event.characters, isPressed: false)
	}

	func flagsChanged(_ newModifiers: NSEvent) {
		self.machine.setKey(VK_Shift, characters: nil, isPressed: newModifiers.modifierFlags.contains(.shift))
		self.machine.setKey(VK_Control, characters: nil, isPressed: newModifiers.modifierFlags.contains(.control))
		self.machine.setKey(VK_Command, characters: nil, isPressed: newModifiers.modifierFlags.contains(.command))
		self.machine.setKey(VK_Option, characters: nil, isPressed: newModifiers.modifierFlags.contains(.option))
	}
}
