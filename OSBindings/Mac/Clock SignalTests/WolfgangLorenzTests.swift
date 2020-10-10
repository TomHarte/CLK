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

	func testWolfgangLorenzStart65816()	{
		self.runWolfgangLorenzTest(" start", processor: .processor65816)
	}
	func testWolfgangLorenzLDA65816()	{
		self.runWolfgangLorenzTest("lda", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor65816)
	}

	func testWolfgangLorenzStart()	{
		self.runWolfgangLorenzTest(" start", processor: .processor6502)
	}
	func testWolfgangLorenzLDA()	{
		self.runWolfgangLorenzTest("lda", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzSTA()	{
		self.runWolfgangLorenzTest("sta", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzLDX()	{
		self.runWolfgangLorenzTest("ldx", suffixes: ["b", "z", "zy", "a", "ay"], processor: .processor6502)
	}
	func testWolfgangLorenzSTX()	{
		self.runWolfgangLorenzTest("stx", suffixes: ["z", "zy", "a"], processor: .processor6502)
	}
	func testWolfgangLorenzLDY()	{
		self.runWolfgangLorenzTest("ldy", suffixes: ["b", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testWolfgangLorenzSTY()	{
		self.runWolfgangLorenzTest("sty", suffixes: ["z", "zx", "a"], processor: .processor6502)
	}
	func testWolfgangLorenzTransfers()	{
		self.runWolfgangLorenzTest("taxn", processor: .processor6502)
		self.runWolfgangLorenzTest("tayn", processor: .processor6502)
		self.runWolfgangLorenzTest("txan", processor: .processor6502)
		self.runWolfgangLorenzTest("tyan", processor: .processor6502)
		self.runWolfgangLorenzTest("tsxn", processor: .processor6502)
		self.runWolfgangLorenzTest("txsn", processor: .processor6502)
	}
	func testWolfgangLorenzStack()	{
		self.runWolfgangLorenzTest("phan", processor: .processor6502)
		self.runWolfgangLorenzTest("plan", processor: .processor6502)
		self.runWolfgangLorenzTest("phpn", processor: .processor6502)
		self.runWolfgangLorenzTest("plpn", processor: .processor6502)
	}
	func testWolfgangLorenzIncsAndDecs()	{
		self.runWolfgangLorenzTest("inxn", processor: .processor6502)
		self.runWolfgangLorenzTest("inyn", processor: .processor6502)
		self.runWolfgangLorenzTest("dexn", processor: .processor6502)
		self.runWolfgangLorenzTest("deyn", processor: .processor6502)
		self.runWolfgangLorenzTest("incz", processor: .processor6502)
		self.runWolfgangLorenzTest("inczx", processor: .processor6502)
		self.runWolfgangLorenzTest("inca", processor: .processor6502)
		self.runWolfgangLorenzTest("incax", processor: .processor6502)
		self.runWolfgangLorenzTest("decz", processor: .processor6502)
		self.runWolfgangLorenzTest("deczx", processor: .processor6502)
		self.runWolfgangLorenzTest("deca", processor: .processor6502)
		self.runWolfgangLorenzTest("decax", processor: .processor6502)
	}
	func testWolfgangLorenzASL()	{
		self.runWolfgangLorenzTest("asl", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testWolfgangLorenzLSR()	{
		self.runWolfgangLorenzTest("lsr", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testWolfgangLorenzROL()	{
		self.runWolfgangLorenzTest("rol", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testWolfgangLorenzROR()	{
		self.runWolfgangLorenzTest("ror", suffixes: ["n", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testWolfgangLorenzAND()	{
		self.runWolfgangLorenzTest("and", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzORA()	{
		self.runWolfgangLorenzTest("ora", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzEOR()	{
		self.runWolfgangLorenzTest("eor", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzFlagManipulation()	{
		self.runWolfgangLorenzTest("clcn", processor: .processor6502)
		self.runWolfgangLorenzTest("secn", processor: .processor6502)
		self.runWolfgangLorenzTest("cldn", processor: .processor6502)
		self.runWolfgangLorenzTest("sedn", processor: .processor6502)
		self.runWolfgangLorenzTest("clin", processor: .processor6502)
		self.runWolfgangLorenzTest("sein", processor: .processor6502)
		self.runWolfgangLorenzTest("clvn", processor: .processor6502)
	}
	func testWolfgangLorenzADC()	{
		self.runWolfgangLorenzTest("adc", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzSBC()	{
		self.runWolfgangLorenzTest("sbc", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzCompare()	{
		self.runWolfgangLorenzTest("cmp", suffixes: ["b", "z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
		self.runWolfgangLorenzTest("cpx", suffixes: ["b", "z", "a"], processor: .processor6502)
		self.runWolfgangLorenzTest("cpy", suffixes: ["b", "z", "a"], processor: .processor6502)
	}
	func testWolfgangLorenzBIT()	{
		self.runWolfgangLorenzTest("bit", suffixes: ["z", "a"], processor: .processor6502)
	}
	func testWolfgangLorenzFlow()	{
		self.runWolfgangLorenzTest("brkn", processor: .processor6502)
		self.runWolfgangLorenzTest("rtin", processor: .processor6502)
		self.runWolfgangLorenzTest("jsrw", processor: .processor6502)
		self.runWolfgangLorenzTest("rtsn", processor: .processor6502)
		self.runWolfgangLorenzTest("jmpw", processor: .processor6502)
		self.runWolfgangLorenzTest("jmpi", processor: .processor6502)
	}
	func testWolfgangLorenzBranch()	{
		self.runWolfgangLorenzTest("beqr", processor: .processor6502)
		self.runWolfgangLorenzTest("bner", processor: .processor6502)
		self.runWolfgangLorenzTest("bmir", processor: .processor6502)
		self.runWolfgangLorenzTest("bplr", processor: .processor6502)
		self.runWolfgangLorenzTest("bcsr", processor: .processor6502)
		self.runWolfgangLorenzTest("bccr", processor: .processor6502)
		self.runWolfgangLorenzTest("bvsr", processor: .processor6502)
		self.runWolfgangLorenzTest("bvcr", processor: .processor6502)
	}
	func testWolfgangLorenzNOP()	{
		self.runWolfgangLorenzTest("nop", suffixes: ["n", "b", "z", "zx", "a", "ax"], processor: .processor6502)
	}
	func testWolfgangLorenzASO()	{
		self.runWolfgangLorenzTest("aso", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzRLA()	{
		self.runWolfgangLorenzTest("rla", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzLSE()	{
		self.runWolfgangLorenzTest("lse", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzRRA()	{
		self.runWolfgangLorenzTest("rra", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzDCM()	{
		self.runWolfgangLorenzTest("dcm", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzINS()	{
		self.runWolfgangLorenzTest("ins", suffixes: ["z", "zx", "a", "ax", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzLAX()	{
		self.runWolfgangLorenzTest("lax", suffixes: ["z", "zy", "a", "ay", "ix", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzAXS()	{
		self.runWolfgangLorenzTest("axs", suffixes: ["z", "zy", "a", "ix"], processor: .processor6502)
	}
	func testWolfgangLorenzALR()	{
		self.runWolfgangLorenzTest("alrb", processor: .processor6502)
	}
	func testWolfgangLorenzARR()	{
		self.runWolfgangLorenzTest("arrb", processor: .processor6502)
	}
	func testWolfgangLorenzSBX()	{
		self.runWolfgangLorenzTest("sbxb", processor: .processor6502)
	}
	func testWolfgangLorenzSHA()	{
		self.runWolfgangLorenzTest("sha", suffixes: ["ay", "iy"], processor: .processor6502)
	}
	func testWolfgangLorenzSHX()	{
		self.runWolfgangLorenzTest("shxay", processor: .processor6502)
	}
	func testWolfgangLorenzSHY()	{
		self.runWolfgangLorenzTest("shyax", processor: .processor6502)
	}
	func testWolfgangLorenzSHS()	{
		self.runWolfgangLorenzTest("shsay", processor: .processor6502)
	}
	func testWolfgangLorenzLXA()	{
		self.runWolfgangLorenzTest("lxab", processor: .processor6502)
	}
	func testWolfgangLorenzANE()	{
		self.runWolfgangLorenzTest("aneb", processor: .processor6502)
	}
	func testWolfgangLorenzANC()	{
		self.runWolfgangLorenzTest("ancb", processor: .processor6502)
	}
	func testWolfgangLorenzLAS()	{
		self.runWolfgangLorenzTest("lasay", processor: .processor6502)
	}
	func testWolfgangLorenzSBCB()	{
		self.runWolfgangLorenzTest("sbcb(eb)", processor: .processor6502)
	}

	fileprivate func runWolfgangLorenzTest(_ name: String, suffixes: [String], processor: CSTestMachine6502Processor) {
		for suffix in suffixes {
			let testName = name + suffix
			self.runWolfgangLorenzTest(testName, processor: processor)
		}
	}

	fileprivate var output: String = ""
	fileprivate func runWolfgangLorenzTest(_ name: String, processor: CSTestMachine6502Processor) {
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
				NSException(name: NSExceptionName(rawValue: "Failed test"), reason: self.petsciiToString(output), userInfo: nil).raise()

			case 0x0000:
				NSException(name: NSExceptionName(rawValue: "Failed test"), reason: "Execution hit 0000", userInfo: nil).raise()

			default:
				let hexAddress = String(format:"%04x", address)
				NSException(name: NSExceptionName(rawValue: "Failed Test"), reason: "Processor jammed unexpectedly at \(hexAddress)", userInfo: nil).raise()
		}
	}

}
