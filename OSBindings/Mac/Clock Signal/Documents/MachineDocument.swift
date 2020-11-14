//
//  MachineDocument.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/01/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

import AudioToolbox
import Cocoa
import QuartzCore

class MachineDocument:
	NSDocument,
	NSWindowDelegate,
	CSMachineDelegate,
	CSScanTargetViewResponderDelegate,
	CSAudioQueueDelegate,
	CSROMReciverViewDelegate
{
	// MARK: - Mutual Exclusion.

	/// Ensures exclusive access between calls to self.machine.run and close().
	private let actionLock = NSLock()
	/// Ensures exclusive access between calls to machine.updateView and machine.drawView, and close().
	private let drawLock = NSLock()

	// MARK: - Machine details.

	/// A description of the machine this document should represent once fully set up.
	private var machineDescription: CSStaticAnalyser?

	/// The active machine, following its successful creation.
	private var machine: CSMachine!

	/// @returns the appropriate window content aspect ratio for this @c self.machine.
	private func aspectRatio() -> NSSize {
		return NSSize(width: 4.0, height: 3.0)
	}

	/// The output audio queue, if any.
	private var audioQueue: CSAudioQueue!

	// MARK: - Main NIB connections.

	/// The OpenGL view to receive this machine's display.
	@IBOutlet weak var scanTargetView: CSScanTargetView!

	/// The options panel, if any.
	@IBOutlet var optionsPanel: MachinePanel!

	/// An action to display the options panel, if there is one.
	@IBAction func showOptions(_ sender: AnyObject!) {
		optionsPanel?.setIsVisible(true)
	}

	/// The activity panel, if one is deemed appropriate.
	@IBOutlet var activityPanel: NSPanel!

	/// An action to display the activity panel, if there is one.
	@IBAction func showActivity(_ sender: AnyObject!) {
		activityPanel.setIsVisible(true)
	}

	/// The volume view.
	@IBOutlet var volumeView: NSBox!
	@IBOutlet var volumeSlider: NSSlider!

	// MARK: - NSDocument Overrides and NSWindowDelegate methods.

	/// Links this class to the MachineDocument NIB.
	override var windowNibName: NSNib.Name? {
		return "MachineDocument"
	}

	convenience init(type typeName: String) throws {
		self.init()
		self.fileType = typeName
	}

	override func read(from url: URL, ofType typeName: String) throws {
		if let analyser = CSStaticAnalyser(fileAt: url) {
			self.displayName = analyser.displayName
			self.configureAs(analyser)
		} else {
			throw NSError(domain: "MachineDocument", code: -1, userInfo: nil)
		}
	}

	override func close() {
		machine?.stop()

		activityPanel?.setIsVisible(false)
		activityPanel = nil

		optionsPanel?.setIsVisible(false)
		optionsPanel = nil

		actionLock.lock()
		drawLock.lock()
		machine = nil
		scanTargetView.invalidate()
		actionLock.unlock()
		drawLock.unlock()

		super.close()
	}

	override func data(ofType typeName: String) throws -> Data {
		throw NSError(domain: NSOSStatusErrorDomain, code: unimpErr, userInfo: nil)
	}

	override func windowControllerDidLoadNib(_ aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)
		aController.window?.contentAspectRatio = self.aspectRatio()
		volumeSlider.floatValue = userDefaultsVolume()
	}

	private var missingROMs: [CSMissingROM] = []
	func configureAs(_ analysis: CSStaticAnalyser) {
		self.machineDescription = analysis

		let missingROMs = NSMutableArray()
		if let machine = CSMachine(analyser: analysis, missingROMs: missingROMs) {
			self.machine = machine
			setupMachineOutput()
			setupActivityDisplay()
			machine.setVolume(userDefaultsVolume())
		} else {
			// Store the selected machine and list of missing ROMs, and
			// show the missing ROMs dialogue.
			self.missingROMs = []
			for untypedMissingROM in missingROMs {
				self.missingROMs.append(untypedMissingROM as! CSMissingROM)
			}

			requestRoms()
		}
	}

	enum InteractionMode {
		case notStarted, showingMachinePicker, showingROMRequester, showingMachine
	}
	private var interactionMode: InteractionMode = .notStarted

	// Attempting to show a sheet before the window is visible (such as when the NIB is loaded) results in
	// a sheet mysteriously floating on its own. For now, use windowDidUpdate as a proxy to check whether
	// the window is visible.
	func windowDidUpdate(_ notification: Notification) {
		if let window = self.windowControllers[0].window, window.isVisible {
			// Grab the regular window title, if it's not already stored.
			if self.unadornedWindowTitle.count == 0 {
				self.unadornedWindowTitle = window.title
			}

			// If an interaction mode is not yet in effect, pick the proper one and display the relevant thing.
			if self.interactionMode == .notStarted {
				// If a full machine exists, just continue showing it.
				if self.machine != nil {
					self.interactionMode = .showingMachine
					setupMachineOutput()
					return
				}

				// If a machine has been picked but is not showing, there must be ROMs missing.
				if self.machineDescription != nil {
					self.interactionMode = .showingROMRequester
					requestRoms()
					return
				}

				// If a machine hasn't even been picked yet, show the machine picker.
				self.interactionMode = .showingMachinePicker
				Bundle.main.loadNibNamed("MachinePicker", owner: self, topLevelObjects: nil)
				self.machinePicker?.establishStoredOptions()
				window.beginSheet(self.machinePickerPanel!, completionHandler: nil)
			}
		}
	}

	// MARK: - Connections Between Machine and the Outside World

	private func setupMachineOutput() {
		if let machine = self.machine, let scanTargetView = self.scanTargetView, machine.view != scanTargetView {
			// Establish the output aspect ratio and audio.
			let aspectRatio = self.aspectRatio()
			machine.setView(scanTargetView, aspectRatio: Float(aspectRatio.width / aspectRatio.height))

			// Attach an options panel if one is available.
			if let optionsPanelNibName = self.machineDescription?.optionsPanelNibName {
				Bundle.main.loadNibNamed(optionsPanelNibName, owner: self, topLevelObjects: nil)
				self.optionsPanel.machine = machine
				self.optionsPanel?.establishStoredOptions()
				showOptions(self)
			}

			machine.delegate = self

			// Callbacks from the OpenGL may come on a different thread, immediately following the .delegate set;
			// hence the full setup of the best-effort updater prior to setting self as a delegate.
//			scanTargetView.delegate = self
			scanTargetView.responderDelegate = self

			// If this machine has a mouse, enable mouse capture; also indicate whether usurption
			// of the command key is desired.
			scanTargetView.shouldCaptureMouse = machine.hasMouse
			scanTargetView.shouldUsurpCommand = machine.shouldUsurpCommand

			setupAudioQueueClockRate()

			// Bring OpenGL view-holding window on top of the options panel and show the content.
			scanTargetView.isHidden = false
			scanTargetView.window!.makeKeyAndOrderFront(self)
			scanTargetView.window!.makeFirstResponder(scanTargetView)

			// Start forwarding best-effort updates.
			machine.start()
		}
	}

	func machineSpeakerDidChangeInputClock(_ machine: CSMachine) {
		// setupAudioQueueClockRate not only needs blocking access to the machine,
		// but may be triggered on an arbitrary thread by a running machine, and that
		// running machine may not be able to stop running until it has been called
		// (e.g. if it is currently trying to run_until an audio event). Break the
		// deadlock with an async dispatch. 
		DispatchQueue.main.async {
			self.setupAudioQueueClockRate()
		}
	}

	private func setupAudioQueueClockRate() {
		// Establish and provide the audio queue, taking advice as to an appropriate sampling rate.
		//
		// TODO: this needs to be threadsafe. FIX!
		let maximumSamplingRate = CSAudioQueue.preferredSamplingRate()
		let selectedSamplingRate = Float64(self.machine.idealSamplingRate(from: NSRange(location: 0, length: NSInteger(maximumSamplingRate))))
		let isStereo = self.machine.isStereo()
		if selectedSamplingRate > 0 {
			// [Re]create the audio queue only if necessary.
			if self.audioQueue == nil || self.audioQueue.samplingRate != selectedSamplingRate {
				self.machine.audioQueue = nil
				self.audioQueue = CSAudioQueue(samplingRate: Float64(selectedSamplingRate), isStereo:isStereo)
				self.audioQueue.delegate = self
				self.machine.audioQueue = self.audioQueue
				self.machine.setAudioSamplingRate(Float(selectedSamplingRate), bufferSize:audioQueue.preferredBufferSize, stereo:isStereo)
			}
		}
	}

	/// Responds to the CSAudioQueueDelegate dry-queue warning message by requesting a machine update.
	final func audioQueueIsRunningDry(_ audioQueue: CSAudioQueue) {
	}

	// MARK: - Pasteboard Forwarding.

	/// Forwards any text currently on the pasteboard into the active machine.
	func paste(_ sender: Any) {
		let pasteboard = NSPasteboard.general
		if let string = pasteboard.string(forType: .string), let machine = self.machine {
			machine.paste(string)
		}
	}

	// MARK: - Runtime Media Insertion.

	/// Delegate message to receive drag and drop files.
	final func scanTargetView(_ view: CSScanTargetView, didReceiveFileAt URL: URL) {
		let mediaSet = CSMediaSet(fileAt: URL)
		if let mediaSet = mediaSet {
			mediaSet.apply(to: self.machine)
		}
	}

	/// Action for the insert menu command; displays an NSOpenPanel and then segues into the same process
	/// as if a file had been received via drag and drop.
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

	// MARK: - Input Management.

	/// Upon a resign key, immediately releases all ongoing input mechanisms — any currently pressed keys,
	/// and joystick and mouse inputs.
	func windowDidResignKey(_ notification: Notification) {
		if let machine = self.machine {
			machine.clearAllKeys()
			machine.joystickManager = nil
		}
		self.scanTargetView.releaseMouse()
	}

	/// Upon becoming key, attaches joystick input to the machine.
	func windowDidBecomeKey(_ notification: Notification) {
		if let machine = self.machine {
			machine.joystickManager = (DocumentController.shared as! DocumentController).joystickManager
		}
	}

	/// Forwards key down events directly to the machine.
	func keyDown(_ event: NSEvent) {
		if let machine = self.machine {
			machine.setKey(event.keyCode, characters: event.characters, isPressed: true)
		}
	}

	/// Forwards key up events directly to the machine.
	func keyUp(_ event: NSEvent) {
		if let machine = self.machine {
			machine.setKey(event.keyCode, characters: event.characters, isPressed: false)
		}
	}

	/// Synthesies appropriate key up and key down events upon any change in modifiers.
	func flagsChanged(_ newModifiers: NSEvent) {
		if let machine = self.machine {
			machine.setKey(VK_Shift, characters: nil, isPressed: newModifiers.modifierFlags.contains(.shift))
			machine.setKey(VK_Control, characters: nil, isPressed: newModifiers.modifierFlags.contains(.control))
			machine.setKey(VK_Command, characters: nil, isPressed: newModifiers.modifierFlags.contains(.command))
			machine.setKey(VK_Option, characters: nil, isPressed: newModifiers.modifierFlags.contains(.option))
		}
	}

	/// Forwards mouse movement events to the mouse.
	func mouseMoved(_ event: NSEvent) {
		if let machine = self.machine {
			machine.addMouseMotionX(event.deltaX, y: event.deltaY)
		}
	}

	/// Forwards mouse button down events to the mouse.
	func mouseUp(_ event: NSEvent) {
		if let machine = self.machine {
			machine.setMouseButton(Int32(event.buttonNumber), isPressed: false)
		}
	}

	/// Forwards mouse button up events to the mouse.
	func mouseDown(_ event: NSEvent) {
		if let machine = self.machine {
			machine.setMouseButton(Int32(event.buttonNumber), isPressed: true)
		}
	}

	// MARK: - MachinePicker Outlets and Actions
	@IBOutlet var machinePicker: MachinePicker?
	@IBOutlet var machinePickerPanel: NSWindow?
	@IBAction func createMachine(_ sender: NSButton?) {
		let selectedMachine = machinePicker!.selectedMachine()
		self.windowControllers[0].window?.endSheet(self.machinePickerPanel!)
		self.machinePicker = nil
		self.configureAs(selectedMachine)
	}

	@IBAction func cancelCreateMachine(_ sender: NSButton?) {
		self.windowControllers[0].window?.endSheet(self.machinePickerPanel!)
		self.machinePicker = nil
		close()
	}

	// MARK: - ROMRequester Outlets and Actions
	@IBOutlet var romRequesterPanel: NSWindow?
	@IBOutlet var romRequesterText: NSTextField?
	@IBOutlet var romReceiverErrorField: NSTextField?
	@IBOutlet var romReceiverView: CSROMReceiverView?
	private var romRequestBaseText = ""
	func requestRoms() {
		// Don't act yet if there's no window controller yet.
		if self.windowControllers.count == 0 {
			return
		}

		// Load the ROM requester dialogue.
		Bundle.main.loadNibNamed("ROMRequester", owner: self, topLevelObjects: nil)
		self.romReceiverView!.delegate = self
		self.romRequestBaseText = romRequesterText!.stringValue
		romReceiverErrorField?.alphaValue = 0.0

		// Populate the current absentee list.
		populateMissingRomList()

		// Show the thing.
		self.windowControllers[0].window?.beginSheet(self.romRequesterPanel!, completionHandler: nil)
	}

	@IBAction func cancelRequestROMs(_ sender: NSButton?) {
		self.windowControllers[0].window?.endSheet(self.romRequesterPanel!)
		close()
	}

	func populateMissingRomList() {
		// Fill in the missing details; first build a list of all the individual
		// line items.
		var requestLines: [String] = []
		for missingROM in self.missingROMs {
			if let descriptiveName = missingROM.descriptiveName {
				requestLines.append("• " + descriptiveName)
			} else {
				requestLines.append("• " + missingROM.fileName)
			}
		}

		// Suffix everything up to the penultimate line with a semicolon;
		// the penultimate line with a semicolon and a conjunctive; the final
		// line with a full stop.
		for x in 0 ..< requestLines.count {
			if x < requestLines.count - 2 {
				requestLines[x].append(";")
			} else if x < requestLines.count - 1 {
				requestLines[x].append("; and")
			} else {
				requestLines[x].append(".")
			}
		}
		romRequesterText!.stringValue = self.romRequestBaseText + requestLines.joined(separator: "\n")
	}

	func romReceiverView(_ view: CSROMReceiverView, didReceiveFileAt URL: URL) {
		// Test whether the file identified matches any of the currently missing ROMs.
		// If so then remove that ROM from the missing list and update the request screen.
		// If no ROMs are still missing, start the machine.
		do {
			let fileData = try Data(contentsOf: URL)
			var didInstallRom = false

			// Try to match by size first, CRC second. Accept that some ROMs may have
			// some additional appended data. Arbitrarily allow them to be up to 10kb
			// too large.
			var index = 0
			for missingROM in self.missingROMs {
				if fileData.count >= missingROM.size && fileData.count < missingROM.size + 10*1024 {
					// Trim to size.
					let trimmedData = fileData[0 ..< missingROM.size]

					// Get CRC.
					if missingROM.crc32s.contains( (trimmedData as NSData).crc32 ) {
						// This ROM matches; copy it into the application library,
						// strike it from the missing ROM list and decide how to
						// proceed.
						let fileManager = FileManager.default
						let targetPath = fileManager.urls(for: .applicationSupportDirectory, in: .userDomainMask)[0]
							.appendingPathComponent("ROMImages")
							.appendingPathComponent(missingROM.machineName)
						let targetFile = targetPath
							.appendingPathComponent(missingROM.fileName)

						do {
							try fileManager.createDirectory(atPath: targetPath.path, withIntermediateDirectories: true, attributes: nil)
							try trimmedData.write(to: targetFile)
						} catch let error {
							showRomReceiverError(error: "Couldn't write to application support directory: \(error)")
						}

						self.missingROMs.remove(at: index)
						didInstallRom = true
						break
					}
				}

				index = index + 1
			}

			if didInstallRom {
				if self.missingROMs.count == 0 {
					self.windowControllers[0].window?.endSheet(self.romRequesterPanel!)
					configureAs(self.machineDescription!)
				} else {
					populateMissingRomList()
				}
			} else {
				showRomReceiverError(error: "Didn't recognise contents of \(URL.lastPathComponent)")
			}
		} catch let error {
			showRomReceiverError(error: "Couldn't read file at \(URL.absoluteString): \(error)")
		}
	}

	// Yucky ugliness follows; my experience as an iOS developer intersects poorly with
	// NSAnimationContext hence the various stateful diplications below. isShowingError
	// should be essentially a duplicate of the current alphaValue, and animationCount
	// is to resolve my inability to figure out how to cancel scheduled animations.
	private var errorText = ""
	private var isShowingError = false
	private var animationCount = 0
	private func showRomReceiverError(error: String) {
		// Set or append the new error.
		if self.errorText.count > 0 {
			self.errorText = self.errorText + "\n" + error
		} else {
			self.errorText = error
		}

		// Apply the new complete text.
		romReceiverErrorField!.stringValue = self.errorText

		if !isShowingError {
			// Schedule the box's appearance.
			NSAnimationContext.beginGrouping()
			NSAnimationContext.current.duration = 0.1
			romReceiverErrorField?.animator().alphaValue = 1.0
			NSAnimationContext.endGrouping()
			isShowingError = true
		}

		// Schedule the box to disappear.
		self.animationCount = self.animationCount + 1
		let capturedAnimationCount = animationCount
		DispatchQueue.main.asyncAfter(deadline: DispatchTime.now() + .seconds(2)) {
			if self.animationCount == capturedAnimationCount {
				NSAnimationContext.beginGrouping()
				NSAnimationContext.current.duration = 1.0
				self.romReceiverErrorField?.animator().alphaValue = 0.0
				NSAnimationContext.endGrouping()
				self.isShowingError = false
				self.errorText = ""
			}
		}
	}

	// MARK: Joystick-via-the-keyboard selection
	@IBAction func useKeyboardAsPhysicalKeyboard(_ sender: NSMenuItem?) {
		machine.inputMode = .keyboardPhysical
	}

	@IBAction func useKeyboardAsLogicalKeyboard(_ sender: NSMenuItem?) {
		machine.inputMode = .keyboardLogical
	}

	@IBAction func useKeyboardAsJoystick(_ sender: NSMenuItem?) {
		machine.inputMode = .joystick
	}

	/// Determines which of the menu items to enable and disable based on the ability of the
	/// current machine to handle keyboard and joystick input, accept new media and whether
	/// it has an associted activity window.
	override func validateUserInterfaceItem(_ item: NSValidatedUserInterfaceItem) -> Bool {
		if let menuItem = item as? NSMenuItem {
			switch item.action {
				case #selector(self.useKeyboardAsPhysicalKeyboard):
					if machine == nil || !machine.hasExclusiveKeyboard {
						menuItem.state = .off
						return false
					}

					menuItem.state = machine.inputMode == .keyboardPhysical ? .on : .off
					return true

				case #selector(self.useKeyboardAsLogicalKeyboard):
					if machine == nil || !machine.hasExclusiveKeyboard {
						menuItem.state = .off
						return false
					}

					menuItem.state = machine.inputMode == .keyboardLogical ? .on : .off
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

	/// Saves a screenshot of the
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
		let imageRepresentation = self.machine.imageRepresentation

		// Encode as a PNG and save.
		let pngData = imageRepresentation.representation(using: .png, properties: [:])
		try! pngData?.write(to: url)
	}

	// MARK: - Window Title Updates.
	private var unadornedWindowTitle = ""
	internal func scanTargetViewDidCaptureMouse(_ view: CSScanTargetView) {
		self.windowControllers[0].window?.title = self.unadornedWindowTitle + " (press ⌘+control to release mouse)"
	}

	internal func scanTargetViewDidReleaseMouse(_ view: CSScanTargetView) {
		self.windowControllers[0].window?.title = self.unadornedWindowTitle
	}

	// MARK: - Activity Display.

	private class LED {
		let levelIndicator: NSLevelIndicator
		init(levelIndicator: NSLevelIndicator) {
			self.levelIndicator = levelIndicator
		}
		var isLit = false
		var isBlinking = false
	}
	private var leds: [String: LED] = [:]

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

	// MARK: - Volume Control.
	@IBAction func setVolume(_ sender: NSSlider!) {
		if let machine = self.machine {
			machine.setVolume(sender.floatValue)
			setUserDefaultsVolume(sender.floatValue)
		}
	}

	// This class is pure nonsense to work around Xcode's opaque behaviour.
	// If I make the main class a sub of CAAnimationDelegate then the compiler
	// generates a bridging header that doesn't include QuartzCore and therefore
	// can't find a declaration of the CAAnimationDelegate protocol. Doesn't
	// seem to matter what I add explicitly to the link stage, which version of
	// macOS I set as the target, etc.
	//
	// So, the workaround: make my CAAnimationDelegate something that doesn't
	// appear in the bridging header.
	fileprivate class ViewFader: NSObject, CAAnimationDelegate {
		var volumeView: NSView

		init(view: NSView) {
			volumeView = view
		}

		func animationDidStop(_ anim: CAAnimation, finished flag: Bool) {
			volumeView.isHidden = true
		}
	}
	fileprivate var animationFader: ViewFader? = nil

	internal func scanTargetViewDidShowOSMouseCursor(_ view: CSScanTargetView) {
		// The OS mouse cursor became visible, so show the volume controls.
		animationFader = nil
		volumeView.layer?.removeAllAnimations()
		volumeView.isHidden = false
		volumeView.layer?.opacity = 1.0
	}

	internal func scanTargetViewWillHideOSMouseCursor(_ view: CSScanTargetView) {
		// The OS mouse cursor will be hidden, so hide the volume controls.
		if !volumeView.isHidden && volumeView.layer?.animation(forKey: "opacity") == nil {
			let fadeAnimation = CABasicAnimation(keyPath: "opacity")
			fadeAnimation.fromValue = 1.0
			fadeAnimation.toValue = 0.0
			fadeAnimation.duration = 0.2
			animationFader = ViewFader(view: volumeView)
			fadeAnimation.delegate = animationFader
			volumeView.layer?.add(fadeAnimation, forKey: "opacity")
			volumeView.layer?.opacity = 0.0
		}
	}

	// The user's selected volume is stored as 1 - volume in the user defaults in order
	// to take advantage of the default value being 0.
	private func userDefaultsVolume() -> Float {
		return 1.0 - UserDefaults.standard.float(forKey: "defaultVolume")
	}

	private func setUserDefaultsVolume(_ volume: Float) {
		UserDefaults.standard.set(1.0 - volume, forKey: "defaultVolume")
	}
}
