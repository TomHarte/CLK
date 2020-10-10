//
//  WolfgangLorenzTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/07/2015.
//  Copyright 2015 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class WolfgangLorenzTests: XCTestCase, CSTestMachineTrapHandler {

	// MARK: - 6502 Tests

	func testStart()	{
		runTest(" start", processor: .processor6502)
	}
	func testLDA()	{
		runTest("lda", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testSTA()	{
		runTest("sta", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testLDX()	{
		runTest("ldx", suffixes: ["b", "z", "zy", "a", "ay"], processor: .processor6502)
	}
	func testSTX()	{
		runTest("stx", suffixes: ["z", "zy", "a"], processor: .processor6502)
	}
	func testLDY()	{
		runTest("ldy", suffixes: ["b", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testSTY()	{
		runTest("sty", suffixes: ["z", "zx", "a"], processor: .processor6502)
	}
	func testTransfers()	{
		testTransfers(processor: .processor6502)
	}
	func testStack()	{
		testStack(processor: .processor6502)
	}
	func testIncsAndDecs()	{
		testIncsAndDecs(processor: .processor6502)
	}
	func testASL()	{
		runTest("asl", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testLSR()	{
		runTest("lsr", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testROL()	{
		runTest("rol", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testROR()	{
		runTest("ror", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testAND()	{
		runTest("and", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testORA()	{
		runTest("ora", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testEOR()	{
		runTest("eor", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testFlagManipulation()	{
		testFlagManipulation(processor: .processor6502)
	}
	func testADC()	{
		runTest("adc", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testSBC()	{
		runTest("sbc", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testCompare()	{
		runTest("cmp", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
		runTest("cpx", suffixes: ["b", "z", "a"], processor: .processor6502)
		runTest("cpy", suffixes: ["b", "z", "a"], processor: .processor6502)
	}
	func testBIT()	{
		runTest("bit", suffixes: ["z", "a"], processor: .processor6502)
	}
	func testFlow()	{
		runTest("brkn", processor: .processor6502)
		runTest("rtin", processor: .processor6502)
		runTest("jsrw", processor: .processor6502)
		runTest("rtsn", processor: .processor6502)
		runTest("jmpw", processor: .processor6502)
		runTest("jmpi", processor: .processor6502)
	}
	func testBranch()	{
		runTest("beqr", processor: .processor6502)
		runTest("bner", processor: .processor6502)
		runTest("bmir", processor: .processor6502)
		runTest("bplr", processor: .processor6502)
		runTest("bcsr", processor: .processor6502)
		runTest("bccr", processor: .processor6502)
		runTest("bvsr", processor: .processor6502)
		runTest("bvcr", processor: .processor6502)
	}
	func testNOP()	{
		runTest("nop", suffixes: ["n", "b", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testASO()	{
		runTest("aso", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testRLA()	{
		runTest("rla", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testLSE()	{
		runTest("lse", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testRRA()	{
		runTest("rra", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testDCM()	{
		runTest("dcm", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testINS()	{
		runTest("ins", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testLAX()	{
		runTest("lax", suffixes: ["z", "zy", "a", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testAXS()	{
		runTest("axs", suffixes: ["z", "zy", "a", "ix"], processor: .processor6502)
	}
	func testALR()	{
		runTest("alrb", processor: .processor6502)
	}
	func testARR()	{
		runTest("arrb", processor: .processor6502)
	}
	func testSBX()	{
		runTest("sbxb", processor: .processor6502)
	}
	func testSHA()	{
		runTest("sha", suffixes: ["ay", "iy"], processor: .processor6502)
	}
	func testSHX()	{
		runTest("shxay", processor: .processor6502)
	}
	func testSHY()	{
		runTest("shyax", processor: .processor6502)
	}
	func testSHS()	{
		runTest("shsay", processor: .processor6502)
	}
	func testLXA()	{
		runTest("lxab", processor: .processor6502)
	}
	func testANE()	{
		runTest("aneb", processor: .processor6502)
	}
	func testANC()	{
		runTest("ancb", processor: .processor6502)
	}
	func testLAS()	{
		runTest("lasay", processor: .processor6502)
	}
	func testSBCB()	{
		runTest("sbcb(eb)", processor: .processor6502)
	}


	// MARK: - 65816 Tests

	func testStart65816()	{
		runTest(" start", processor: .processor65816)
	}
	func testLDA65816()	{
		runTest("lda", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor65816)
	}
	func testSTA65816()	{
		runTest("sta", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor65816)
	}
	func testLDX65816()	{
		runTest("ldx", suffixes: ["b", "z", "zy", "a", "ay"], processor: .processor65816)
	}
	func testSTX65816()	{
		runTest("stx", suffixes: ["z", "zy", "a"], processor: .processor65816)
	}
	func testLDY65816()	{
		runTest("ldy", suffixes: ["b", "z", "zx", "a", "ax"], processor: .processor65816)
	}
	func testSTY65816()	{
		runTest("sty", suffixes: ["z", "zx", "a"], processor: .processor65816)
	}
	func testTransfers65816()	{
		testTransfers(processor: .processor65816)
	}
	func testStack65816()	{
		testStack(processor: .processor65816)
	}
	func testIncsAndDecs65816() {
		testIncsAndDecs(processor: .processor65816)
	}
	func testASL65816()	{
		runTest("asl", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor65816)
	}
	func testLSR65816()	{
		runTest("lsr", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor65816)
	}
	func testROL65816()	{
		runTest("rol", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor65816)
	}
	func testROR65816()	{
		runTest("ror", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor65816)
	}
	func testAND65816()	{
		runTest("and", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor65816)
	}
	func testORA65816()	{
		runTest("ora", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor65816)
	}
	func testEOR65816()	{
		runTest("eor", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor65816)
	}
	func testFlagManipulation65816()	{
		testFlagManipulation(processor: .processor65816)
	}
	func testADC65816()	{
		runTest("adc", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor65816)
	}
	func testSBC65816()	{
		runTest("sbc", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor65816)
	}


	// MARK: - Collections

	func testTransfers(processor: CSTestMachine6502Processor)	{
		for test in ["taxn", "tayn", "txan", "tyan", "tsxn", "txsn"] {
			runTest(test, processor: processor)
		}
	}
	func testStack(processor: CSTestMachine6502Processor) {
		for test in ["phan", "plan", "phpn", "plpn"] {
			runTest(test, processor: processor)
		}
	}
	func testIncsAndDecs(processor: CSTestMachine6502Processor) {
		runTest("inxn", processor: processor)
		runTest("inyn", processor: processor)
		runTest("dexn", processor: processor)
		runTest("deyn", processor: processor)
		runTest("incz", processor: processor)
		runTest("inczx", processor: processor)
		runTest("inca", processor: processor)
		runTest("incax", processor: processor)
		runTest("decz", processor: processor)
		runTest("deczx", processor: processor)
		runTest("deca", processor: processor)
		runTest("decax", processor: processor)
	}
	func testFlagManipulation(processor: CSTestMachine6502Processor)	{
		for test in ["clcn", "secn", "cldn", "sedn", "clin", "sein", "clvn"] {
			runTest(test, processor: processor)
		}
	}


	// MARK: - Test Running

	fileprivate func runTest(_ name: String, suffixes: [String], processor: CSTestMachine6502Processor) {
		for suffix in suffixes {
			let testName = name + suffix
			runTest(testName, processor: processor)
		}
	}

	fileprivate var output: String = ""
	fileprivate func runTest(_ name: String, processor: CSTestMachine6502Processor) {
		var machine: CSTestMachine6502!

		if let filename = Bundle(for: type(of: self)).path(forResource: name, ofType: nil) {
			if let testData = try? Data(contentsOf: URL(fileURLWithPath: filename)) {

				machine = CSTestMachine6502(processor: processor)
				machine.trapHandler = self
				output = ""

				let dataPointer = (testData as NSData).bytes.bindMemory(to: UInt8.self, capacity: testData.count)
				let loadAddress = UInt16(dataPointer[0]) | (UInt16(dataPointer[1]) << 8)
				let contents = testData.subdata(in: 2..<(testData.count - 2))

				machine.setData(contents, atAddress: loadAddress)

				// Cf. http://www.softwolves.com/arkiv/cbm-hackers/7/7114.html for the steps being taken here.

				// Initialise memory locations as instructed.
				machine.setValue(0x00, forAddress: 0x0002)
				machine.setValue(0x00, forAddress: 0xa002)
				machine.setValue(0x80, forAddress: 0xa003)
				machine.setValue(0xff, forAddress: 0x01fe)
				machine.setValue(0x7f, forAddress: 0x01ff)
				machine.setValue(0x48, forAddress: 0xfffe)
				machine.setValue(0xff, forAddress: 0xffff)

				// Place the Commodore's default IRQ handler.
				let irqHandler: [UInt8] = [
					0x48, 0x8a, 0x48, 0x98, 0x48, 0xba, 0xbd, 0x04, 0x01,
					0x29, 0x10, 0xf0, 0x03, 0x6c, 0x16, 0x03, 0x6c, 0x14, 0x03
				]
				machine.setData(Data(irqHandler), atAddress: 0xff48)

				// Set a couple of trap addresses to capture test output.
				machine.addTrapAddress(0xffd2)	// print character
				machine.addTrapAddress(0xffe4)	// scan keyboard

				// Set a couple of test addresses that indicate failure.
				machine.addTrapAddress(0x8000)	// exit
				machine.addTrapAddress(0xa474)	// exit

				// Ensure that any of those addresses return control.
				machine.setValue(0x60, forAddress:0xffd2)	// 0x60 is RTS
				machine.setValue(0x60, forAddress:0xffe4)
				machine.setValue(0x60, forAddress:0x8000)
				machine.setValue(0x60, forAddress:0xa474)

				// Commodore's load routine resides at $e16f; this is used to spot the end of a test.
				machine.setData(Data([0x4c, 0x6f, 0xe1]), atAddress: 0xe16f)

				// Seed program entry.
				machine.setValue(0x0801, for: .programCounter)
				machine.setValue(0xfd, for: .stackPointer)
				machine.setValue(0x04, for: .flags)

				// For consistency when debugging; otherwise immaterial.
				machine.setValue(0x00, for: .A)
				machine.setValue(0x00, for: .X)
				machine.setValue(0x00, for: .Y)
			}
		}

		if machine == nil {
			NSException(name: NSExceptionName(rawValue: "Failed Test"), reason: "Couldn't load file \(name)", userInfo: nil).raise()
		}

		while machine.value(for: .lastOperationAddress) != 0xe16f && !machine.isJammed {
			machine.runForNumber(ofCycles: 1000)
		}

		if machine.isJammed {
			let hexAddress = String(format:"%04x", machine.value(for: .lastOperationAddress))
			NSException(name: NSExceptionName(rawValue: "Failed Test"), reason: "Processor jammed unexpectedly at \(hexAddress)", userInfo: nil).raise()
		}
	}

	func petsciiToString(_ string: String) -> String {
		let petsciiToCharCommon: [String] = [
			"?", "?", "?", "[RUN/STOP]", "?", "[WHT]", "?", "?", "[SHIFT DISABLE]", "[SHIFT ENABLE]", "?", "?", "?", "\r", "[TEXT MODE]", "?",
			"?", "\n", "[RVS ON]", "[HOME]", "[DEL]", "?", "?", "?", "?", "?", "?", "?", "[RED]", "[RIGHT]", "[GRN]", "[BLU]",

			" ", "!", "\"", "#", "$", "%", "&", "'", "(", ")", "*", "+", ",", "-", ".", "/",
			"0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "<", "=", ">", "?",
		];

		let petsciiToCharRegular: [String] = [
			"@", "a", "b", "c", "d", "e", "f", "g", "h", "i", "j", "k", "l", "m", "n", "o",
			"p", "q", "r", "s", "t", "u", "v", "w", "x", "y", "z", "[", "£", "]", "↑", "←",
		]
		let petsciiToCharInverse: [String] = [
			"@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
			"P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "[", "£", "]", "↑", "←",
		]

		var result: String = ""
		for character in string.utf16 {
			let charInt: Int = Int(character)

			var stringToAppend = ""

			if charInt&0x7f < petsciiToCharCommon.count {
				stringToAppend = petsciiToCharCommon[charInt&0x7f]
			} else {
				let lookupTable = (charInt > 0x80) ? petsciiToCharInverse : petsciiToCharRegular
				let lookupIndex = (charInt&0x7f) - petsciiToCharCommon.count
				if lookupIndex < lookupTable.count {
					stringToAppend = lookupTable[lookupIndex]
				} else {
					stringToAppend += "!"
				}
			}

			result += stringToAppend
		}

		return result
	}

	func testMachine(_ testMachine: CSTestMachine, didTrapAtAddress address: UInt16) {
		let testMachine6502 = testMachine as! CSTestMachine6502
		switch address {
			case 0xffd2:
				testMachine6502.setValue(0x00, forAddress: 0x030c)

				let character = testMachine6502.value(for: CSTestMachine6502Register.A)
				output.append(Character(UnicodeScalar(character)!))

			case 0xffe4:
				testMachine6502.setValue(0x3, for:CSTestMachine6502Register.A)

			case 0x8000, 0xa474:
				NSException(name: NSExceptionName(rawValue: "Failed test"), reason: petsciiToString(output), userInfo: nil).raise()

			case 0x0000:
				NSException(name: NSExceptionName(rawValue: "Failed test"), reason: "Execution hit 0000", userInfo: nil).raise()

			default:
				let hexAddress = String(format:"%04x", address)
				NSException(name: NSExceptionName(rawValue: "Failed Test"), reason: "Processor jammed unexpectedly at \(hexAddress)", userInfo: nil).raise()
		}
	}

}
