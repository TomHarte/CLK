//
//  MachinePanel.swift
//  Clock Signal
//
//  Created by Thomas Harte on 02/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

import Cocoa

class MachinePanel: NSPanel {
	var machine: CSMachine!

	// MARK: IBActions
	final func prefixedUserDefaultsKey(_ key: String) -> String {
		return "\(self.machine.userDefaultsPrefix).\(key)"
	}

	// MARK: Fast Loading
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

	// MARK: Quick Boot
	var bootQuicklyUserDefaultsKey: String {
		return prefixedUserDefaultsKey("bootQuickly")
	}
	@IBOutlet var fastBootingButton: NSButton?
	@IBAction func setFastBooting(_ sender: NSButton!) {
//		if let fastLoadingMachine = machine as? CSFastLoading {
//			let useFastLoadingHack = sender.state == .on
//			fastLoadingMachine.useFastLoadingHack = useFastLoadingHack
//			UserDefaults.standard.set(useFastLoadingHack, forKey: fastLoadingUserDefaultsKey)
//		}
	}

	// MARK: Display-Type Selection
	fileprivate func signalForTag(tag: Int) -> CSMachineVideoSignal {
		switch tag {
			case 1: return .composite
			case 2: return .sVideo
			case 3: return .monochromeComposite
			default: break
		}
		return .RGB
	}

	var displayTypeUserDefaultsKey: String {
		return prefixedUserDefaultsKey("displayType")
	}
	@IBOutlet var displayTypeButton: NSPopUpButton?
	@IBAction func setDisplayType(_ sender: NSPopUpButton!) {
		if let selectedItem = sender.selectedItem {
			machine.videoSignal = signalForTag(tag: selectedItem.tag)
			UserDefaults.standard.set(selectedItem.tag, forKey: self.displayTypeUserDefaultsKey)
		}
	}

	// MARK: Restoring user defaults
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


		if let displayTypeButton = self.displayTypeButton {
			// Enable or disable options as per machine support.
			var titlesToRemove: [String] = []
			for item in displayTypeButton.itemArray {
				if !machine.supportsVideoSignal(signalForTag(tag: item.tag)) {
					titlesToRemove.append(item.title)
				}
			}
			for title in titlesToRemove {
				displayTypeButton.removeItem(withTitle: title)
			}

			let displayType = standardUserDefaults.integer(forKey: self.displayTypeUserDefaultsKey)
			displayTypeButton.selectItem(withTag: displayType)
			setDisplayType(displayTypeButton)
		}
	}
}
