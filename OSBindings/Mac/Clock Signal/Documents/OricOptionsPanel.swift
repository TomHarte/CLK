//
//  OricOptionsPanel.swift
//  Clock Signal
//
//  Created by Thomas Harte on 19/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

class OricOptionsPanel: MachinePanel {
	var oric: CSOric! {
		get {
			return self.machine as! CSOric
		}
	}

	fileprivate let displayTypeUserDefaultsKey = "oric.displayType"

	@IBOutlet var displayTypeButton: NSPopUpButton?
	@IBAction func setDisplayType(_ sender: NSPopUpButton!) {
		oric.useCompositeOutput = (sender.indexOfSelectedItem == 1)
		UserDefaults.standard.set(sender.indexOfSelectedItem, forKey: self.displayTypeUserDefaultsKey)
	}

	override func establishStoredOptions() {
		super.establishStoredOptions()
		let standardUserDefaults = UserDefaults.standard
		standardUserDefaults.register(defaults: [
			displayTypeUserDefaultsKey: 0,
		])

		let displayType = standardUserDefaults.integer(forKey: self.displayTypeUserDefaultsKey)
		oric.useCompositeOutput = (displayType == 1)
		self.displayTypeButton?.selectItem(at: displayType)
	}
}
