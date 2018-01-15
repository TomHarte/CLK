//
//  MachinePanel.swift
//  Clock Signal
//
//  Created by Thomas Harte on 02/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

import Cocoa

class MachinePanel: NSPanel {
	var machine: CSMachine!

	// MARK: IBActions
	final func prefixedUserDefaultsKey(_ key: String) -> String {
		return "\(self.machine.userDefaultsPrefix).\(key)"
	}

	var fastLoadingUserDefaultsKey: String {
		return prefixedUserDefaultsKey("fastLoading")
	}
	@IBOutlet var fastLoadingButton: NSButton?
	@IBAction func setFastLoading(_ sender: NSButton!) {
		if let fastLoadingMachine = machine as? CSFastLoading {
			let useFastLoadingHack = sender.state == .on
			fastLoadingMachine.useFastLoadingHack = useFastLoadingHack
			UserDefaults.standard.set(useFastLoadingHack, forKey: fastLoadingUserDefaultsKey)
		}
	}

	var displayTypeUserDefaultsKey: String {
		return prefixedUserDefaultsKey("displayType")
	}
	@IBOutlet var displayTypeButton: NSPopUpButton?
	@IBAction func setDisplayType(_ sender: NSPopUpButton!) {
		machine.useCompositeOutput = (sender.indexOfSelectedItem == 1)
		UserDefaults.standard.set(sender.indexOfSelectedItem, forKey: self.displayTypeUserDefaultsKey)
	}

	func establishStoredOptions() {
		let standardUserDefaults = UserDefaults.standard
		standardUserDefaults.register(defaults: [
			fastLoadingUserDefaultsKey: true,
			displayTypeUserDefaultsKey: 0
		])

		if let fastLoadingMachine = machine as? CSFastLoading {
			let useFastLoadingHack = standardUserDefaults.bool(forKey: self.fastLoadingUserDefaultsKey)
			fastLoadingMachine.useFastLoadingHack = useFastLoadingHack
			self.fastLoadingButton?.state = useFastLoadingHack ? .on : .off
		}

		let displayType = standardUserDefaults.integer(forKey: self.displayTypeUserDefaultsKey)
		machine.useCompositeOutput = (displayType == 1)
		self.displayTypeButton?.selectItem(at: displayType)
	}
}
