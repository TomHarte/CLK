//
//  Atari2600Options.swift
//  Clock Signal
//
//  Created by Thomas Harte on 02/10/2016.
//  Copyright © 2016 Thomas Harte. All rights reserved.
//

class Atari2600OptionsPanel: MachinePanel {
	var atari2600: CSAtari2600! {
		get {
			return self.machine as! CSAtari2600
		}
	}

	@IBOutlet var resetButton: NSButton!
	@IBOutlet var selectButton: NSButton!
	@IBOutlet var colourButton: NSButton!
	@IBOutlet var leftPlayerDifficultyButton: NSButton!
	@IBOutlet var rightPlayerDifficultyButton: NSButton!

	@IBAction func optionDidChange(_ sender: AnyObject!) {
		pushSwitchValues()
	}

	fileprivate func pushSwitchValues() {
		atari2600.colourButton = colourButton.state == NSOnState
		atari2600.leftPlayerDifficultyButton = leftPlayerDifficultyButton.state == NSOnState
		atari2600.rightPlayerDifficultyButton = rightPlayerDifficultyButton.state == NSOnState
	}

	@IBAction func optionWasPressed(_ sender: NSButton!) {
		if sender == resetButton {
			atari2600.pressResetButton()
		} else {
			atari2600.pressSelectButton()
		}
	}
}
