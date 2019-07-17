//
//  MachineDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

import AudioToolbox
import Cocoa

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

	@IBOutlet var activityPanel: NSPanel!
	@IBAction func showActivity(_ sender: AnyObject!) {
		activityPanel.setIsVisible(true)
	}

	fileprivate var audioQueue: CSAudioQueue! = nil
	fileprivate var bestEffortUpdater: CSBestEffortUpdater?

	override var windowNibName: NSNib.Name? {
		return "MachineDocument"
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
			Bundle.main.loadNibNamed("MachinePicker", owner: self, topLevelObjects: nil)
			self.machinePicker?.establishStoredOptions()
			self.windowControllers[0].window?.beginSheet(self.machinePickerPanel!, completionHandler: nil)
		}
	}

	fileprivate func setupMachineOutput() {
		if let machine = self.machine, let openGLView = self.openGLView {
			// Establish the output aspect ratio and audio.
			let aspectRatio = self.aspectRatio()
			openGLView.perform(glContext: {
				machine.setView(openGLView, aspectRatio: Float(aspectRatio.width / aspectRatio.height))
			})

			// Attach an options panel if one is available.
			if let optionsPanelNibName = self.optionsPanelNibName {
				Bundle.main.loadNibNamed(optionsPanelNibName, owner: self, topLevelObjects: nil)
				self.optionsPanel.machine = machine
				self.optionsPanel?.establishStoredOptions()
				showOptions(self)
			}

			machine.delegate = self
			self.bestEffortUpdater = CSBestEffortUpdater()

			// Callbacks from the OpenGL may come on a different thread, immediately following the .delegate set;
			// hence the full setup of the best-effort updater prior to setting self as a delegate.
			openGLView.delegate = self
			openGLView.responderDelegate = self

			// If this machine has a mouse, enable mouse capture.
			openGLView.shouldCaptureMouse = machine.hasMouse

			setupAudioQueueClockRate()

			// Bring OpenGL view-holding window on top of the options panel and show the content.
			openGLView.isHidden = false
			openGLView.window!.makeKeyAndOrderFront(self)
			openGLView.window!.makeFirstResponder(openGLView)

			// Start accepting best effort updates.
			self.bestEffortUpdater!.delegate = self
		}
	}

	func machineSpeakerDidChangeInputClock(_ machine: CSMachine) {
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
		activityPanel?.setIsVisible(false)
		activityPanel = nil

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
			setupActivityDisplay()
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
	final func openGLViewRedraw(_ view: CSOpenGLView, event redrawEvent: CSOpenGLViewRedrawEvent) {
		if redrawEvent == .timer {
			bestEffortLock.lock()
			if let bestEffortUpdater = bestEffortUpdater {
				bestEffortLock.unlock()
				bestEffortUpdater.update()
			} else {
				bestEffortLock.unlock()
			}
		}

		if drawLock.try() {
			if redrawEvent == .timer {
				machine.updateView(forPixelSize: view.backingSize)
			}
			machine.drawView(forPixelSize: view.backingSize)
			drawLock.unlock()
		}
	}

	// MARK: Runtime media insertion.
	final func openGLView(_ view: CSOpenGLView, didReceiveFileAt URL: URL) {
		let mediaSet = CSMediaSet(fileAt: URL)
		if let mediaSet = mediaSet {
			mediaSet.apply(to: self.machine)
		}
	}

	@IBAction final func insertMedia(_ sender: AnyObject!) {
		let openPanel = NSOpenPanel()
		openPanel.message = "Hint: you can also insert media by dragging and dropping it onto the machine's window."
		openPanel.beginSheetModal(for: self.windowControllers[0].window!) { (response) in
			if response == .OK {
				for url in openPanel.urls {
					let mediaSet = CSMediaSet(fileAt: url)
					if let mediaSet = mediaSet {
						mediaSet.apply(to: self.machine)
					}
				}
			}
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
			machine.joystickManager = nil
		}
		self.openGLView.releaseMouse()
	}

	func windowDidBecomeKey(_ notification: Notification) {
		if let machine = self.machine {
			machine.joystickManager = (DocumentController.shared as! DocumentController).joystickManager
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

	func mouseMoved(_ event: NSEvent) {
		if let machine = self.machine {
			machine.addMouseMotionX(event.deltaX, y: event.deltaY)
		}
	}

	func mouseUp(_ event: NSEvent) {
		if let machine = self.machine {
			machine.setMouseButton(Int32(event.buttonNumber), isPressed: false)
		}
	}

	func mouseDown(_ event: NSEvent) {
		if let machine = self.machine {
			machine.setMouseButton(Int32(event.buttonNumber), isPressed: true)
		}
	}

	// MARK: New machine creation
	@IBOutlet var machinePicker: MachinePicker?
	@IBOutlet var machinePickerPanel: NSWindow?
	@IBAction func createMachine(_ sender: NSButton?) {
		self.configureAs(machinePicker!.selectedMachine())
		machinePicker = nil
		self.windowControllers[0].window?.endSheet(self.machinePickerPanel!)
	}

	@IBAction func cancelCreateMachine(_ sender: NSButton?) {
		close()
	}

	// MARK: Joystick-via-the-keyboard selection
	@IBAction func useKeyboardAsKeyboard(_ sender: NSMenuItem?) {
		machine.inputMode = .keyboard
	}

	@IBAction func useKeyboardAsJoystick(_ sender: NSMenuItem?) {
		machine.inputMode = .joystick
	}

	override func validateUserInterfaceItem(_ item: NSValidatedUserInterfaceItem) -> Bool {
		if let menuItem = item as? NSMenuItem {
			switch item.action {
				case #selector(self.useKeyboardAsKeyboard):
					if machine == nil || !machine.hasExclusiveKeyboard {
						menuItem.state = .off
						return false
					}

					menuItem.state = machine.inputMode == .keyboard ? .on : .off
					return true

				case #selector(self.useKeyboardAsJoystick):
					if machine == nil || !machine.hasJoystick {
						menuItem.state = .off
						return false
					}

					menuItem.state = machine.inputMode == .joystick ? .on : .off
					return true

				case #selector(self.showActivity(_:)):
					return self.activityPanel != nil

				case #selector(self.insertMedia(_:)):
					return self.machine != nil && self.machine.canInsertMedia

				default: break
			}
		}
		return super.validateUserInterfaceItem(item)
	}

	// Screenshot capture.
	@IBAction func saveScreenshot(_ sender: AnyObject!) {
		// Grab a date formatter and form a file name.
		let dateFormatter = DateFormatter()
		dateFormatter.dateStyle = .short
		dateFormatter.timeStyle = .long

		let filename = ("Clock Signal Screen Shot " + dateFormatter.string(from: Date()) + ".png").replacingOccurrences(of: "/", with: "-")
			.replacingOccurrences(of: ":", with: ".")
		let pictursURL = FileManager.default.urls(for: .picturesDirectory, in: .userDomainMask)[0]
		let url = pictursURL.appendingPathComponent(filename)

		// Obtain the machine's current display.
		var imageRepresentation: NSBitmapImageRep? = nil
		self.openGLView.perform {
			imageRepresentation = self.machine.imageRepresentation
		}

		// Encode as a PNG and save.
		let pngData = imageRepresentation!.representation(using: .png, properties: [:])
		try! pngData?.write(to: url)
	}

	// MARK: Activity display.
	class LED {
		let levelIndicator: NSLevelIndicator
		init(levelIndicator: NSLevelIndicator) {
			self.levelIndicator = levelIndicator
		}
		var isLit = false
		var isBlinking = false
	}
	fileprivate var leds: [String: LED] = [:]
	func setupActivityDisplay() {
		var leds = machine.leds
		if leds.count > 0 {
			Bundle.main.loadNibNamed("Activity", owner: self, topLevelObjects: nil)
			showActivity(nil)

			// Inspect the activity panel for indicators.
			var activityIndicators: [NSLevelIndicator] = []
			var textFields: [NSTextField] = []
			if let contentView = self.activityPanel.contentView {
				for view in contentView.subviews {
					if let levelIndicator = view as? NSLevelIndicator {
						activityIndicators.append(levelIndicator)
					}

					if let textField = view as? NSTextField {
						textFields.append(textField)
					}
				}
			}

			// If there are fewer level indicators than LEDs, trim that list.
			if activityIndicators.count < leds.count {
				leds.removeSubrange(activityIndicators.count ..< leds.count)
			}

			// Remove unused views.
			for c in leds.count ..< activityIndicators.count {
				textFields[c].removeFromSuperview()
				activityIndicators[c].removeFromSuperview()
			}

			// Apply labels and create leds entries.
			for c in 0 ..< leds.count {
				textFields[c].stringValue = leds[c]
				self.leds[leds[c]] = LED(levelIndicator: activityIndicators[c])
			}

			// Add a constraints to minimise window height.
			let heightConstraint = NSLayoutConstraint(
				item: self.activityPanel.contentView!,
				attribute: .bottom,
				relatedBy: .equal,
				toItem: activityIndicators[leds.count-1],
				attribute: .bottom,
				multiplier: 1.0,
				constant: 20.0)
			self.activityPanel.contentView?.addConstraint(heightConstraint)
		}
	}

	func machine(_ machine: CSMachine, ledShouldBlink ledName: String) {
		// If there is such an LED, switch it off for 0.03 of a second; if it's meant
		// to be off at the end of that, leave it off. Don't allow the blinks to
		// pile up — allow there to be only one in flight at a time.
		if let led = leds[ledName] {
			DispatchQueue.main.async {
				if !led.isBlinking {
					led.levelIndicator.floatValue = 0.0
					led.isBlinking = true

					DispatchQueue.main.asyncAfter(deadline: .now() + 0.03) {
						led.levelIndicator.floatValue = led.isLit ? 1.0 : 0.0
						led.isBlinking = false
					}
				}
			}
		}
	}

	func machine(_ machine: CSMachine, led ledName: String, didChangeToLit isLit: Bool) {
		// If there is such an LED, switch it appropriately.
		if let led = leds[ledName] {
			DispatchQueue.main.async {
				led.levelIndicator.floatValue = isLit ? 1.0 : 0.0
				led.isLit = isLit
			}
		}
	}
}
