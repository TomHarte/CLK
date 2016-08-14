//
//  Vic20Document.swift
//  Clock Signal
//
//  Created by Thomas Harte on 04/06/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Foundation

class Vic20Document: MachineDocument {

	private lazy var vic20 = CSVic20()
	override var machine: CSMachine! {
		get {
			return vic20
		}
	}
	override var name: String! {
		get {
			return "vic20"
		}
	}

	// MARK: NSDocument overrides
	override init() {
		super.init()

		if let kernel = rom("kernel-ntsc"), basic = rom("basic"), characters = rom("characters-english") {
			vic20.setKernelROM(kernel)
			vic20.setBASICROM(basic)
			vic20.setCharactersROM(characters)
		}

		if let drive = dataForResource("1540", ofType: "bin", inDirectory: "ROMImages/Commodore1540") {
			vic20.setDriveROM(drive)
		}

		establishStoredOptions()
	}

	override class func autosavesInPlace() -> Bool {
		return true
	}

	override var windowNibName: String? {
		return "Vic20Document"
	}

	override func readFromURL(url: NSURL, ofType typeName: String) throws {
		if let pathExtension = url.pathExtension {
			switch pathExtension.lowercaseString {
				case "tap":	vic20.openTAPAtURL(url)
				case "g64":	vic20.openG64AtURL(url)
				case "d64":	vic20.openD64AtURL(url)
				default:
					let fileWrapper = try NSFileWrapper(URL: url, options: NSFileWrapperReadingOptions(rawValue: 0))
					try self.readFromFileWrapper(fileWrapper, ofType: typeName)
			}
		}
	}

	// MARK: machine setup
	private func rom(name: String) -> NSData? {
		return dataForResource(name, ofType: "bin", inDirectory: "ROMImages/Vic20")
	}

	override func readFromData(data: NSData, ofType typeName: String) throws {
		vic20.setPRG(data)
	}

	// MARK: automatic loading tick box
	@IBOutlet var loadAutomaticallyButton: NSButton?
	var autoloadingUserDefaultsKey: String {
		get { return prefixedUserDefaultsKey("autoload") }
	}

	@IBAction func setShouldLoadAutomatically(sender: NSButton!) {
		let loadAutomatically = sender.state == NSOnState
		vic20.shouldLoadAutomatically = loadAutomatically
		NSUserDefaults.standardUserDefaults().setBool(loadAutomatically, forKey: self.autoloadingUserDefaultsKey)
	}

	// MARK: country selector
	@IBOutlet var countryButton: NSPopUpButton?
	var countryUserDefaultsKey: String {
		get { return prefixedUserDefaultsKey("country") }
	}

	@IBAction func setCountry(sender: NSPopUpButton!) {
		print("Country should be \(sender.indexOfSelectedItem)")
	}

	// MARK: memory model selector
	@IBOutlet var memorySizeButton: NSPopUpButton?
	var memorySizeUserDefaultsKey: String {
		get { return prefixedUserDefaultsKey("memorySize") }
	}

	@IBAction func setMemorySize(sender: NSPopUpButton!) {
		print("Memory size should be \(sender.indexOfSelectedItem)")
	}

	// MARK: option restoration
	override func establishStoredOptions() {
		super.establishStoredOptions()

		let standardUserDefaults = NSUserDefaults.standardUserDefaults()
		standardUserDefaults.registerDefaults([
			autoloadingUserDefaultsKey: true
		])

		let loadAutomatically = standardUserDefaults.boolForKey(self.autoloadingUserDefaultsKey)
		vic20.shouldLoadAutomatically = loadAutomatically
		self.loadAutomaticallyButton?.state = loadAutomatically ? NSOnState : NSOffState
	}
}
