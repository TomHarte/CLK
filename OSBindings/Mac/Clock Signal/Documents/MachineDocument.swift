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

	/// The options view, if any.
	@IBOutlet var optionsView: NSView!
	@IBOutlet var optionsController: MachineController!

	/// The activity panel, if one is deemed appropriate.
	@IBOutlet var activityView: NSView!

	/// The volume view.
	@IBOutlet var volumeView: NSView!
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
		// Close any dangling sheets.
		//
		// Be warned: in 11.0 at least, if there are any panels then posting the endSheet request
		// will defer the close(), and close() will be called again at the end of that animation.
		//
		// So: MAKE SURE IT'S SAFE TO ENTER THIS FUNCTION TWICE. Hence the non-assumption here about
		// any windows still existing.
		if let window = self.windowControllers.first?.window {
			for sheet in window.sheets {
				window.endSheet(sheet)
			}
		}

		// Stop the machine, if any.
		machine?.stop()

		// End the update cycle.
		actionLock.lock()
		drawLock.lock()
		machine = nil
		scanTargetView.invalidate()
		actionLock.unlock()
		drawLock.unlock()

		// Let the document controller do its thing.
		super.close()
	}

	override func data(ofType typeName: String) throws -> Data {
		throw NSError(domain: NSOSStatusErrorDomain, code: unimpErr, userInfo: nil)
	}

	override func windowControllerDidLoadNib(_ aController: NSWindowController) {
		super.windowControllerDidLoadNib(aController)
		aController.window?.contentAspectRatio = self.aspectRatio()
		volumeSlider.floatValue = pow(2.0, userDefaultsVolume())

		volumeView.layer!.cornerRadius = 5.0
	}

	private var missingROMs: String = ""
	func configureAs(_ analysis: CSStaticAnalyser) {
		self.machineDescription = analysis

		actionLock.lock()
		drawLock.lock()

		let missingROMs = NSMutableString()
		if let machine = CSMachine(analyser: analysis, missingROMs: missingROMs) {
			setRomRequesterIsVisible(false)

			self.machine = machine
			machine.setVolume(userDefaultsVolume())
			setupMachineOutput()
		} else {
			self.missingROMs = missingROMs as String
			requestRoms()
		}

		actionLock.unlock()
		drawLock.unlock()
	}

	enum InteractionMode {
		case notStarted, showingMachinePicker, showingROMRequester, showingMachine
	}
	private var interactionMode: InteractionMode = .notStarted

	// Attempting to show a sheet before the window is visible (such as when the NIB is loaded) results in
	// a sheet mysteriously floating on its own. For now, use windowDidUpdate as a proxy to check whether
	// the window is visible.
	func windowDidUpdate(_ notification: Notification) {
		if self.windowControllers.count > 0, let window = self.windowControllers[0].window, window.isVisible {
			// Grab the regular window title, if it's not already stored.
			if self.unadornedWindowTitle == "" {
				self.unadornedWindowTitle = window.title
			}
			updateWindowTitle()

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

	func windowDidEnterFullScreen(_ notification: Notification) {
		updateActivityViewVisibility()
	}

	// MARK: - Connections Between Machine and the Outside World.

	private func setupMachineOutput() {
		if let machine = self.machine, let scanTargetView = self.scanTargetView, machine.view != scanTargetView {
			// Establish the output aspect ratio and audio.
			let aspectRatio = self.aspectRatio()
			machine.setView(scanTargetView, aspectRatio: Float(aspectRatio.width / aspectRatio.height))

			// Attach an options panel if one is available.
			if let optionsNibName = self.machineDescription?.optionsNibName {
				Bundle.main.loadNibNamed(optionsNibName, owner: self, topLevelObjects: nil)
				if let optionsController = self.optionsController {
					optionsController.machine = machine
					optionsController.establishStoredOptions()
				}
				if let optionsView = self.optionsView, let superview = self.volumeView.superview {
					// Apply rounded edges.
					optionsView.layer!.cornerRadius = 5.0

					// Add to the superview.
					superview.addSubview(optionsView)

					// Apply constraints to appear centred and above the volume view.
					let constraints = [
						optionsView.centerXAnchor.constraint(equalTo: volumeView.centerXAnchor),
						optionsView.bottomAnchor.constraint(equalTo: volumeView.topAnchor, constant: -8.0),
					]
					superview.addConstraints(constraints)
				}
			}

			// Set up a fader for the volume and options.
			var fadingViews: [NSView] = []
			if let optionsView = self.optionsView {
				fadingViews.append(optionsView)
			}
			if let volumeView = self.volumeView {
				fadingViews.append(volumeView)
			}
			optionsFader = ViewFader(views: fadingViews)

			// Create and populate an activity display if required.
			setupActivityDisplay()

			machine.delegate = self
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
		let isStereo = self.machine.isStereo
		if selectedSamplingRate > 0 {
			// [Re]create the audio queue only if necessary.
			if self.audioQueue == nil || self.audioQueue.samplingRate != selectedSamplingRate || self.audioQueue != self.machine.audioQueue {
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
		insertFile(URL)
	}

	/// Action for the insert menu command; displays an NSOpenPanel and then segues into the same process
	/// as if a file had been received via drag and drop.
	@IBAction final func insertMedia(_ sender: AnyObject!) {
		let openPanel = NSOpenPanel()
		openPanel.message = "Hint: you can also insert media by dragging and dropping it onto the machine's window."
		openPanel.beginSheetModal(for: self.windowControllers[0].window!) { (response) in
			if response == .OK {
				for url in openPanel.urls {
					self.insertFile(url)
				}
			}
		}
	}

	private func insertFile(_ URL: URL) {
		// Try to insert media.
		let mediaSet = CSMediaSet(fileAt: URL)
		if !mediaSet.empty {
			mediaSet.apply(to: self.machine)
			return
		}

		// Failing that see whether a new machine is required.
		// TODO.
		if let newMachine = CSStaticAnalyser(fileAt: URL) {
			machine?.stop()
			self.interactionMode = .notStarted
			self.scanTargetView.willChangeScanTargetOwner()
			configureAs(newMachine)
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

	@IBAction func tableViewDoubleClick(_ sender: NSTableView?) {
		createMachine(nil)
	}

	@IBAction func cancelCreateMachine(_ sender: NSButton?) {
		close()
	}

	// MARK: - ROMRequester Outlets and Actions

	@IBOutlet var romRequesterPanel: NSWindow?
	@IBOutlet var romRequesterText: NSTextField?
	@IBOutlet var romReceiverErrorField: NSTextField?
	@IBOutlet var romReceiverView: CSROMReceiverView?
	private var romRequestBaseText = ""

	private func setRomRequesterIsVisible(_ visible : Bool) {
		if !visible && self.romRequesterPanel == nil {
			return;
		}

		if self.romRequesterPanel!.isVisible == visible {
			return
		}

		if visible {
			self.windowControllers[0].window?.beginSheet(self.romRequesterPanel!, completionHandler: nil)
		} else {
			self.windowControllers[0].window?.endSheet(self.romRequesterPanel!)
		}
	}

	func requestRoms() {
		// Don't act yet if there's no window controller yet.
		if self.windowControllers.count == 0 {
			return
		}

		// Load the ROM requester dialogue if it's not already loaded.
		if self.romRequesterPanel == nil {
			Bundle.main.loadNibNamed("ROMRequester", owner: self, topLevelObjects: nil)
			self.romReceiverView!.delegate = self
			self.romRequestBaseText = romRequesterText!.stringValue
			romReceiverErrorField?.alphaValue = 0.0
		}

		// Populate the current absentee list.
		populateMissingRomList()

		// Show the thing.
		setRomRequesterIsVisible(true)
	}

	@IBAction func cancelRequestROMs(_ sender: NSButton?) {
		close()
	}

	func populateMissingRomList() {
		romRequesterText!.stringValue = self.romRequestBaseText + self.missingROMs
	}

	func romReceiverView(_ view: CSROMReceiverView, didReceiveFileAt URL: URL) {
		// Test whether the file identified matches any of the currently missing ROMs.
		// If so then remove that ROM from the missing list and update the request screen.
		// If no ROMs are still missing, start the machine.
		if CSMachine.attemptInstallROM(URL) {
			configureAs(self.machineDescription!)
		} else {
			showRomReceiverError(error: "Didn't recognise contents of \(URL.lastPathComponent)")
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

	// MARK: - Joystick-via-the-keyboard selection.

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

				case #selector(self.insertMedia(_:)):
					return self.machine != nil && self.machine.canInsertMedia

				default: break
			}
		}
		return super.validateUserInterfaceItem(item)
	}

	// MARK: - Screenshots.

	/// Saves a screenshot of the machine's current display.
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
	private var mouseIsCaptured = false
	private var windowTitleSuffix = ""

	private func updateWindowTitle() {
		var title = self.unadornedWindowTitle
		if windowTitleSuffix != "" {
			title += windowTitleSuffix
		}
		if mouseIsCaptured {
			title += " (press ⌘+control to release mouse)"
		}
		self.windowControllers[0].window?.title = title
	}

	internal func scanTargetViewDidCaptureMouse(_ view: CSScanTargetView) {
		mouseIsCaptured = true
		updateWindowTitle()
	}

	internal func scanTargetViewDidReleaseMouse(_ view: CSScanTargetView) {
		mouseIsCaptured = false
		updateWindowTitle()
	}

	// MARK: - Activity Display.

	private class LED {
		let levelIndicator: NSLevelIndicator
		init(levelIndicator: NSLevelIndicator, isPersistent: Bool) {
			self.levelIndicator = levelIndicator
			self.isPersistent = isPersistent
		}
		var isLit = false
		var isBlinking = false
		var isPersistent = false
	}
	private var leds: [String: LED] = [:]
	private var activityFader: ViewFader! = nil

	func setupActivityDisplay() {
		var leds = machine.leds
		if leds.count > 0 {
			Bundle.main.loadNibNamed("Activity", owner: self, topLevelObjects: nil)

			// Inspect the activity panel for indicators.
			var activityIndicators: [NSLevelIndicator] = []
			var textFields: [NSTextField] = []
			if let activityView = self.activityView {
				for view in activityView.subviews {
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
				textFields[c].stringValue = leds[c].name
				self.leds[leds[c].name] = LED(levelIndicator: activityIndicators[c], isPersistent: leds[c].isPersisent)
			}

			// Create a fader.
			activityFader = ViewFader(views: [self.activityView!])

			// Add view to window, and constrain.
			if let superview = activityIndicators[leds.count-1].superview {
				superview.addConstraint(
					activityIndicators[leds.count-1].bottomAnchor.constraint(equalTo: activityIndicators[leds.count-1].superview!.bottomAnchor, constant: -8.0)
				)
			}
			if let windowView = self.volumeView.superview {
				windowView.addSubview(self.activityView)

				let constraints = [
					self.activityView.rightAnchor.constraint(equalTo: windowView.rightAnchor),
					self.activityView.topAnchor.constraint(equalTo: windowView.topAnchor),
				]
				windowView.addConstraints(constraints)

				activityView.layer!.cornerRadius = 5.0
				activityView.layer!.maskedCorners = [.layerMinXMinYCorner]
			}

			// Show or hide activity view as per current state.
			updateActivityViewVisibility(true)
		}
	}

	func machine(_ machine: CSMachine, ledShouldBlink ledName: String) {
		// If there is such an LED, switch it off for 0.03 of a second; if it's meant
		// to be off at the end of that, leave it off. Don't allow the blinks to
		// pile up — allow there to be only one in flight at a time.
		if let led = leds[ledName] {
			DispatchQueue.main.async {
				if !led.isBlinking && led.isLit {
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
			DispatchQueue.main.async { [self] in
				// Do nothing for no change of state.
				if led.isLit == isLit {
					return
				}

				led.levelIndicator.floatValue = isLit ? 1.0 : 0.0
				led.isLit = isLit

				// Possibly show or hide the activity subview.
				self.updateActivityViewVisibility(false, changed: ledName)
			}
		}
	}

	private func updateActivityViewVisibility(_ isAppLaunch : Bool = false, changed: String? = nil) {
		if let window = self.windowControllers.first?.window, let activityFader = self.activityFader {
			// Rules applied below:
			//
			// Fullscreen:
			//	(i) always show activity view if any persistent LEDs are present;
			//	(ii) otherwise, show activity view only while at least one LED is lit.
			//
			// Windowed:
			//	(i) show while any non-persistent LED is lit;
			//	(ii) show transiently to indicate a change of state in any persistent LED.
			//
			let hasLitLEDs = !self.leds.filter {
				$0.value.isLit && (!$0.value.isPersistent || window.styleMask.contains(.fullScreen)) ||
				($0.value.isPersistent && window.styleMask.contains(.fullScreen))
			}.isEmpty
			let shouldShowTransient = !window.styleMask.contains(.fullScreen) && changed != nil && self.leds[changed!]!.isPersistent

			if hasLitLEDs {
				activityFader.animateIn()
			} else if shouldShowTransient {
				activityFader.showTransiently(for: 1.0)
			} else {
				activityFader.animateOut(delay: 0.2)
			}
		}
	}

	// MARK: - In-window panels (i.e. options, volume).

	private var optionsFader: ViewFader! = nil

	internal func scanTargetViewDidShowOSMouseCursor(_ view: CSScanTargetView) {
		// The OS mouse cursor became visible, so show the volume controls.
		optionsFader.animateIn()
	}

	internal func scanTargetViewWillHideOSMouseCursor(_ view: CSScanTargetView) {
		// The OS mouse cursor will be hidden, so hide the volume controls.
		optionsFader.animateOut(delay: 0.0)
	}

	// MARK: - Helpers for fading things in and out.

	/// Maintains a list of views and offers in-and-out animations on those,
	/// testing current state as necessary and otherwise coordinating with
	/// CoreAnimation.
	private class ViewFader: NSObject, CAAnimationDelegate {
		private var views: [NSView]

		init(views: [NSView]) {
			self.views = views
			for view in views {
				view.isHidden = true
			}
		}

		func animationDidStop(_ animation: CAAnimation, finished: Bool) {
			if finished {
				for view in views {
					view.isHidden = true
				}
			}
		}

		func animateIn() {
			for view in views {
				view.layer?.removeAllAnimations()
				view.isHidden = false
			}
		}

		func animateOut(delay : TimeInterval) {
			// Do nothing if already animating out or invisible.
			if views[0].isHidden || views[0].layer?.animation(forKey: "opacity") != nil {
				return
			}

			for view in views {
				let fadeAnimation = CABasicAnimation(keyPath: "opacity")
				fadeAnimation.beginTime = CACurrentMediaTime() + delay
				fadeAnimation.fromValue = 1.0
				fadeAnimation.toValue = 0.0
				fadeAnimation.duration = 0.2
				fadeAnimation.delegate = self

				fadeAnimation.fillMode = .forwards
				fadeAnimation.isRemovedOnCompletion = false

				view.layer?.removeAllAnimations()
				view.layer!.add(fadeAnimation, forKey: "opacity")
			}
		}

		func showTransiently(for period: TimeInterval) {
			animateIn()
			animateOut(delay: period)
		}
	}

	// MARK: - Volume Control.

	@IBAction func setVolume(_ sender: NSSlider!) {
		if let machine = self.machine {
			let linearValue = log2(sender.floatValue)
			machine.setVolume(linearValue)
			setUserDefaultsVolume(linearValue)
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
