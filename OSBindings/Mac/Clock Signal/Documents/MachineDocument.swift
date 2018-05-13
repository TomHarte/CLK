//
//  MachineDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
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
	var optionsPanelNibName: String?

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
		aController.window?.contentAspectRatio = self.aspectRatio()
		setupMachineOutput()

	}

	// Attempting to show a sheet before the window is visible (such as when the NIB is loaded) results in
	// a sheet mysteriously floating on its own. For now, use windowDidUpdate as a proxy to know that the window
	// is visible, though it's a little premature.
	func windowDidUpdate(_ notification: Notification) {
		if self.shouldShowNewMachinePanel {
			self.shouldShowNewMachinePanel = false
			Bundle.main.loadNibNamed(NSNib.Name(rawValue: "MachinePicker"), owner: self, topLevelObjects: nil)
			self.machinePicker?.establishStoredOptions()
			self.windowControllers[0].window?.beginSheet(self.machinePickerPanel!, completionHandler: nil)
		}
	}

	fileprivate func setupMachineOutput() {
		if let machine = self.machine, let openGLView = self.openGLView {
			// establish the output aspect ratio and audio
			let aspectRatio = self.aspectRatio()
			openGLView.perform(glContext: {
				machine.setView(openGLView, aspectRatio: Float(aspectRatio.width / aspectRatio.height))
			})

			// attach an options panel if one is available
			if let optionsPanelNibName = self.optionsPanelNibName {
				Bundle.main.loadNibNamed(NSNib.Name(rawValue: optionsPanelNibName), owner: self, topLevelObjects: nil)
				self.optionsPanel.machine = machine
				self.optionsPanel?.establishStoredOptions()
				showOptions(self)
			}

			machine.delegate = self
			self.bestEffortUpdater = CSBestEffortUpdater()

			// callbacks from the OpenGL may come on a different thread, immediately following the .delegate set;
			// hence the full setup of the best-effort updater prior to setting self as a delegate
			openGLView.delegate = self
			openGLView.responderDelegate = self

			setupAudioQueueClockRate()

			// bring OpenGL view-holding window on top of the options panel and show the content
			openGLView.isHidden = false
			openGLView.window!.makeKeyAndOrderFront(self)
			openGLView.window!.makeFirstResponder(openGLView)

			// start accepting best effort updates
			self.bestEffortUpdater!.delegate = self
		}
	}

	func machineSpeakerDidChangeInputClock(_ machine: CSMachine!) {
		setupAudioQueueClockRate()
	}

	fileprivate func setupAudioQueueClockRate() {
		// establish and provide the audio queue, taking advice as to an appropriate sampling rate
		let maximumSamplingRate = CSAudioQueue.preferredSamplingRate()
		let selectedSamplingRate = self.machine.idealSamplingRate(from: NSRange(location: 0, length: NSInteger(maximumSamplingRate)))
		if selectedSamplingRate > 0 {
			audioQueue = CSAudioQueue(samplingRate: Float64(selectedSamplingRate))
			audioQueue.delegate = self
			self.machine.audioQueue = self.audioQueue
			self.machine.setAudioSamplingRate(selectedSamplingRate, bufferSize:audioQueue.preferredBufferSize)
		}
	}

	override func close() {
		optionsPanel?.setIsVisible(false)
		optionsPanel = nil

		bestEffortLock.lock()
		if let bestEffortUpdater = bestEffortUpdater {
			bestEffortUpdater.delegate = nil
			bestEffortUpdater.flush()
			self.bestEffortUpdater = nil
		}
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
			self.optionsPanelNibName = analysis.optionsPanelNibName
			setupMachineOutput()
		}
	}

	fileprivate var shouldShowNewMachinePanel = false
	override func read(from url: URL, ofType typeName: String) throws {
		if let analyser = CSStaticAnalyser(fileAt: url) {
			self.displayName = analyser.displayName
			self.configureAs(analyser)
		} else {
			throw NSError(domain: "MachineDocument", code: -1, userInfo: nil)
		}
	}

	convenience init(type typeName: String) throws {
		self.init()
		self.fileType = typeName
		self.shouldShowNewMachinePanel = true
	}

	// MARK: the pasteboard
	func paste(_ sender: Any) {
		let pasteboard = NSPasteboard.general
		if let string = pasteboard.string(forType: .string) {
			self.machine.paste(string)
		}
	}

	// MARK: CSBestEffortUpdaterDelegate
	final func bestEffortUpdater(_ bestEffortUpdater: CSBestEffortUpdater!, runForInterval duration: TimeInterval, didSkipPreviousUpdate: Bool) {
		if actionLock.try() {
			self.machine.run(forInterval: duration)
			actionLock.unlock()
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
		if let machine = self.machine {
			machine.clearAllKeys()
		}
	}

	func keyDown(_ event: NSEvent) {
		if let machine = self.machine {
			machine.setKey(event.keyCode, characters: event.characters, isPressed: true)
		}
	}

	func keyUp(_ event: NSEvent) {
		if let machine = self.machine {
			machine.setKey(event.keyCode, characters: event.characters, isPressed: false)
		}
	}

	func flagsChanged(_ newModifiers: NSEvent) {
		if let machine = self.machine {
			machine.setKey(VK_Shift, characters: nil, isPressed: newModifiers.modifierFlags.contains(.shift))
			machine.setKey(VK_Control, characters: nil, isPressed: newModifiers.modifierFlags.contains(.control))
			machine.setKey(VK_Command, characters: nil, isPressed: newModifiers.modifierFlags.contains(.command))
			machine.setKey(VK_Option, characters: nil, isPressed: newModifiers.modifierFlags.contains(.option))
		}
	}

	// MARK: New machine creation
	@IBOutlet var machinePicker: MachinePicker?
	@IBOutlet var machinePickerPanel: NSWindow?
	@IBAction func createMachine(_ sender: NSButton?) {
		self.configureAs(machinePicker!.selectedMachine())
		machinePicker = nil
		sender?.window?.close()
	}

	@IBAction func cancelCreateMachine(_ sender: NSButton?) {
		close()
	}
}
