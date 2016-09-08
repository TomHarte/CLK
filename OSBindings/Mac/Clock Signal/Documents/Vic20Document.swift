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
				case "prg": vic20.openPRGAtURL(url)
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
		NSUserDefaults.standardUserDefaults().setInteger(sender.indexOfSelectedItem, forKey: self.countryUserDefaultsKey)
		setCountry(sender.indexOfSelectedItem)
	}

	private func setCountry(countryID: Int) {
		var charactersROM: String?
		var kernelROM: String?
		switch countryID {
			case 0:	// Danish
				charactersROM = "characters-danish"
				kernelROM = "kernel-danish"
				vic20.region = .PAL
			case 1: // European
				charactersROM = "characters-english"
				kernelROM = "kernel-pal"
				vic20.region = .PAL
			case 2: // Japanese
				charactersROM = "characters-japanese"
				kernelROM = "kernel-japanese"
				vic20.region = .NTSC
			case 3: // Swedish
				charactersROM = "characters-swedish"
				kernelROM = "kernel-swedish"
				vic20.region = .PAL
			case 4: // US
				charactersROM = "characters-english"
				kernelROM = "kernel-ntsc"
				vic20.region = .NTSC
			default: break
		}

		if let charactersROM = charactersROM, kernelROM = kernelROM {
			if let kernel = rom(kernelROM), basic = rom("basic"), characters = rom(charactersROM) {
				vic20.setKernelROM(kernel)
				vic20.setBASICROM(basic)
				vic20.setCharactersROM(characters)
			}
		}
	}

	// MARK: memory model selector
	@IBOutlet var memorySizeButton: NSPopUpButton?
	var memorySizeUserDefaultsKey: String {
		get { return prefixedUserDefaultsKey("memorySize") }
	}

	@IBAction func setMemorySize(sender: NSPopUpButton!) {
		var selectedSize: Int?
		switch sender.indexOfSelectedItem {
			case 0: selectedSize = 5
			case 1: selectedSize = 8
			case 2: selectedSize = 32
			default: break
		}
		if let selectedSize = selectedSize {
			NSUserDefaults.standardUserDefaults().setInteger(selectedSize, forKey: self.memorySizeUserDefaultsKey)
			setMemorySize(sender.indexOfSelectedItem)
		}
	}
	private func setMemorySize(sizeIndex: Int) {
		switch sizeIndex {
			case 2:		vic20.memorySize = .Size32Kb
			case 1:		vic20.memorySize = .Size8Kb
			default:	vic20.memorySize = .Size5Kb
		}
	}

	// MARK: option restoration
	override func establishStoredOptions() {
		super.establishStoredOptions()

		let standardUserDefaults = NSUserDefaults.standardUserDefaults()
		standardUserDefaults.registerDefaults([
			self.autoloadingUserDefaultsKey: true,
			self.memorySizeUserDefaultsKey: 5,
			self.countryUserDefaultsKey: 1
		])

		let loadAutomatically = standardUserDefaults.boolForKey(self.autoloadingUserDefaultsKey)
		vic20.shouldLoadAutomatically = loadAutomatically
		self.loadAutomaticallyButton?.state = loadAutomatically ? NSOnState : NSOffState

		let memorySize = standardUserDefaults.integerForKey(self.memorySizeUserDefaultsKey)
		var indexToSelect: Int?
		switch memorySize {
			case 32:	indexToSelect = 2
			case 8:		indexToSelect = 1
			default:	indexToSelect = 0
		}
		if let indexToSelect = indexToSelect {
			self.memorySizeButton?.selectItemAtIndex(indexToSelect)
			setMemorySize(indexToSelect)
		}

		let country = standardUserDefaults.integerForKey(self.countryUserDefaultsKey)
		setCountry(country)
		self.countryButton?.selectItemAtIndex(country)
	}
}
