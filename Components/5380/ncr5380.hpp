//
//  ncr5380.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 10/08/2019.
//  Copyright © 2019 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>

#include "../../Storage/MassStorage/SCSI/SCSI.hpp"


namespace NCR::NCR5380 {

/*!
	Models the NCR 5380, a SCSI interface chip.
*/
class NCR5380 final: public SCSI::Bus::Observer {
	public:
		NCR5380(SCSI::Bus &bus, int clock_rate);

		/*! Writes @c value to @c address. */
		void write(int address, uint8_t value, bool dma_acknowledge = false);

		/*! Reads from @c address. */
		uint8_t read(int address, bool dma_acknowledge = false);

		/*! @returns The SCSI ID assigned to this device. */
		size_t scsi_id();

		/*! @return @c true if DMA request is active; @c false otherwise. */
		bool dma_request();

		/*! Signals DMA acknowledge with a simultaneous read. */
		uint8_t dma_acknowledge();

		/*! Signals DMA acknowledge with a simultaneous write. */
		void dma_acknowledge(uint8_t);

	private:
		SCSI::Bus &bus_;

		const int clock_rate_;
		size_t device_id_;

		SCSI::BusState bus_output_ = SCSI::DefaultBusState;
		SCSI::BusState expected_phase_ = SCSI::DefaultBusState;
		uint8_t mode_ = 0xff;
		uint8_t initiator_command_ = 0xff;
		uint8_t data_bus_ = 0xff;
		uint8_t target_command_ = 0xff;
		bool test_mode_ = false;
		bool assert_data_bus_ = false;
		bool dma_request_ = false;
		bool dma_acknowledge_ = false;
		bool end_of_dma_ = false;

		bool irq_ = false;
		bool phase_mismatch_ = false;

		enum class ExecutionState {
			None,
			WaitingForBusy,
			WatchingBusy,
			PerformingDMA,
		} state_ = ExecutionState::None;
		enum class DMAOperation {
			Ready,
			Send,
			TargetReceive,
			InitiatorReceive
		} dma_operation_ = DMAOperation::Ready;
		bool lost_arbitration_ = false, arbitration_in_progress_ = false;

		void set_execution_state(ExecutionState state);

		SCSI::BusState target_output() const;
		void update_control_output();

		void scsi_bus_did_change(SCSI::Bus *, SCSI::BusState new_state, double time_since_change) final;
		bool phase_matches() const;
};

}
