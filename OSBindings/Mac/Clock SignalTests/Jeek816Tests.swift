//
//  Jeek816Tests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 12/10/2020.
//  Copyright 2020 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class Jeek816Tests: XCTestCase {

	func testJeek816() {
		var machine: CSTestMachine6502!

		if let filename = Bundle(for: type(of: self)).path(forResource: "suite-a.prg", ofType: nil) {
			if let testData = try? Data(contentsOf: URL(fileURLWithPath: filename)) {
				machine = CSTestMachine6502(processor: .processor65816)

				let contents = testData.subdata(in: 0xe ..< testData.count)
				machine.setData(contents, atAddress: 0x080d)

				machine.setValue(0x080d, for: .programCounter)
			}
		}

		if machine == nil {
			NSException(name: NSExceptionName(rawValue: "Failed Test"), reason: "Couldn't load file \(name)", userInfo: nil).raise()
		}

		// $874 is the failure stopping point and $85d is success.
		while machine.value(for: .lastOperationAddress) != 0x0874 && machine.value(for: .lastOperationAddress) != 0x085d {
			machine.runForNumber(ofCycles: 1000)
		}

		// The test leaves $ff in $d7ff to indicate failure; $0000 to indicate success.
		// If the tests failed, it'll leave a bitmap of failures in address $0401.
		if machine.value(forAddress: 0xd7ff) != 0 {
			NSException(name: NSExceptionName(rawValue: "Failed Test"), reason: "Failed tests with bitmap: \(String(format:"%02x", machine.value(forAddress: 0x401)))", userInfo: nil).raise()
		}
	}

}
