//
//  MachineController.swift
//  Clock Signal
//
//  Created by Thomas Harte on 02/10/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

import Cocoa

class MachineController: NSObject {
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
		let useFastLoadingHack = sender.state == .on
		machine.useFastLoadingHack = useFastLoadingHack
		UserDefaults.standard.set(useFastLoadingHack, forKey: fastLoadingUserDefaultsKey)
	}

	// MARK: Quick Boot
	var bootQuicklyUserDefaultsKey: String {
		return prefixedUserDefaultsKey("bootQuickly")
	}
	@IBOutlet var fastBootingButton: NSButton?
	@IBAction func setFastBooting(_ sender: NSButton!) {
		let useQuickBootingHack = sender.state == .on
		machine.useQuickBootingHack = useQuickBootingHack
		UserDefaults.standard.set(useQuickBootingHack, forKey: bootQuicklyUserDefaultsKey)
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
			UserDefaults.standard.set(selectedItem.tag, forKey: displayTypeUserDefaultsKey)
		}
	}

	// MARK: Dynamic Cropping
	var dynamicCroppingUserDefaultsKey: String {
		return prefixedUserDefaultsKey("dynamicCrop")
	}
	@IBOutlet var dynamicCropButton: NSButton?
	@IBAction func setDynamicCrop(_ sender: NSButton!) {
		let useDynamicCropping = sender.state == .on
		machine.useDynamicCropping = useDynamicCropping
		UserDefaults.standard.set(useDynamicCropping, forKey: dynamicCroppingUserDefaultsKey)
	}

	// MARK: Restoring user defaults
	func establishStoredOptions() {
		let standardUserDefaults = UserDefaults.standard
		standardUserDefaults.register(defaults: [
			fastLoadingUserDefaultsKey: true,
			bootQuicklyUserDefaultsKey: true,
			displayTypeUserDefaultsKey: 0
		])

		if let fastLoadingButton = self.fastLoadingButton {
			let useFastLoadingHack = standardUserDefaults.bool(forKey: fastLoadingUserDefaultsKey)
			machine.useFastLoadingHack = useFastLoadingHack
			fastLoadingButton.state = useFastLoadingHack ? .on : .off
		}

		if let fastBootingButton = self.fastBootingButton {
			let bootQuickly = standardUserDefaults.bool(forKey: bootQuicklyUserDefaultsKey)
			machine.useQuickBootingHack = bootQuickly
			fastBootingButton.state = bootQuickly ? .on : .off
		}

		if let dynamicCropButton = self.dynamicCropButton {
			let useDynamicCropping = standardUserDefaults.bool(forKey: dynamicCroppingUserDefaultsKey)
			machine.useDynamicCropping = useDynamicCropping
			dynamicCropButton.state = useDynamicCropping ? .on : .off
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
