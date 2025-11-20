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
	ReadStatus = 9,					// RSTAT
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

// Pages 68–70.
enum class Error: uint8_t {
	//
	// General errors returned by the EXOS kernel.
	//
	InvalidFunctionCode = 0xff,				// IFUNC
	FunctionCallNotAllowed = 0xfe,			// ILLFN
	InvalidFunctionName = 0xfd,				// INAME
	InsufficientStack = 0xfc,				// STACK
	ChannelIllegalOrDoesNotExist = 0xfb,	// ICHAN
	DeviceDoesNotExist = 0xfa,				// NODEV
	ChannelAlreadyExists = 0xf9,			// CHANX
	NoAllocateBufferCallMade = 0xf8,		// NOBUF
	BadAllocateBufferParameters = 0xf7,		// BADAL
	InsufficientRAMForBuffer = 0xf6,		// NORAM
	InsufficientVideoRAM = 0xf5,			// NOVID
	NoFreeSegments = 0xf4,					// NOSEG
	InvalidSegment = 0xf3,					// ISEG
	InvalidUserBoundary = 0xf2,				// IBOUN
	InvalidEXOSVariableNumber = 0xf1,		// IVAR
	InvalidDesviceDescriptorType = 0xf0,	// IDESC

	//
	// General errors returned by various devices.
	//
	InvalidSpecialFunctionCode = 0xef,		// ISPEC
	AttemptToOpenSecondChannel = 0xee,		// 2NDCH
	FunctionNotSupported = 0xed,			// NOFN
	InvalidEscapeCharacter = 0xec,			// ESC
	StopKeyPressed = 0xeb,					// STOP

	//
	// File related errors.
	//
	FileDoesNotExist = 0xea,				// NOFIL
	FileAlreadyExists = 0xe9,				// EXFIL
	FileAlreadyOpen = 0xe8,					// FOPEN
	EndOfFileMetInRead = 0xe7,				// EOF
	FileIsTooBig = 0xe6,					// FSIZE
	InvalidFilePointerValue = 0xe5,			// FPTR
	ProtectionViolation = 0xe4,				// PROT

	//
	// Keyboard errors.
	//
	InvalidFunctionKeyNumber = 0xe3,		// KFKEY
	RunOutOfFunctionKeySpace = 0xe2,		// KFSPC

	//
	// Sound errors.
	//
	EnvelopeInvalidOrTooBig = 0xe1,				// SENV
	NotEnoughRoomToDefineEnvelope = 0xe0,		// SENDBF
	EnvelopeStorageRequestedTooSmall = 0xdf,	// SENFLO
	SoundQueueFull = 0xde,						// SQFUL

	//
	// Video errors.
	//
	InvalidRowNumberToScroll = 0xdd,			// VROW
	AttemptToMoveCursorOffPage = 0xdc,			// VCURS
	InvalidColourPassedToINKOrPAPER = 0xdb,		// VCOLR
	InvalidXOrYSizeToOPEN = 0xda,				// VSIZE
	InvalidVideoModeToOPEN = 0xd9,				// VMODE
	BadParameterToDISPLAY = 0xdb,				// VDISP, and officially 'naff' rather than 'bad'
	NotEnoughRowsInPageToDISPLAY = 0xd7,		// VDSP2
	AttemptToMoveBeamOffPage = 0xd6,			// VBEAM
	LineStyleTooBig = 0xd5,						// VLSTY
	LineModeTooBig = 0xd4,						// VLMOD
	CantDisplayCharacterOrGraphic = 0xd3,		// VCHAR

	//
	// Serial errors.
	//
	InvalidBaudRate = 0xd2,						// BAUD

	//
	// Editor errors.
	//
	InvalidVideoPageForOPEN = 0xd1,				// EVID
	TroubleInCommunicatingWithKeyboard = 0xd0,	// EKEY
	InvalidCoordinatesForPosition = 0xcf,		// ECURS

	//
	// Cassette errors.
	//
	CRCErrorFromCassetteDriver = 0xce,			// CCRC

	//
	// Network errors
	//
	SerialDeviceOpenCannotUseNetwork = 0xcd,	// SEROP
	ADDR_NETNotSetUp = 0xcc,					// NOADR
};

}
