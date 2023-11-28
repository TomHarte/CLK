//
//  Status.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef Status_hpp
#define Status_hpp

namespace Intel::i8272 {

enum class MainStatus: uint8_t {
	FDD0Seeking			= 0x01,
	FDD1Seeking			= 0x02,
	FDD2Seeking			= 0x04,
	FDD3Seeking			= 0x08,

	ReadOrWriteOngoing	= 0x10,
	InNonDMAExecution	= 0x20,
	DataIsToProcessor	= 0x40,
	DataReady			= 0x80,
};

enum class Status0: uint8_t {
	NormalTermination	= 0x00,
	AbnormalTermination = 0x80,
	InvalidCommand		= 0x40,
	BecameNotReady		= 0xc0,

	SeekEnded			= 0x20,
	EquipmentFault		= 0x10,
	NotReady			= 0x08,

	HeadAddress			= 0x04,
	UnitSelect			= 0x03,
};

enum class Status1: uint8_t {
	EndOfCylinder		= 0x80,
	DataError 			= 0x20,
	OverRun				= 0x10,
	NoData				= 0x04,
	NotWriteable		= 0x02,
	MissingAddressMark	= 0x01,
};

enum class Status2: uint8_t {
	DeletedControlMark	= 0x40,
	DataCRCError 		= 0x20,
	WrongCyinder		= 0x10,
	ScanEqualHit		= 0x08,
	ScanNotSatisfied	= 0x04,
	BadCylinder			= 0x02,
	MissingDataAddressMark	= 0x01,
};

enum class Status3: uint8_t {
	Fault				= 0x80,
	WriteProtected 		= 0x40,
	Ready				= 0x20,
	Track0				= 0x10,
	TwoSided			= 0x08,
	HeadAddress			= 0x04,
	UnitSelect			= 0x03,
};

class Status {
	public:
		Status() {
			reset();
		}

		void reset() {
			main_status_ = 0;
			set(MainStatus::DataReady, true);
			status_[0] = status_[1] = status_[2] = 0;
		}

		/// @returns The main status register value.
		uint8_t main() const {
			return main_status_;
		}
		uint8_t operator [](int index) const {
			return status_[index];
		}

		//
		// Flag setters.
		//
		void set(MainStatus flag, bool value) {
			set(uint8_t(flag), value, main_status_);
		}
		void start_seek(int drive) 	{	main_status_ |= 1 << drive;	}
		void set(Status0 flag) {	set(uint8_t(flag), true, status_[0]);	}
		void set(Status1 flag) {	set(uint8_t(flag), true, status_[1]);	}
		void set(Status2 flag) {	set(uint8_t(flag), true, status_[2]);	}

		//
		// Flag getters.
		//
		bool get(MainStatus flag)	{	return main_status_ & uint8_t(flag);	}
		bool get(Status2 flag)		{	return status_[2] & uint8_t(flag);		}

		/// Begin execution of whatever @c CommandDecoder currently describes, setting internal
		/// state appropriately.
		void begin(const CommandDecoder &command) {
			set(MainStatus::DataReady, false);

			if(command.is_access()) {
				set(MainStatus::ReadOrWriteOngoing, true);
				status_[0] = command.drive_head();
			}
		}

		void end_sense_interrupt_status(int drive, int head) 	{
			status_[0] = uint8_t(drive | (head << 2));
			main_status_ &= ~(1 << drive);
		}

	private:
		void set(uint8_t flag, bool value, uint8_t &target) {
			if(value) {
				target |= flag;
			} else {
				target &= ~flag;
			}
		}

		uint8_t main_status_;
		uint8_t status_[3];
};

}

#endif /* Status_hpp */
