//
//  Vic20OptionsPanel.swift
//  Clock Signal
//
//  Created by Thomas Harte on 03/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

class Vic20OptionsPanel: MachinePanel {
	var vic20: CSVic20! {
		get {
			return self.machine as! CSVic20
		}
	}

	// MARK: automatic loading tick box
	@IBOutlet var loadAutomaticallyButton: NSButton?
	var autoloadingUserDefaultsKey: String {
		get { return prefixedUserDefaultsKey("autoload") }
	}

	@IBAction func setShouldLoadAutomatically(_ sender: NSButton!) {
		let loadAutomatically = sender.state == NSOnState
		vic20.shouldLoadAutomatically = loadAutomatically
		UserDefaults.standard.set(loadAutomatically, forKey: self.autoloadingUserDefaultsKey)
	}

	// MARK: country selector
	@IBOutlet var countryButton: NSPopUpButton?
	var countryUserDefaultsKey: String {
		get { return prefixedUserDefaultsKey("country") }
	}

	@IBAction func setCountry(_ sender: NSPopUpButton!) {
		UserDefaults.standard.set(sender.indexOfSelectedItem, forKey: self.countryUserDefaultsKey)
		setCountry(sender.indexOfSelectedItem)
	}

	fileprivate func setCountry(_ countryID: Int) {
		switch countryID {
			case 0:	// Danish
				vic20.country = .danish
			case 1: // European
				vic20.country = .european
			case 2: // Japanese
				vic20.country = .japanese
			case 3: // Swedish
				vic20.country = .swedish
			case 4: // US
				vic20.country = .american
			default: break
		}
	}

	// MARK: memory model selector
	@IBOutlet var memorySizeButton: NSPopUpButton?
	var memorySizeUserDefaultsKey: String {
		get { return prefixedUserDefaultsKey("memorySize") }
	}

	@IBAction func setMemorySize(_ sender: NSPopUpButton!) {
		var selectedSize: Int?
		switch sender.indexOfSelectedItem {
			case 0: selectedSize = 5
			case 1: selectedSize = 8
			case 2: selectedSize = 32
			default: break
		}
		if let selectedSize = selectedSize {
			UserDefaults.standard.set(selectedSize, forKey: self.memorySizeUserDefaultsKey)
			setMemorySize(sender.indexOfSelectedItem)
		}
	}
	fileprivate func setMemorySize(_ sizeIndex: Int) {
		switch sizeIndex {
			case 2:		vic20.memorySize = .size32Kb
			case 1:		vic20.memorySize = .size8Kb
			default:	vic20.memorySize = .size5Kb
		}
	}

	// MARK: option restoration
	override func establishStoredOptions() {
		super.establishStoredOptions()

		let standardUserDefaults = UserDefaults.standard
		standardUserDefaults.register(defaults: [
			self.autoloadingUserDefaultsKey: true,
			self.memorySizeUserDefaultsKey: 5,
			self.countryUserDefaultsKey: 1
		])

		let loadAutomatically = standardUserDefaults.bool(forKey: self.autoloadingUserDefaultsKey)
		vic20.shouldLoadAutomatically = loadAutomatically
		self.loadAutomaticallyButton?.state = loadAutomatically ? NSOnState : NSOffState

		if !loadAutomatically {
			let memorySize = standardUserDefaults.integer(forKey: self.memorySizeUserDefaultsKey)
			var indexToSelect: Int?
			switch memorySize {
				case 32:	indexToSelect = 2
				case 8:		indexToSelect = 1
				default:	indexToSelect = 0
			}
			if let indexToSelect = indexToSelect {
				self.memorySizeButton?.selectItem(at: indexToSelect)
				setMemorySize(indexToSelect)
			}
		}

		// TODO: this should be part of the configuration
		let country = standardUserDefaults.integer(forKey: self.countryUserDefaultsKey)
		setCountry(country)
		self.countryButton?.selectItem(at: country)
	}
}
