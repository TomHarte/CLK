//
//  EXOSCodes.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

// Various EXOS codes, transcribed from	EXOS20_technical_information.pdf via archive.org,
// which appears to be a compilation of original documentation so page numbers below
// refer to the page within the PDF. Numbers printed on the in-document pages are inconsistent.

namespace Enterprise::EXOS {

// Page 67.
enum class Function: uint8_t {
	ResetSystem = 0,				// RESET
	OpenChannel = 1,				// OPEN
	CreateChannel = 2,				// CREAT
	CloseChannel = 3,				// CLOSE
	DestroyChannel = 4,				// DEST
	ReadCharacter = 5,				// RDCH
	ReadBlock = 6,					// RDBLK
	WriteCharacter = 7,				// WRCH
	WriteBlock = 8,					// WRBLK
	ReadChannelStatus = 9,			// RSTAT
	SetChannelStatus = 10,			// SSTAT
	SpecialFunction = 11,			// SFUNC

	SetReadToggleEXOSVariable = 16,	// EVAR
	CaptureChannel = 17,			// CAPT
	RedirectChannel = 18,			// REDIR
	SetDefaultDevice = 19,			// DDEV
	ReturnSystemStatus = 20,		// SYSS
	LinkDevices = 21,				// LINK
	ReadEXOSBoundary = 22,			// READB
	SetUSERBoundary = 23,			// SETB,
	AllocateSegment = 24,			// ALLOC,
	FreeSegment = 25,				// FREE
	LocateROMs = 26,				// ROMS
	AllocateChannelBuffer = 27,		// BUFF
	ReturnErrorMessage = 28,		// ERRMSG
};

// Page 25.
enum class DeviceDescriptorFunction: uint8_t {
	//
	// Codes are the same as `Function` in the range 1–11.
	//
	Interrupt = 0,
	Initialise = 12,
	BufferMoved = 13,
};

enum class Error: uint8_t {
	NoError = 0x00,

	//
	// General Kernel Errors.
	//
	InvalidFunctionCode = 0xff,				// IFUNC
	FunctionCallNotAllowed = 0xfe,			// ILLFN
	InvalidString = 0xfd,					// INAME
	InsufficientStack = 0xfc,				// STACK
	ChannelIllegalOrDoesNotExist = 0xfb,	// ICHAN
	DeviceDoesNotExist = 0xfa,				// NODEV
	ChannelAlreadyExists = 0xf9,			// CHANX
	NoAllocateBufferCallMade = 0xf8,		// NOBUF
	InsufficientRAMForBuffer = 0xf7,		// NORAM
	InsufficientVideoRAM = 0xf6,			// NOVID
	NoFreeSegments = 0xf5,					// NOSEG
	InvalidSegment = 0xf4,					// ISEG
	InvalidUserBoundary = 0xf3,				// IBOUND
	InvalidEXOSVariableNumber = 0xf2,		// IVAR
	InvalidDesviceDescriptorType = 0xf1,	// IDESC
	UnrecognisedCommandString = 0xf0,		// NOSTR
	InvalidFileHeader = 0xef,				// ASCII
	UnknownModuleType = 0xee,				// ITYPE
	InvalidRelocatableModule = 0xed,		// IREL
	NoModule = 0xec,						// NOMOD
	InvalidTimeOrDateValue,					// ITIME

	//
	// General Device Errors.
	//
	InvalidSpecialFunctionCode = 0xea,		// ISPEC
	AttemptToOpenSecondChannel = 0xe9,		// 2NDCH
	InvalidUnitNumber = 0xe8,				// IUNIT
	FunctionNotSupported = 0xe7,			// NOFN
	InvalidEscapeSequence = 0xe6,			// ESC
	StopKeyPressed = 0xe5,					// STOP
	EndOfFileMetInRead = 0xe4,				// EOF
	ProtectionViolation = 0xe3,				// PROT

	//
	// Device-Specific Errors.
	//
//	FileDoesNotExist = 0xea,				// NOFIL
//	FileAlreadyExists = 0xe9,				// EXFIL
//	FileAlreadyOpen = 0xe8,					// FOPEN
//	FileIsTooBig = 0xe6,					// FSIZE
//	InvalidFilePointerValue = 0xe5,			// FPTR
//
//	//
//	// Keyboard errors.
//	//
//	InvalidFunctionKeyNumber = 0xe3,		// KFKEY
//	RunOutOfFunctionKeySpace = 0xe2,		// KFSPC
//
//	//
//	// Sound errors.
//	//
//	EnvelopeInvalidOrTooBig = 0xe1,				// SENV
//	NotEnoughRoomToDefineEnvelope = 0xe0,		// SENDBF
//	EnvelopeStorageRequestedTooSmall = 0xdf,	// SENFLO
//	SoundQueueFull = 0xde,						// SQFUL
//
//	//
//	// Video errors.
//	//
//	InvalidRowNumberToScroll = 0xdd,			// VROW
//	AttemptToMoveCursorOffPage = 0xdc,			// VCURS
//	InvalidColourPassedToINKOrPAPER = 0xdb,		// VCOLR
//	InvalidXOrYSizeToOPEN = 0xda,				// VSIZE
//	InvalidVideoModeToOPEN = 0xd9,				// VMODE
//	BadParameterToDISPLAY = 0xdb,				// VDISP, and officially 'naff' rather than 'bad'
//	NotEnoughRowsInPageToDISPLAY = 0xd7,		// VDSP2
//	AttemptToMoveBeamOffPage = 0xd6,			// VBEAM
//	LineStyleTooBig = 0xd5,						// VLSTY
//	LineModeTooBig = 0xd4,						// VLMOD
//	CantDisplayCharacterOrGraphic = 0xd3,		// VCHAR
//
//	//
//	// Serial errors.
//	//
//	InvalidBaudRate = 0xd2,						// BAUD
//
//	//
//	// Editor errors.
//	//
//	InvalidVideoPageForOPEN = 0xd1,				// EVID
//	TroubleInCommunicatingWithKeyboard = 0xd0,	// EKEY
//	InvalidCoordinatesForPosition = 0xcf,		// ECURS
//
//	//
//	// Cassette errors.
//	//
//	CRCErrorFromCassetteDriver = 0xce,			// CCRC
//
//	//
//	// Network errors
//	//
//	SerialDeviceOpenCannotUseNetwork = 0xcd,	// SEROP
//	ADDR_NETNotSetUp = 0xcc,					// NOADR
};

}
