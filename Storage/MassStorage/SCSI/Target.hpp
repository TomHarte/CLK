//
//  Target.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 17/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#ifndef SCSI_Target_hpp
#define SCSI_Target_hpp

#include "SCSI.hpp"

#include <functional>

namespace SCSI {
namespace Target {

/*!
	Encapsulates the arguments supplied for a target SCSI command during
	the command phase plus any other data read since then.
*/
class CommandState {
	public:
		CommandState(const std::vector<uint8_t> &data);

		uint32_t address() const;
		uint16_t number_of_blocks() const;

	private:
		const std::vector<uint8_t> &data_;
};

/*!
	A Responder is supplied both (i) to the initial call-in to an Executor; and
	(ii) to all continuations provided by that Executor. It allows the next
	set of bus interactions to be dictated.
*/
struct Responder {
	using continuation = std::function<void(const CommandState &, Responder &)>;

	enum class Status {
		Good						= 0x00,
		CheckCondition				= 0x02,
		ConditionMet				= 0x04,
		Busy						= 0x08,
		Intermediate				= 0x10,
		IntermediateConditionMet	= 0x14,
		ReservationConflict			= 0x18,
		CommandTerminated			= 0x22,
		TaskSetFull					= 0x28,
		ACAActive					= 0x30,
		TaskAborted					= 0x40
	};

	enum class Message {
		CommandComplete				= 0x00
	};

	/*!
		Causes the SCSI device to send @c data to the initiator and
		call @c next when done.
	*/
	virtual void send_data(std::vector<uint8_t> &&data, continuation next) = 0;
	/*!
		Causes the SCSI device to receive @c length bytes from the initiator and
		call @c next when done. The bytes will be accessible via the CommandInput object.
	*/
	virtual void receive_data(size_t length, continuation next) = 0;
	/*!
		Communicates the supplied status to the initiator.
	*/
	virtual void send_status(Status, continuation next) = 0;
	/*!
		Communicates the supplied message to the initiator.
	*/
	virtual void send_message(Message, continuation next) = 0;
	/*!
		Ends the SCSI command.
	*/
	virtual void end_command() = 0;
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
	bool test_unit_ready(const CommandState &, Responder &)		{	return false;	}
	bool rezero_unit(const CommandState &, Responder &)			{	return false;	}
	bool request_sense(const CommandState &, Responder &)		{	return false;	}
	bool format_unit(const CommandState &, Responder &)			{	return false;	}
	bool seek(const CommandState &, Responder &)				{	return false;	}
	bool reserve_unit(const CommandState &, Responder &)		{	return false;	}
	bool release_unit(const CommandState &, Responder &)		{	return false;	}
	bool read_diagnostic(const CommandState &, Responder &)		{	return false;	}
	bool write_diagnostic(const CommandState &, Responder &)	{	return false;	}
	bool inquiry(const CommandState &, Responder &)				{	return false;	}

	/* Group 0/1 commands. */
	bool read(const CommandState &, Responder &)				{	return false;	}
	bool write(const CommandState &, Responder &)				{	return false;	}

	/* Group 1 commands. */
	bool read_capacity(const CommandState &, Responder &)		{	return false;	}
	bool write_and_verify(const CommandState &, Responder &)	{	return false;	}
	bool verify(const CommandState &, Responder &)				{	return false;	}
	bool search_data_equal(const CommandState &, Responder &)	{	return false;	}
	bool search_data_high(const CommandState &, Responder &)	{	return false;	}
	bool search_data_low(const CommandState &, Responder &)		{	return false;	}

	/*  Group 5 commands. */
	bool set_block_limits(const CommandState &, Responder &)	{	return false;	}
};

/*!
	A template for any SCSI target; provides the necessary bus glue to
	receive and respond to commands. Specific targets should be implemented
	as Executors.
*/
template <typename Executor> class Target: public Bus::Observer, public Responder {
	public:
		/*!
			Instantiates a target attached to @c bus,
			with SCSI ID @c scsi_id — a number in the range 0 to 7.

			Received commands will be handed to the Executor to perform.
		*/
		Target(Bus &bus, int scsi_id);

		inline Executor *operator->() {
			return &executor_;
		}

	private:
		Executor executor_;

		// Bus::Observer.
		void scsi_bus_did_change(Bus *, BusState new_state, double time_since_change) final;

		// Responder
		void send_data(std::vector<uint8_t> &&data, continuation next) final;
		void receive_data(size_t length, continuation next) final;
		void send_status(Status, continuation next) final;
		void send_message(Message, continuation next) final;
		void end_command() final;

		// Instance storage.
		Bus &bus_;
		const BusState scsi_id_mask_;
		const size_t scsi_bus_device_id_;

		enum class Phase {
			AwaitingSelection,
			Command,
			ReceivingData,
			SendingData,
			SendingStatus,
			SendingMessage
		} phase_ = Phase::AwaitingSelection;
		BusState bus_state_ = DefaultBusState;

		void set_device_output(BusState state) {
			expected_control_state_ = state & (Line::Control | Line::Input | Line::Message);
			bus_.set_device_output(scsi_bus_device_id_, state);
		}
		BusState expected_control_state_ = DefaultBusState;

		void begin_command(uint8_t first_byte);
		std::vector<uint8_t> command_;
		Status status_;
		Message message_;
		size_t command_pointer_ = 0;
		bool dispatch_command();

		std::vector<uint8_t> data_;
		size_t data_pointer_ = 0;

		continuation next_function_;
};

#import "TargetImplementation.hpp"

}
}

#endif /* SCSI_Target_hpp */
