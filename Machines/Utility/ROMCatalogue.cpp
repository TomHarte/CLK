//
//  ROMCatalogue.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 01/06/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#include "ROMCatalogue.hpp"

#include <algorithm>
#include <cassert>
#include <cstdlib>
#include <iomanip>
#include <locale>
#include <sstream>

using namespace ROM;

namespace {
constexpr Name MaxName = Name::SpectrumPlus3;

size_t operator ""_kb(const unsigned long long kilobytes) {
	return kilobytes * 1024;
}

size_t operator ""_mb(const unsigned long long megabytes) {
	return megabytes * 1024 * 1024;
}
}

const std::vector<Description> &Description::all_roms() {
	using enum Name;
	using CRCs = std::initializer_list<uint32_t>;
	using Files = std::initializer_list<std::string>;

	static const std::vector<Description> descriptions = {
	//
	// Acorn Archimedes.
	//
		{
			AcornArthur030,
			"Archimedes",
			"Arthur v0.30",
			"ROM030",
			512_kb,
			0x5df8ed42u
		},
		{
			AcornRISCOS200,
			"Archimedes",
			"RISC OS v2.00",
			"ROM200",
			512_kb,
			0x89c4ad36u
		},
		{
			AcornRISCOS311,
			"Archimedes",
			"RISC OS v3.11",
			"ROM311",
			2_mb,
			0x54c0c963u
		},
		{
			AcornRISCOS319,
			"Archimedes",
			"RISC OS v3.19",
			"ROM319",
			2_mb,
			0x00c7a3d3u
		},

	//
	// Acorn 8-bit.
	//
		{
			AcornBASICII,
			"Acorn",
			"the Acorn BASIC II ROM",
			Files{"basic.rom", "basic2.rom"},
			16_kb,
			0x79434781u
		},

	//
	// Acorn Electron.
	//
		{
			AcornBASICII,
			"Electron",
			"the Acorn BASIC II ROM",
			"basic.rom",
			16_kb,
			0x79434781u
		},
		{
			PRESADFSSlot1,
			"Electron",
			"the E00 ADFS ROM, first slot",
			"ADFS-E00_1.rom",
			16_kb,
			0x51523993u
		},
		{
			PRESADFSSlot2,
			"Electron",
			"the E00 ADFS ROM, second slot",
			"ADFS-E00_2.rom",
			16_kb,
			0x8d17de0eu
		},
		{
			AcornADFS,
			"Electron",
			"the Acorn ADFS ROM",
			"adfs.rom",
			16_kb,
			0x3289bdc6u
		},
		{
			Acorn1770DFS,
			"Electron",
			"the 1770 DFS ROM",
			"DFS-1770-2.20.rom",
			16_kb,
			0xf3dc9bc5u
		},
		{
			AcornIDEADFS103,
			"Electron",
			"the IDE-modified ADFS 1.03 ROM",
			"ELK130E.rom",
			16_kb,
			0xa923368du
		},
		{
			PRESAdvancedPlus6,
			"Electron",
			"the 8kb Advanced Plus 6 ROM",
			"AP6v133.rom",
			8_kb,
			0xe0013cfcu
		},
		{
			AcornElectronMOS100,
			"Electron",
			"the Electron MOS ROM v1.00",
			"os.rom",
			16_kb,
			0xbf63fb1fu
		},

	//
	// Amiga.
	//
		{
			AmigaKickstart10,
			"Amiga",
			"the Kickstart 1.0 ROM",
			"Kickstart-v1.0-1985-Commodore-A1000-NTSC.rom",
			256_kb,
			0x299790ffu
		},
		{
			AmigaKickstart11,
			"Amiga",
			"the Kickstart 1.1 ROM",
			"Kickstart-v1.1-rev31.34-1985-Commodore-A1000.NTSC.rom",
			256_kb,
			0xd060572au
		},
		{
			AmigaKickstart12,
			"Amiga",
			"the Kickstart 1.2 ROM",
			"Kickstart-v1.2-rev33.166-1986-Commodore-A1000.rom",
			256_kb,
			0x9ed783d0u
		},
		{
			AmigaA500Kickstart13,
			"Amiga",
			"the A500/A1000/A2000/CDTV Kickstart 1.3 ROM",
			"Kickstart-v1.3-rev34.5-1987-Commodore-A500-A1000-A2000-CDTV.rom",
			256_kb,
			0xc4f0f55fu
		},
		{
			AmigaA3000Kickstart13,
			"Amiga",
			"the A3000 Kickstart 1.3 ROM",
			"Kickstart-v1.3-rev34.5-1987-Commodore-A3000.rom",
			256_kb,
			0xe0f37258u
		},
		{
			AmigaKickstart20,
			"Amiga",
			"the Kickstart 2.0 ROM",
			"Kickstart-v2.0-rev36.143-1990-Commodore-A3000.rom",
			512_kb,
			0xb333d3c6u
		},
		{
			AmigaA500PlusKickstart204,
			"Amiga",
			"the A500+ Kickstart 2.04 ROM",
			"Kickstart-v2.04-rev37.175-1991-Commodore-A500plus.rom",
			512_kb,
			0xc3bdb240u
		},
		{
			AmigaA600Kickstart205,
			"Amiga",
			"the Kickstart 2.05 ROM",
			"Kickstart-v2.05-rev37.299-1991-Commodore-A600.rom",
			512_kb,
			0x83028fb5u
		},
		{
			AmigaA500Kickstart31,
			"Amiga",
			"the A500/A600/A2000 Kickstart 3.1 ROM",
			"Kickstart-v3.1-rev40.63-1993-Commodore-A500-A600-A2000.rom",
			512_kb,
			0xfc24ae0du
		},
		{
			AmigaDiagROM121,
			"Amiga",
			"DiagROM 1.2.1",
			"16bit.bin",
			512_kb,
			0xf2ac0a3b
		},

	//
	// Amstrad CPC.
	//
		{
			AMSDOS,
			"AmstradCPC",
			"the Amstrad Disk Operating System",
			"amsdos.rom",
			16_kb,
			0x1fe22ecdu
		},
		{
			CPC464Firmware,
			"AmstradCPC",
			"the CPC 464 firmware",
			"os464.rom",
			16_kb,
			0x815752dfu
		},
		{
			CPC464BASIC,
			"AmstradCPC",
			"the CPC 464 BASIC ROM",
			"basic464.rom",
			16_kb,
			0x7d9a3bacu
		},
		{
			CPC664Firmware,
			"AmstradCPC", "the CPC 664 firmware",
			"os664.rom",
			16_kb,
			0x3f5a6dc4u
		},
		{
			CPC664BASIC,
			"AmstradCPC",
			"the CPC 664 BASIC ROM",
			"basic664.rom",
			16_kb,
			0x32fee492u
		},
		{
			CPC6128Firmware,
			"AmstradCPC",
			"the CPC 6128 firmware",
			"os6128.rom",
			16_kb,
			0x0219bb74u
		},
		{
			CPC6128BASIC,
			"AmstradCPC",
			"the CPC 6128 BASIC ROM",
			"basic6128.rom",
			16_kb,
			0xca6af63du
		},

	//
	// Apple II.
	//
		{
			AppleIIEnhancedE,
			"AppleII",
			"the Enhanced Apple IIe ROM",
			"apple2e.rom",
			32_kb,
			0x65989942u
		},
		{
			AppleIIe,
			"AppleII",
			"the Apple IIe ROM",
			"apple2eu.rom",
			32_kb,
			0xe12be18du
		},
		{
			AppleIIPlus,
			"AppleII",
			"the Apple II+ ROM",
			"apple2.rom",
			12_kb,
			0xf66f9c26u
		},
		{
			AppleIIOriginal,
			"AppleII",
			"the original Apple II ROM",
			"apple2o.rom",
			12_kb,
			0xba210588u
		},
		{
			AppleIICharacter,
			"AppleII",
			"the basic Apple II character ROM",
			"apple2-character.rom",
			2_kb,
			0x64f415c6u
		},
		{
			AppleIIeCharacter,
			"AppleII",
			"the Apple IIe character ROM",
			"apple2eu-character.rom",
			4_kb,
			0x816a86f1u
		},
		{
			AppleIIEnhancedECharacter,
			"AppleII",
			"the Enhanced Apple IIe character ROM",
			"apple2e-character.rom",
			4_kb,
			0x2651014du
		},
		{
			AppleIISCSICard,
			"AppleII",
			"the Apple II SCSI card ROM",
			"scsi.rom",
			16_kb,
			0x5aff85d3u
		},

	//
	// Apple IIgs.
	//
		{
			AppleIIgsROM01,
			"AppleIIgs",
			"the Apple IIgs ROM01",
			"apple2gs.rom",
			128_kb,
			0x42f124b0u
		},
		{
			AppleIIgsROM03,
			"AppleIIgs",
			"the Apple IIgs ROM03",
			"apple2gs.rom2",
			256_kb,
			0xde7ddf29u
		},
		{
			AppleIIgsCharacter,
			"AppleIIgs",
			"the Apple IIgs character ROM",
			"apple2gs.chr",
			4_kb,
			0x91e53cd8u
		},
		{
			AppleIIgsMicrocontrollerROM03,
			"AppleIIgs",
			"the Apple IIgs ROM03 ADB microcontroller ROM",
			"341s0632-2",
			4_kb,
			0xe1c11fb0u
		},

	//
	// Atari ST
	//
		{
			AtariSTTOS100,
			"AtariST",
			"the UK TOS 1.00 ROM",
			"tos100.img",
			192_kb,
			0x1a586c64u
		},
		{
			AtariSTTOS104,
			"AtariST",
			"the UK TOS 1.04 ROM",
			"tos104.img",
			192_kb,
			0xa50d1d43u
		},

	//
	// BBC Micro.
	//
		{
			BBCMicroMOS12,
			"BBCMicro",
			"the BBC MOS v1.2",
			"os12.rom",
			16_kb,
			0x3c14fc70u
		},
		{
			BBCMicroDFS226,
			"BBCMicro",
			"the Acorn 1770 DFS 2.26 ROM",
			"dfs-2.26.rom",
			16_kb,
			0x5ae33e94u
		},
		{
			BBCMicroADFS130,
			"BBCMicro",
			"the Acorn ADFS 1.30 ROM",
			"adfs-1.30.rom",
			16_kb,
			0xd3855588u
		},
		{
			BBCMicroAdvancedDiscToolkit140,
			"BBCMicro",
			"the Advanced Disc Toolkit 1.40 ROM",
			"ADT-1.40.rom",
			16_kb,
			0x8314fed0u
		},
		{
			BBCMicroTube110,
			"BBCMicro",
			"the Tube 1.10 Boot ROM",
			"TUBE110.rom",
			2_kb,
			0x9ec2dbd0u
		},

	//
	// ColecoVision.
	//
		{
			ColecoVisionBIOS,
			"ColecoVision",
			"the ColecoVision BIOS",
			"coleco.rom",
			8_kb,
			0x3aa93ef3u
		},

	//
	// Commodore 1540/1541.
	//
		{
			Commodore1540,
			"Commodore1540",
			"the 1540 ROM",
			"1540.bin",
			16_kb,
			0x718d42b1u
		},
		{
			Commodore1541,
			"Commodore1540",
			"the 1541 ROM",
			"1541.bin",
			16_kb,
			0xfb760019
		},

	//
	// Disk II.
	//
		{
			DiskIIBoot16Sector,
			"DiskII",
			"the Disk II 16-sector boot ROM",
			"boot-16.rom",
			256,
			0xce7144f6u
		},
		{
			DiskIIStateMachine16Sector,
			"DiskII",
			"the Disk II 16-sector state machine ROM",
			"state-machine-16.rom",
			256,
			CRCs{ 0x9796a238, 0xb72a2c70 }
		},
		{
			DiskIIBoot13Sector,
			"DiskII",
			"the Disk II 13-sector boot ROM",
			"boot-13.rom",
			256,
			0xd34eb2ffu
		},
		{
			DiskIIStateMachine13Sector,
			"DiskII",
			"the Disk II 13-sector state machine ROM",
			"state-machine-13.rom",
			256,
			0x62e22620u
		},

	//
	// Enterprise.
	//
		{
			EnterpriseEXOS10,
			"Enterprise",
			"the Enterprise EXOS ROM v1.0",
			Files{
				"exos10.bin",
				"Exos (198x)(Enterprise).bin"
			},
			32_kb,
			0x30b26387u
		},
		{
			EnterpriseEXOS20,
			"Enterprise",
			"the Enterprise EXOS ROM v2.0",
			Files{
				"exos20.bin",
				"Expandible OS v2.0 (1984)(Intelligent Software).bin"
			},
			32_kb,
			0xd421795fu
		},
		{
			EnterpriseEXOS21,
			"Enterprise",
			"the Enterprise EXOS ROM v2.1",
			Files{
				"exos21.bin",
				"Expandible OS v2.1 (1985)(Intelligent Software).bin"
			},
			32_kb,
			0x982a3b44u
		},
		{
			EnterpriseEXOS23,
			"Enterprise",
			"the Enterprise EXOS ROM v2.3",
			Files{
				"exos23.bin",
				"Expandible OS v2.3 (1987)(Intelligent Software).bin"
			},
			64_kb,
			0x24838410u
		},

		{
			EnterpriseBASIC10,
			"Enterprise",
			"the Enterprise BASIC ROM v1.0",
			"basic10.bin",
			16_kb,
			0xd62e4fb7u
		},
		{
			EnterpriseBASIC10Part1,
			"Enterprise",
			"the Enterprise BASIC ROM v1.0, Part 1",
			"BASIC 1.0 - EPROM 1-2 (198x)(Enterprise).bin",
			8_kb + 1,
			0x37bf48e1u
		},
		{
			EnterpriseBASIC10Part2,
			"Enterprise",
			"the Enterprise BASIC ROM v1.0, Part 2",
			"BASIC 1.0 - EPROM 2-2 (198x)(Enterprise).bin",
			8_kb + 1,
			0xc5298c79u
		},
		{
			EnterpriseBASIC11,
			"Enterprise",
			"the Enterprise BASIC ROM v1.1",
			"basic11.bin",
			16_kb,
			0x683cf455u
		},
		{
			EnterpriseBASIC11Suffixed,
			"Enterprise",
			"the Enterprise BASIC ROM v1.1, with trailing byte",
			"BASIC 1.1 - EPROM 1.1 (198x)(Enterprise).bin",
			16_kb + 1,
			0xc96b7602u
		},
		{
			EnterpriseBASIC21,
			"Enterprise",
			"the Enterprise BASIC ROM v2.1",
			Files{
				"basic21.bin",
				"BASIC Interpreter v2.1 (1985)(Intelligent Software).bin",
				"BASIC Interpreter v2.1 (1985)(Intelligent Software)[a].bin"
			},
			16_kb,
			CRCs{ 0x55f96251, 0x683cf455 }
		},

		{
			EnterpriseEPDOS,
			"Enterprise",
			"the Enterprise EPDOS ROM",
			Files{
				"epdos.bin",
				"EPDOS v1.7 (19xx)(Haluska, Laszlo).bin"
			},
			32_kb,
			0x201319ebu
		},
		{
			EnterpriseEXDOS,
			"Enterprise",
			"the Enterprise EXDOS ROM",
			Files{
				"exdos.bin",
				"EX-DOS EPROM (198x)(Enterprise).bin"
			},
			16_kb,
			0xe6daa0e9u
		},

	//
	// Macintosh
	//
		{
			Macintosh128k,
			"Macintosh",
			"the Macintosh 128k ROM",
			"mac128k.rom",
			64_kb,
			0x6d0c8a28u
		},
		{
			Macintosh512k,
			"Macintosh",
			"the Macintosh 512k ROM",
			"mac512k.rom",
			64_kb,
			0xcf759e0du
		},
		{
			MacintoshPlus,
			"Macintosh",
			"the Macintosh Plus ROM",
			"macplus.rom",
			128_kb,
			CRCs{
				0x4fa5b399,
				0x7cacd18f,
				0xb2102e8e
			}
		},

	//
	// Master System.
	//
		{
			MasterSystemJapaneseBIOS,
			"MasterSystem",
			"the Japanese Master System BIOS",
			"japanese-bios.sms",
			8_kb,
			0x48d44a13u
		},
		{
			MasterSystemWesternBIOS,
			"MasterSystem",
			"the European/US Master System BIOS",
			"bios.sms",
			8_kb,
			0x0072ed54u
		},

	//
	// MSX.
	//
	// TODO: MSX CRCs below are incomplete, at best.
		{
			MSXGenericBIOS,
			"MSX",
			"a generix MSX BIOS",
			"msx.rom",
			32_kb,
			0x94ee12f3u
		},
		{
			MSXJapaneseBIOS,
			"MSX",
			"a Japanese MSX BIOS",
			"msx-japanese.rom",
			32_kb,
			0xee229390u
		},
		{
			MSXAmericanBIOS,
			"MSX",
			"an American MSX BIOS",
			"msx-american.rom",
			32_kb,
			0u
		},
		{
			MSXEuropeanBIOS,
			"MSX",
			"a European MSX BIOS",
			"msx-european.rom",
			32_kb,
			0u
		},
		{
			MSXDOS,
			"MSX",
			"the MSX-DOS ROM",
			"disk.rom",
			16_kb,
			0x721f61dfu
		},

		{
			MSX2GenericBIOS,
			"MSX",
			"a generic MSX2 BIOS",
			"msx2.rom",
			32_kb,
			0x6cdaf3a5u
		},
		{
			MSX2Extension,
			"MSX",
			"the MSX2 extension ROM",
			"msx2ext.rom",
			16_kb,
			0x66237ecfu
		},
		{
			MSXMusic,
			"MSX",
			"the MSX-MUSIC / FM-PAC ROM",
			"fmpac.rom",
			64_kb,
			0x0e84505du
		},

	//
	// Oric.
	//
		{
			OricColourROM,
			"Oric",
			"the Oric colour ROM",
			"colour.rom",
			128,
			0xd50fca65u
		},
		{
			OricBASIC10,
			"Oric",
			"Oric BASIC 1.0",
			"basic10.rom",
			16_kb,
			0xf18710b4u
		},
		{
			OricBASIC11,
			"Oric",
			"Oric BASIC 1.1",
			"basic11.rom",
			16_kb,
			0xc3a92befu
		},
		{
			OricPravetzBASIC,
			"Oric",
			"Pravetz BASIC",
			"pravetz.rom",
			16_kb,
			0x58079502u
		},
		{
			OricByteDrive500,
			"Oric",
			"the Oric Byte Drive 500 ROM",
			"bd500.rom",
			8_kb,
			0x61952e34u
		},
		{
			OricJasmin,
			"Oric",
			"the Oric Jasmin ROM",
			"jasmin.rom",
			2_kb,
			0x37220e89u
		},
		{
			OricMicrodisc,
			"Oric",
			"the Oric Microdisc ROM",
			"microdisc.rom",
			8_kb,
			0xa9664a9cu
		},
		{
			Oric8DOSBoot,
			"Oric",
			"the 8DOS boot ROM",
			"8dos.rom",
			512,
			0x49a74c06u
		},

	//
	// PC Compatible.
	//
		{
			PCCompatibleGLaBIOS,
			"PCCompatible",
			"8088 GLaBIOS 0.2.5",
			"GLABIOS_0.2.5_8T.ROM",
			8_kb,
			0x9576944cu
		},
		{
			PCCompatibleGLaTICK,
			"PCCompatible",
			"AT GLaTICK 0.8.5",
			"GLaTICK_0.8.5_AT.ROM",
			2_kb,
			0x371ea3f1u
		},
		{
			PCCompatiblePhoenix80286BIOS,
			"PCCompatible",
			"Phoenix 80286 BIOS 3.05",
			"Phoenix 80286 ROM BIOS Version 3.05.bin",
			32_kb,
			0x8d0d318au
		},
		{
			PCCompatibleIBMATBIOS,
			"PCCompatible",
			"IBM PC AT BIOS v3",
			"at-bios.bin",
			64_kb,
			0x674426beu
		},
		{
			PCCompatibleIBMATBIOSNov85U27,
			"PCCompatible",
			"IBM PC AT BIOS; 15th Nov 1985; U27",
			"BIOS_5170_15NOV85_U27_61X9266_27256.BIN",
			32_kb,
			0x4995be7au
		},
		{
			PCCompatibleIBMATBIOSNov85U47,
			"PCCompatible",
			"IBM PC AT BIOS; 15th Nov 1985; U47",
			"BIOS_5170_15NOV85_U47_61X9265_27256.BIN",
			32_kb,
			0xc32713e4u
		},
		{
			PCCompatibleCGAFont,
			"PCCompatible",
			"IBM's CGA font",
			"CGA.F08",
			8 * 256,
			0xa362ffe6u
		},
		{
			PCCompatibleMDAFont,
			"PCCompatible",
			"IBM's MDA font",
			"EUMDA9.F14",
			14 * 256,
			0x7754882au
		},
		{
			PCCompatibleEGABIOS,
			"PCCompatible",
			"IBM's EGA BIOS",
			"ibm_6277356_ega_card_u44_27128.bin",
			16_kb,
			0x2f2fbc40u
		},
		{
			PCCompatibleVGABIOS,
			"PCCompatible",
			"IBM's VGA BIOS",
			"ibm_vga.bin",
			32_kb,
			0x03b3f90du
		},
		{
			IBMBASIC110,
			"PCCompatible",
			"IBM ROM BASIC 1.10",
			"ibm-basic-1.10.rom",
			32_kb,
			0xebacb791u
		},

	//
	// Plus 4.
	//
		{
			Plus4KernelPALv3,
			"Plus4",
			"the C16+4 kernel, PAL-G revision 3",
			"kernal.318004-03.bin",
			16_kb,
			0x77bab934u
		},
		{
			Plus4KernelPALv4,
			"Plus4",
			"the C16+4 kernel, PAL-G revision 4",
			"kernal.318004-04.bin",
			16_kb,
			0xbe54ed79u
		},
		{
			Plus4KernelPALv5,
			"Plus4",
			"the C16+4 kernel, PAL-G revision 5",
			"kernal.318004-05.bin",
			16_kb,
			0x71c07bd4u
		},
		{
			Plus4BASIC,
			"Plus4",
			"the C16+4 BASIC ROM",
			"basic.318006-01.bin",
			16_kb,
			0x74eaae87u
		},

	//
	// Sinclair QL.
	//
		{
			SinclairQLJS,
			"SinclairQL",
			"the Sinclair QL 'JS' ROM",
			"js.rom",
			48_kb,
			0x0f95aab5u
		},

	//
	// Vic-20.
	//
		{
			Vic20BASIC,
			"Vic20",
			"the VIC-20 BASIC ROM",
			"basic.bin",
			8_kb,
			0xdb4c43c1u
		},
		{
			Vic20EnglishCharacters,
			"Vic20",
			"the English-language VIC-20 character ROM",
			"characters-english.bin",
			4_kb,
			0x83e032a6u
		},
		{
			Vic20EnglishPALKernel,
			"Vic20",
			"the English-language PAL VIC-20 kernel ROM",
			"kernel-pal.bin",
			8_kb,
			0x4be07cb4u
		},
		{
			Vic20EnglishNTSCKernel,
			"Vic20",
			"the English-language NTSC VIC-20 kernel ROM",
			"kernel-ntsc.bin",
			8_kb,
			0xe5e7c174u
		},
		{
			Vic20DanishCharacters,
			"Vic20",
			"the Danish VIC-20 character ROM",
			"characters-danish.bin",
			4_kb,
			0x7fc11454u
		},
		{
			Vic20DanishKernel,
			"Vic20",
			"the Danish VIC-20 kernel ROM",
			"kernel-danish.bin",
			8_kb,
			0x02adaf16u
		},
		{
			Vic20JapaneseCharacters,
			"Vic20",
			"the Japanese VIC-20 character ROM",
			"characters-japanese.bin",
			4_kb,
			0xfcfd8a4bu
		},
		{
			Vic20JapaneseKernel,
			"Vic20",
			"the Japanese VIC-20 kernel ROM",
			"kernel-japanese.bin",
			8_kb,
			0x336900d7u
		},
		{
			Vic20SwedishCharacters,
			"Vic20",
			"the Swedish VIC-20 character ROM",
			"characters-swedish.bin",
			4_kb,
			0xd808551du
		},
		{
			Vic20SwedishKernel,
			"Vic20",
			"the Swedish VIC-20 kernel ROM",
			"kernel-swedish.bin",
			8_kb,
			0xb2a60662u
		},

	//
	// ZX Spectrum.
	//
		{
			Spectrum48k,
			"ZXSpectrum",
			"the 48kb ROM",
			"48.rom",
			16_kb,
			0xddee531fu
		},
		{
			Spectrum128k,
			"ZXSpectrum",
			"the 128kb ROM",
			"128.rom",
			32_kb,
			0x2cbe8995u
		},
		{
			SpecrumPlus2,
			"ZXSpectrum",
			"the +2 ROM",
			"plus2.rom",
			32_kb,
			0xe7a517dcu
		},
		{
			SpectrumPlus3,
			"ZXSpectrum",
			"the +2a/+3 ROM",
			"plus3.rom",
			64_kb,
			CRCs{
				0x96e3c17a,
				0xbe0d9ec4
			}
		},

	//
	// ZX80/81.
	//
		{
			ZX80,
			"ZX8081",
			"the ZX80 BASIC ROM",
			"zx80.rom",
			4_kb,
			0x4c7fc597u
		},
		{
			ZX81,
			"ZX8081",
			"the ZX81 BASIC ROM",
			"zx81.rom",
			8_kb,
			0x4b1dd6ebu
		},
	};

	return descriptions;
}

Request::Request(const Name name, const bool optional) {
	node.name = name;
	node.is_optional = optional;
}

Request Request::append(const Node::Type type, const Request &rhs) {
	// If either side is empty, act appropriately.
	if(node.empty() && !rhs.node.empty()) {
		return rhs;
	}
	if(rhs.node.empty()) {
		return *this;
	}

	// Just copy in the RHS child nodes if types match.
	if(node.type == type && rhs.node.type == type) {
		Request new_request = *this;
		new_request.node.children.insert(
			new_request.node.children.end(),
			rhs.node.children.begin(),
			rhs.node.children.end()
		);
		new_request.node.sort();
		return new_request;
	}

	// Possibly: left is appropriate request and rhs is just one more thing?
	if(node.type == type && rhs.node.type == Node::Type::One) {
		Request new_request = *this;
		new_request.node.children.push_back(rhs.node);
		new_request.node.sort();
		return new_request;
	}

	// Or: right is appropriate request and this is just one more thing?
	if(rhs.node.type == type && node.type == Node::Type::One) {
		Request new_request = rhs;
		new_request.node.children.push_back(node);
		new_request.node.sort();
		return new_request;
	}

	// Otherwise create a new parent node.
	Request parent;
	parent.node.type = type;
	parent.node.children.push_back(this->node);
	parent.node.children.push_back(rhs.node);
	return parent;
}

Request Request::operator &&(const Request &rhs) {
	return append(Node::Type::All, rhs);
}

Request Request::operator ||(const Request &rhs) {
	return append(Node::Type::Any, rhs);
}

bool Request::validate(Map &map) const {
	return node.validate(map);
}

std::vector<ROM::Description> Request::all_descriptions() const {
	std::vector<Description> result;
	node.add_descriptions(result);
	return result;
}

void Request::Node::add_descriptions(std::vector<Description> &result) const {
	if(type == Type::One) {
		result.push_back(name);
		return;
	}

	for(const auto &child: children) {
		child.add_descriptions(result);
	}
}

Request Request::subtract(const ROM::Map &map) const {
	Request copy(*this);
	if(copy.node.subtract(map)) {
		copy.node.name = Name::None;
		copy.node.type = Node::Type::One;
	}
	return copy;
}

bool Request::Node::subtract(const ROM::Map &map) {
	switch(type) {
		case Type::One:
		return map.find(name) != map.end();

		default: {
			bool has_all = true;
			bool has_any = false;

			auto iterator = children.begin();
			while(iterator != children.end()) {
				const bool did_subtract = iterator->subtract(map);
				has_all &= did_subtract;
				has_any |= did_subtract;
				if(did_subtract) {
					iterator = children.erase(iterator);
				} else {
					++iterator;
				}
			}

			return (type == Type::All && has_all) || (type == Type::Any && has_any);
		}
	}
}

bool Request::empty() {
	return node.type == Node::Type::One && node.name == Name::None;
}

bool Request::Node::validate(Map &map) const {
	// Leaf nodes are easy: check that the named ROM is present,
	// unless it's optional, in which case it is always valid.
	//
	// If it is present, make sure it's the proper size.
	if(type == Type::One) {
		auto rom = map.find(name);
		if(rom == map.end()) {
			return is_optional;
		}

		const Description description(name);
		rom->second.resize(description.size);

		return true;
	}

	// This is a collection node then. Check for both any or all
	// simultaneously, since all nodes will need to be visited
	// regardless of any/all in order to ensure proper sizing.
	bool has_all = true;
	bool has_any = false;

	for(const auto &child: children) {
		const bool is_valid = child.validate(map);
		has_all &= is_valid;
		has_any |= is_valid;
	}

	return (type == Type::Any && has_any) || (type == Type::All && has_all);
}

void Request::visit(
	const std::function<void(ListType, size_t)> &enter_list,
	const std::function<void(void)> &exit_list,
	const std::function<void(ROM::Request::ListType, const ROM::Description &, bool, size_t)> &add_item
) const {
	node.visit(enter_list, exit_list, add_item);
}

void Request::visit(
	const std::function<void(LineItem, ListType, int, const ROM::Description *, bool, size_t)> &add_item
) const {
	int indentation_level = 0;
	node.visit(
		[&indentation_level, &add_item] (ROM::Request::ListType type, size_t size) {
			add_item(LineItem::NewList, type, indentation_level, nullptr, false, size);
			++indentation_level;
		},
		[&indentation_level] {
			--indentation_level;
		},
		[&indentation_level, &add_item] (
			const ROM::Request::ListType type,
			const ROM::Description &rom,
			const bool is_optional,
			const size_t remaining
		) {
			add_item(LineItem::Description, type, indentation_level, &rom, is_optional, remaining);
		}
	);
}

void Request::Node::visit(
	const std::function<void(ListType, size_t)> &enter_list,
	const std::function<void(void)> &exit_list,
	const std::function<
		void(
			ROM::Request::ListType type,
			const ROM::Description &,
			bool is_optional,
			size_t remaining
		)
	> &add_item
) const {
	switch(type) {
		case Type::One:
			enter_list(ListType::Single, 1);
			add_item(ROM::Request::ListType::Any, Description(name), is_optional, 0);
			exit_list();
		break;

		case Type::Any:
		case Type::All: {
			const ListType list_type = type == Type::Any ? ListType::Any : ListType::All;
			enter_list(list_type, children.size());
			for(size_t index = 0; index < children.size(); index++) {
				auto &child = children[index];

				if(child.type == Type::One) {
					add_item(list_type, Description(child.name), child.is_optional, children.size() - 1 - index);
				} else {
					child.visit(enter_list, exit_list, add_item);
				}
			}
			exit_list();
		} break;
	}
}

std::vector<Description> ROM::all_descriptions() {
	std::vector<Description> result;
	for(int name = 1; name <= MaxName; name++) {
		result.push_back(Description(ROM::Name(name)));
	}
	return result;
}

std::string Description::description(int flags) const {
	std::stringstream output;

	// If there are no CRCs, don't output them.
	if(crc32s.empty()) flags &= ~DescriptionFlag::CRC;

	// Print the file name(s) and the descriptive name.
	if(flags & DescriptionFlag::Filename) {
		flags &= ~DescriptionFlag::Filename;

		output << machine_name << '/';
		if(file_names.size() == 1) {
			output << file_names[0];
		} else {
			output << "{";
			bool is_first = true;
			for(const auto &file_name: file_names) {
				if(!is_first) output << " or ";
				output << file_name;
				is_first = false;
			}
			output << "}";
		}
		output << " (" << descriptive_name;
		if(!flags) {
			output << ")";
			return output.str();
		}
		output << "; ";
	} else {
		output << descriptive_name;
		if(!flags) {
			return output.str();
		}
		output << " (";
	}

	// Print the size.
	if(flags & DescriptionFlag::Size) {
		flags &= ~DescriptionFlag::Size;
		output << size << " bytes";

		if(!flags) {
			output << ")";
			return output.str();
		}
		output << "; ";
	}

	// Print the CRC(s).
	if(flags & DescriptionFlag::CRC) {
		flags &= ~DescriptionFlag::CRC;

		output << ((crc32s.size() > 1) ? "usual crc32s: " : "usual crc32: ");
		bool is_first = true;
		for(const auto crc32: crc32s) {
			if(!is_first) output << ", ";
			is_first = false;
			output << std::hex << std::setfill('0') << std::setw(8) << crc32;
		}

		if(!flags) {
			output << ")";
			return output.str();
		}
	}

	return output.str();
}

std::wstring Request::description(const int description_flags, const wchar_t bullet_point) {
	std::wstringstream output;

	visit(
		[&output, description_flags, bullet_point] (
			const ROM::Request::LineItem item,
			const ROM::Request::ListType type,
			const int indentation_level,
			const ROM::Description *const description,
			const bool is_optional,
			const size_t remaining
		) {
			if(indentation_level) {
				output << std::endl;
				for(int c = 0; c < indentation_level; c++) output << '\t';
				output << bullet_point << ' ';
			}

			switch(item) {
				case ROM::Request::LineItem::NewList:
					if(remaining > 1) {
						if(!indentation_level) output << " ";
						switch(type) {
							default:
							case ROM::Request::ListType::All:	output << "all of:";		break;
							case ROM::Request::ListType::Any:
								if(remaining == 2) {
									output << "either of:";
								} else {
									output << "any of:";
								}
							break;
						}
					} else {
						output << ":";
					}
				break;

				case ROM::Request::LineItem::Description: {
					if(is_optional) output << "optionally, ";

					const auto text = description->description(description_flags);
					std::wstring wide_text(text.size(), L' ');
					std::mbstowcs(wide_text.data(), text.data(), text.size());
					output << wide_text;

					if(remaining) {
						output << ";";
						if(remaining == 1) {
							output << ((type == ROM::Request::ListType::All) ? " and" : " or");
						}
					} else {
						output << ".";
					}
				} break;
			}
		}
	);

	return output.str();
}

std::optional<Description> Description::from_crc(const uint32_t crc32) {
	const auto all = all_roms();
	const auto rom = std::find_if(all.begin(), all.end(), [&](const Description &candidate) {
		const auto found_crc = std::find(candidate.crc32s.begin(), candidate.crc32s.end(), crc32);
		return found_crc != candidate.crc32s.end();
	});

	if(rom != all.end()) {
		return *rom;
	}
	return std::nullopt;
}

Description::Description(const Name name) {
	const auto all = all_roms();
	const auto rom = std::find_if(all.begin(), all.end(), [&](const Description &candidate) {
		return candidate.name == name;
	});
	if(rom != all.end()) {
		*this = *rom;
	}
}
