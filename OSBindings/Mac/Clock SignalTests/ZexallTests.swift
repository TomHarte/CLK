//
//  ZexallTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright © 2017 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class ZexallTests: XCTestCase, CSTestMachineTrapHandler {

	fileprivate var done = false
	fileprivate var output = ""

	func testZexall() {
		if let filename = Bundle(for: type(of: self)).path(forResource: "zexall", ofType: "com") {
			if let testData = try? Data(contentsOf: URL(fileURLWithPath: filename)) {

				// install test program, at the usual CP/M place
				let machine = CSTestMachineZ80()
				machine.setData(testData, atAddress: 0x0100)

				// add a RET at the CP/M entry location, and establish it as a trap location
				machine.setValue(0xc9, atAddress: 0x0005)
				machine.setValue(0xff, atAddress: 0x0006)
				machine.setValue(0xff, atAddress: 0x0007)
				machine.addTrapAddress(0x0005);
				machine.trapHandler = self

				// establish 0 as another trap location, as RST 0h is one of the ways that
				// CP/M programs can exit
				machine.addTrapAddress(0);

				// seed execution at 0x0100
				machine.setValue(0x0100, for: .programCounter)

				// run!
				while !done {
					machine.runForNumber(ofCycles: 200)
				}

				print("Done!")
				print(output)
			}
		}
	}

	func testMachine(_ testMachine: CSTestMachineZ80, didTrapAtAddress address: UInt16) {
		switch address {
			case 0x0005:
				let cRegister = testMachine.value(for: .C)
				switch cRegister {
					case 9:
						var address = testMachine.value(for: .DE)
						var character: Character = " "
						while true {
							character = Character(UnicodeScalar(testMachine.value(atAddress: address)))
							if character == "$" {
								break
							}
							output = output + String(character)
							address = address + 1
						}
					case 5:
						output += String(describing: UnicodeScalar(testMachine.value(for: .E)))
					case 0:
						done = true
					default:
						break
				}
				print(output)
			case 0x0000:
				done = true

			default:
				break
		}
	}
}
