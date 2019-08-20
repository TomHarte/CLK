//
//  DirectAccessDevice.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef DirectAccessDevice_hpp
#define DirectAccessDevice_hpp

#include "SCSI.hpp"

namespace SCSI {
namespace Target {

/*!
	Encapsulates the arguments supplied for a target SCSI command during
	the command phase. An instance of TargetCommandArguments will be
	supplied to the target whenever a function is called.
*/
class CommandArguments {
	public:
		CommandArguments(const std::vector<uint8_t> &data);

		uint32_t address();
		uint16_t number_of_blocks();

	private:
		const std::vector<uint8_t> &data_;
};

/*!
	Executors contain device-specific logic; when the target has completed
	the command phase it will call the appropriate method on its executor,
	supplying it with the command's arguments.

	If you implement a method, you should push a result and return @c true.
	Return @c false if you do not implement a method (or, just inherit from
	the basic executor below, and don't implement anything you don't support).
*/
struct Executor {
	/* Group 0 commands. */
	bool test_unit_ready(const CommandArguments &)		{	return false;	}
	bool rezero_unit(const CommandArguments &)			{	return false;	}
	bool request_sense(const CommandArguments &)		{	return false;	}
	bool format_unit(const CommandArguments &)			{	return false;	}
	bool seek(const CommandArguments &)					{	return false;	}
	bool reserve_unit(const CommandArguments &)			{	return false;	}
	bool release_unit(const CommandArguments &)			{	return false;	}
	bool read_diagnostic(const CommandArguments &)		{	return false;	}
	bool write_diagnostic(const CommandArguments &)		{	return false;	}
	bool inquiry(const CommandArguments &)				{	return false;	}

	/* Group 0/1 commands. */
	bool read(const CommandArguments &)					{	return false;	}
	bool write(const CommandArguments &)				{	return false;	}

	/* Group 1 commands. */
	bool read_capacity(const CommandArguments &)		{	return false;	}
	bool write_and_verify(const CommandArguments &)		{	return false;	}
	bool verify(const CommandArguments &)				{	return false;	}
	bool search_data_equal(const CommandArguments &)	{	return false;	}
	bool search_data_high(const CommandArguments &)		{	return false;	}
	bool search_data_low(const CommandArguments &)		{	return false;	}

	/*  Group 5 commands. */
	bool set_block_limits(const CommandArguments &)		{	return false;	}
	void reset_block_limits(const CommandArguments &)	{}
};

/*!
	A template for any SCSI target; provides the necessary bus glue to
	receive and respond to commands. Specific targets should be implemented
	as Executors.
*/
template <typename Executor> class Target: public Bus::Observer {
	public:
		/*!
			Instantiates a target attached to @c bus,
			with SCSI ID @c scsi_id — a number in the range 0 to 7.

			Received commands will be handed to the Executor to perform.
		*/
		Target(Bus &bus, int scsi_id);

		Executor executor;

	private:
		void scsi_bus_did_change(Bus *, BusState new_state) final;

		Bus &bus_;
		const BusState scsi_id_mask_;
		const size_t scsi_bus_device_id_;

		enum class Phase {
			AwaitingSelection,
			Command
		} phase_ = Phase::AwaitingSelection;
		BusState bus_state_ = DefaultBusState;

		void begin_command(uint8_t first_byte);
		std::vector<uint8_t> command_;
		size_t command_pointer_ = 0;
		bool dispatch_command();
};

#import "TargetImplementation.hpp"

}
}

#endif /* DirectAccessDevice_hpp */
