//
//  ZexallTests.swift
//  Clock Signal
//
//  Created by Thomas Harte on 16/05/2017.
//  Copyright 2017 Thomas Harte. All rights reserved.
//

import XCTest
import Foundation

class ZexallTests: XCTestCase, CSTestMachineTrapHandler {

	fileprivate var done = false
	fileprivate var output = ""

	private func runTest(_ name: String) {
		if let filename = Bundle(for: type(of: self)).path(forResource: name, ofType: "com") {
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

				// ensure that if the CPU hits zero, it stays there until the end of the
				// sampling window
				machine.setValue(0xc3, atAddress: 0x0000)
				machine.setValue(0x00, atAddress: 0x0001)
				machine.setValue(0x00, atAddress: 0x0002)

				// seed execution at 0x0100
				machine.setValue(0x0100, for: .programCounter)

				// run!
				let cyclesPerIteration: Int32 = 400_000_000
				var cyclesToDate: TimeInterval = 0
				let startDate = Date()
				var printDate = Date()
				let printMhz = false
				while !done {
					machine.runForNumber(ofCycles: cyclesPerIteration)
					cyclesToDate += TimeInterval(cyclesPerIteration)
					if printMhz && printDate.timeIntervalSinceNow < -5.0 {
						print("\(cyclesToDate / -startDate.timeIntervalSinceNow) Mhz")
						printDate = Date()
					}
				}

				let targetOutput =
					"Z80doc instruction exerciser\n\r"			+
					"<adc,sbc> hl,<bc,de,hl,sp>....  OK\n\r"	+
					"add hl,<bc,de,hl,sp>..........  OK\n\r"	+
					"add ix,<bc,de,ix,sp>..........  OK\n\r"	+
					"add iy,<bc,de,iy,sp>..........  OK\n\r"	+
					"aluop a,nn....................  OK\n\r"	+
					"aluop a,<b,c,d,e,h,l,(hl),a>..  OK\n\r"	+
					"aluop a,<ixh,ixl,iyh,iyl>.....  OK\n\r"	+
					"aluop a,(<ix,iy>+1)...........  OK\n\r"	+
					"bit n,(<ix,iy>+1).............  OK\n\r"	+
					"bit n,<b,c,d,e,h,l,(hl),a>....  OK\n\r"	+
					"cpd<r>........................  OK\n\r"	+
					"cpi<r>........................  OK\n\r"	+
					"<daa,cpl,scf,ccf>.............  OK\n\r"	+
					"<inc,dec> a...................  OK\n\r"	+
					"<inc,dec> b...................  OK\n\r"	+
					"<inc,dec> bc..................  OK\n\r"	+
					"<inc,dec> c...................  OK\n\r"	+
					"<inc,dec> d...................  OK\n\r"	+
					"<inc,dec> de..................  OK\n\r"	+
					"<inc,dec> e...................  OK\n\r"	+
					"<inc,dec> h...................  OK\n\r"	+
					"<inc,dec> hl..................  OK\n\r"	+
					"<inc,dec> ix..................  OK\n\r"	+
					"<inc,dec> iy..................  OK\n\r"	+
					"<inc,dec> l...................  OK\n\r"	+
					"<inc,dec> (hl)................  OK\n\r"	+
					"<inc,dec> sp..................  OK\n\r"	+
					"<inc,dec> (<ix,iy>+1).........  OK\n\r"	+
					"<inc,dec> ixh.................  OK\n\r"	+
					"<inc,dec> ixl.................  OK\n\r"	+
					"<inc,dec> iyh.................  OK\n\r"	+
					"<inc,dec> iyl.................  OK\n\r"	+
					"ld <bc,de>,(nnnn).............  OK\n\r"	+
					"ld hl,(nnnn)..................  OK\n\r"	+
					"ld sp,(nnnn)..................  OK\n\r"	+
					"ld <ix,iy>,(nnnn).............  OK\n\r"	+
					"ld (nnnn),<bc,de>.............  OK\n\r"	+
					"ld (nnnn),hl..................  OK\n\r"	+
					"ld (nnnn),sp..................  OK\n\r"	+
					"ld (nnnn),<ix,iy>.............  OK\n\r"	+
					"ld <bc,de,hl,sp>,nnnn.........  OK\n\r"	+
					"ld <ix,iy>,nnnn...............  OK\n\r"	+
					"ld a,<(bc),(de)>..............  OK\n\r"	+
					"ld <b,c,d,e,h,l,(hl),a>,nn....  OK\n\r"	+
					"ld (<ix,iy>+1),nn.............  OK\n\r"	+
					"ld <b,c,d,e>,(<ix,iy>+1)......  OK\n\r"	+
					"ld <h,l>,(<ix,iy>+1)..........  OK\n\r"	+
					"ld a,(<ix,iy>+1)..............  OK\n\r"	+
					"ld <ixh,ixl,iyh,iyl>,nn.......  OK\n\r"	+
					"ld <bcdehla>,<bcdehla>........  OK\n\r"	+
					"ld <bcdexya>,<bcdexya>........  OK\n\r"	+
					"ld a,(nnnn) / ld (nnnn),a.....  OK\n\r"	+
					"ldd<r> (1)....................  OK\n\r"	+
					"ldd<r> (2)....................  OK\n\r"	+
					"ldi<r> (1)....................  OK\n\r"	+
					"ldi<r> (2)....................  OK\n\r"	+
					"neg...........................  OK\n\r"	+
					"<rrd,rld>.....................  OK\n\r"	+
					"<rlca,rrca,rla,rra>...........  OK\n\r"	+
					"shf/rot (<ix,iy>+1)...........  OK\n\r"	+
					"shf/rot <b,c,d,e,h,l,(hl),a>..  OK\n\r"	+
					"<set,res> n,<bcdehl(hl)a>.....  OK\n\r"	+
					"<set,res> n,(<ix,iy>+1).......  OK\n\r"	+
					"ld (<ix,iy>+1),<b,c,d,e>......  OK\n\r"	+
					"ld (<ix,iy>+1),<h,l>..........  OK\n\r"	+
					"ld (<ix,iy>+1),a..............  OK\n\r"	+
					"ld (<bc,de>),a................  OK\n\r"	+
					"Tests complete\n\r"
				XCTAssertEqual(targetOutput, output);
			}
		}
	}

	func testZexAll() {
		runTest("zexall")
	}

	func testZexDoc() {
		runTest("zexdoc")
	}

	func testMachine(_ testMachine: CSTestMachine, didTrapAtAddress address: UInt16) {
		let testMachineZ80 = testMachine as! CSTestMachineZ80
		switch address {
			case 0x0005:
				let cRegister = testMachineZ80.value(for: .C)
				var textToAppend = ""
				switch cRegister {
					case 9:
						var address = testMachineZ80.value(for: .DE)
						var character: Character = " "
						while true {
							character = Character(UnicodeScalar(testMachineZ80.value(atAddress: address)))
							if character == "$" {
								break
							}
							textToAppend += String(character)
							address = address + 1
						}
					case 5:
						textToAppend = String(describing: UnicodeScalar(testMachineZ80.value(for: .E)))
					case 0:
						done = true
					default:
						break
				}
				output += textToAppend
				print(textToAppend)

			case 0x0000:
				done = true

			default:
				break
		}
	}
}
