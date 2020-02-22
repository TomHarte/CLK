//
//  PatrikRakTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 22/02/2020.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class PatrikRakTests: XCTestCase, CSTestMachineTrapHandler {

	fileprivate var done = false
	fileprivate var output = ""

	private func runTest(_ name: String, targetOutput: String) {
		if let filename = Bundle(for: type(of: self)).path(forResource: name, ofType: "tap") {
			if let testData = try? Data(contentsOf: URL(fileURLWithPath: filename)) {

				// Do a minor parsing of the TAP file to find the final file.
				var dataPointer = 0
				var finalBlock = 0
				while dataPointer < testData.count {
					let blockSize = Int(testData[dataPointer]) + Int(testData[dataPointer+1]) << 8
					finalBlock = dataPointer + 2
					dataPointer += 2 + blockSize
				}

				assert(dataPointer == testData.count)

				// Create a machine.
				let machine = CSTestMachineZ80()

				// Copy everything from finalBlock+1 to the end of the file to $8000.
				let fileContents = testData.subdata(in: finalBlock+1 ..< testData.count)
				machine.setData(fileContents, atAddress: 0x8000)

				// Add a RET and a trap at 10h, this is the Spectrum's system call for outputting text.
				machine.setValue(0xc9, atAddress: 0x0010)
				machine.addTrapAddress(0x0010);
				machine.trapHandler = self

				// Also add a RET at $1601, which is where the Spectrum puts 'channel open'.
				machine.setValue(0xc9, atAddress: 0x1601)

				// Add a call to $8000 and then an infinite loop; these tests load at $8000 and RET when done.
				machine.setValue(0xcd, atAddress: 0x7000)
				machine.setValue(0x00, atAddress: 0x7001)
				machine.setValue(0x80, atAddress: 0x7002)
				machine.setValue(0xc3, atAddress: 0x7003)
				machine.setValue(0x03, atAddress: 0x7004)
				machine.setValue(0x70, atAddress: 0x7005)
				machine.addTrapAddress(0x7003);

				// seed execution at 0x7000
				machine.setValue(0x7000, for: .programCounter)

				// run!
				let cyclesPerIteration: Int32 = 400_000_000
				while !done {
					machine.runForNumber(ofCycles: cyclesPerIteration)
				}

				XCTAssertEqual(targetOutput, output);
			}
		}
	}

	func testCCF() {
		let targetOutput = "???"
		runTest("z80ccf", targetOutput: targetOutput)
	}

	func testDoc() {
		let targetOutput = "???"
		runTest("z80doc", targetOutput: targetOutput)
	}

	func testDocFlags() {
		let targetOutput = "???"
		runTest("z80docflags", targetOutput: targetOutput)
	}

	func testFull() {
		let targetOutput = "???"
		runTest("z80full", targetOutput: targetOutput)
	}

	func testMemptr() {
		let targetOutput = "???"
		runTest("z80memptr", targetOutput: targetOutput)
	}

	func testMachine(_ testMachine: CSTestMachine, didTrapAtAddress address: UInt16) {
		let testMachineZ80 = testMachine as! CSTestMachineZ80
		switch address {
			case 0x0010:
				let textToAppend = UnicodeScalar(testMachineZ80.value(for: .A))!
				output += String(textToAppend)
				print(textToAppend, terminator:"")

			case 0x7003:
				done = true

			default:
				break
		}
	}
}
