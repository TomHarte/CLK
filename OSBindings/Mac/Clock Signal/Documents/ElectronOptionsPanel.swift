//
//  ElectronOptionsPanel.swift
//  Clock Signal
//
//  Created by Thomas Harte on 02/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

class ElectronOptionsPanel: MachinePanel {
	var electron: CSElectron! {
		get {
			return self.machine as! CSElectron
		}
	}

	fileprivate let displayTypeUserDefaultsKey = "electron.displayType"

	@IBOutlet var displayTypeButton: NSPopUpButton?
	@IBAction func setDisplayType(_ sender: NSPopUpButton!) {
		electron.useCompositeOutput = (sender.indexOfSelectedItem == 1)
		UserDefaults.standard.set(sender.indexOfSelectedItem, forKey: self.displayTypeUserDefaultsKey)
	}

	override func establishStoredOptions() {
		super.establishStoredOptions()
		let standardUserDefaults = UserDefaults.standard
		standardUserDefaults.register(defaults: [
			displayTypeUserDefaultsKey: 0,
		])

		let displayType = standardUserDefaults.integer(forKey: self.displayTypeUserDefaultsKey)
		electron.useCompositeOutput = (displayType == 1)
		self.displayTypeButton?.selectItem(at: displayType)
	}
}
