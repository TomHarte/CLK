//
//  AppleIIOptionsPanel.swift
//  Clock Signal
//
//  Created by Thomas Harte on 07/06/2021.
//  Copyright 2021 Thomas Harte. All rights reserved.
//

class AppleIIController: MachineController {
	var appleII: CSAppleII! {
		get {
			return self.machine.appleII
		}
	}
	var squarePixelsUserDefaultsKey: String {
		return prefixedUserDefaultsKey("useSquarePixels")
	}

	@IBOutlet var squarePixelButton: NSButton!

	@IBAction func optionDidChange(_ sender: AnyObject!) {
		let useSquarePixels = squarePixelButton.state == .on
		appleII.useSquarePixels = useSquarePixels

		let standardUserDefaults = UserDefaults.standard
		standardUserDefaults.set(useSquarePixels, forKey: squarePixelsUserDefaultsKey)
	}

	override func establishStoredOptions() {
		super.establishStoredOptions()

		let standardUserDefaults = UserDefaults.standard
		let useSquarePixels = standardUserDefaults.bool(forKey: squarePixelsUserDefaultsKey)
		appleII.useSquarePixels = useSquarePixels
		squarePixelButton.state = useSquarePixels ? .on : .off
	}
}
