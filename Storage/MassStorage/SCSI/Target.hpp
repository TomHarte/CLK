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

#include <cstring>
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

		// For read and write commands.
		uint32_t address() const;
		uint16_t number_of_blocks() const;

		// For inquiry commands.
		size_t allocated_inquiry_bytes() const;

		// For mode sense commands.
		struct ModeSense {
			bool exclude_block_descriptors = false;
			enum class PageControlValues {
				Current = 0,
				Changeable = 1,
				Default = 2,
				Saved = 3
			} page_control_values = PageControlValues::Current;
			uint8_t page_code;
			uint8_t subpage_code;
			uint16_t allocated_bytes;
		};
		ModeSense mode_sense_specs() const;

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
	/*!
		Terminates a SCSI command, sending the proper sequence of status and message phases.
	*/
	void terminate_command(Status status) {
		send_status(status, [] (const Target::CommandState &state, Target::Responder &responder) {
			responder.send_message(Target::Responder::Message::CommandComplete, [] (const Target::CommandState &state, Target::Responder &responder) {
				responder.end_command();
			});
		});
	}
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
	bool test_unit_ready(const CommandState &, Responder &responder)		{
		/* "Returns zero status if addressed unit is powered on and ready. */
		responder.terminate_command(Target::Responder::Status::Good);
		return true;
	}
	bool rezero_unit(const CommandState &, Responder &)			{	return false;	}
	bool request_sense(const CommandState &, Responder &)		{	return false;	}
	bool format_unit(const CommandState &, Responder &)			{	return false;	}
	bool seek(const CommandState &, Responder &)				{	return false;	}
	bool reserve_unit(const CommandState &, Responder &)		{	return false;	}
	bool release_unit(const CommandState &, Responder &)		{	return false;	}
	bool read_diagnostic(const CommandState &, Responder &)		{	return false;	}
	bool write_diagnostic(const CommandState &, Responder &)	{	return false;	}

	/// Mode sense: the default implementation will call into the appropriate
	/// strucutred getter.
	bool mode_sense(const CommandState &state, Responder &responder) {
		const auto specs = state.mode_sense_specs();
		std::vector<uint8_t> response = {
			specs.page_code,
			uint8_t(specs.allocated_bytes)
		};
		switch(specs.page_code) {
			default:
				printf("Unknown mode sense page code %02x\n", specs.page_code);
				response.resize(specs.allocated_bytes);
			break;

			case 0x30:
				response.resize(34);
				strcpy(reinterpret_cast<char *>(&response[14]), "APPLE COMPUTER, INC");	// This seems to be required to satisfy the Apple HD SC Utility.
			break;
		}

		if(specs.allocated_bytes < response.size()) {
			response.resize(specs.allocated_bytes);
		}
		responder.send_data(std::move(response), [] (const Target::CommandState &state, Target::Responder &responder) {
			responder.terminate_command(Target::Responder::Status::Good);
		});

		return true;
	}

	/// Inquiry: the default implementation will call the structured version and
	/// package appropriately.
	struct Inquiry {
		enum class DeviceType {
			DirectAccess = 0,
			SequentialAccess = 1,
			Printer = 2,
			Processor = 3,
			WriteOnceMultipleRead = 4,
			ReadOnlyDirectAccess = 5,
			Scanner = 6,
			OpticalMemory = 7,
			MediumChanger = 8,
			Communications = 9,
		} device_type = DeviceType::DirectAccess;
		bool is_removeable = false;
		uint8_t iso_standard = 0, ecma_standard = 0, ansi_standard = 0;
		bool supports_asynchronous_events = false;
		bool supports_terminate_io_process = false;
		bool supports_relative_addressing = false;
		bool supports_synchronous_transfer = true;
		bool supports_linked_commands = false;
		bool supports_command_queing = false;
		bool supports_soft_reset = false;
		char vendor_identifier[9] = "";
		char product_identifier[17] = "";
		char product_revision_level[5] = "";

		Inquiry(const char *vendor, const char *product, const char *revision) {
			assert(strlen(vendor) <= 8);
			assert(strlen(product) <= 16);
			assert(strlen(revision) <= 4);
			strcpy(vendor_identifier, vendor);
			strcpy(product_identifier, product);
			strcpy(product_revision_level, revision);
		}
		Inquiry() = default;
	};
	Inquiry inquiry_values() {
		return Inquiry();
	}
	bool inquiry(const CommandState &state, Responder &responder) {
		const Inquiry inq = inquiry_values();

		// Set up the easy fields.
		std::vector<uint8_t> response = {
			uint8_t(inq.device_type),
			uint8_t(inq.is_removeable ? 0x80 : 0x00),
			uint8_t((inq.iso_standard << 5) | (inq.ecma_standard << 3) | (inq.ansi_standard)),
			uint8_t((inq.supports_asynchronous_events ? 0x80 : 0x00) | (inq.supports_terminate_io_process ? 0x40 : 0x00) | 0x02),
			32,		/* Additional length: 36 - 4. */
			0x00,	/* Reserved. */
			0x00,	/* Reserved. */
			uint8_t(
				(inq.supports_relative_addressing ? 0x80 : 0x00) |
				/* b6: supports 32-bit data; b5: supports 16-bit data. */
				(inq.supports_synchronous_transfer ? 0x10 : 0x00) |
				(inq.supports_linked_commands ? 0x08 : 0x00) |
				/* b3: reserved. */
				(inq.supports_command_queing ? 0x02 : 0x00) |
				(inq.supports_soft_reset ? 0x01 : 0x00)
			),
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* Space for the vendor ID. */
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
			0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,	/* Space for the product ID. */
			0x00, 0x00, 0x00, 0x00							/* Space for the revision level. */
		};

		auto copy_string = [] (uint8_t *destination, const char *source, size_t length) -> void {
			// Copy as much of the string as will fit, and pad with spaces.
			uint8_t *end = reinterpret_cast<uint8_t *>(stpncpy(reinterpret_cast<char *>(destination), source, length));
			while(end < destination + length) {
				*end = ' ';
				++end;
			}
		};
		copy_string(&response[8], inq.vendor_identifier, 8);
		copy_string(&response[16], inq.product_identifier, 16);
		copy_string(&response[32], inq.product_revision_level, 4);

		// Truncate if requested.
		const auto allocated_bytes = state.allocated_inquiry_bytes();
		if(allocated_bytes < response.size()) {
			response.resize(allocated_bytes);
		}

		responder.send_data(std::move(response), [] (const Target::CommandState &state, Target::Responder &responder) {
			responder.terminate_command(Target::Responder::Status::Good);
		});

		return true;
	}

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
