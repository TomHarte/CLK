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
		get {
			return prefixedUserDefaultsKey("fastLoading")
		}
	}

	@IBOutlet var fastLoadingButton: NSButton?
	@IBAction func setFastLoading(_ sender: NSButton!) {
		if let fastLoadingMachine = machine as? CSFastLoading {
			let useFastLoadingHack = sender.state == NSOnState
			fastLoadingMachine.useFastLoadingHack = useFastLoadingHack
			UserDefaults.standard.set(useFastLoadingHack, forKey: fastLoadingUserDefaultsKey)
		}
	}

	func establishStoredOptions() {
		let standardUserDefaults = UserDefaults.standard
		standardUserDefaults.register(defaults: [
			fastLoadingUserDefaultsKey: true
		])

		if let fastLoadingMachine = machine as? CSFastLoading {
			let useFastLoadingHack = standardUserDefaults.bool(forKey: self.fastLoadingUserDefaultsKey)
			fastLoadingMachine.useFastLoadingHack = useFastLoadingHack
			self.fastLoadingButton?.state = useFastLoadingHack ? NSOnState : NSOffState
		}
	}
}
