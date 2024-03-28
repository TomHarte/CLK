//
//  I2C.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include <cstdint>
#include <optional>
#include <unordered_map>

namespace I2C {

/// Provides the virtual interface for an I2C peripheral; attaching this to a bus
/// provides automatic protocol handling.
struct Peripheral {
	/// Indicates that the host signalled the start condition and addressed this
	/// peripheral, along with whether it indicated a read or write.
	virtual void start([[maybe_unused]] bool is_read) {}

	/// Indicates that the host signalled a stop.
	virtual void stop() {}

	/// Requests the next byte to serialise onto the I2C bus after this peripheral has
	/// been started in read mode.
	///
	/// @returns A byte to serialise or std::nullopt if the peripheral declines to
	/// continue to communicate.
	virtual std::optional<uint8_t> read() { return std::nullopt; }

	/// Provides a byte received from the bus after this peripheral has been started
	/// in write mode.
	///
	/// @returns @c true if the write should be acknowledged; @c false otherwise.
	virtual bool write(uint8_t) { return false; }
};

class Bus {
public:
	void set_data(bool pulled);
	bool data();

	void set_clock(bool pulled);
	bool clock();

	void set_clock_data(bool clock_pulled, bool data_pulled);

	void add_peripheral(Peripheral *, int address);

private:
	bool data_ = false;
	bool clock_ = false;
	bool in_bit_ = false;
	std::unordered_map<int, Peripheral *> peripherals_;

	uint16_t input_ = 0xffff;
	int input_count_ = -1;

	Peripheral *active_peripheral_ = nullptr;
	uint16_t peripheral_response_ = 0xffff;
	int peripheral_bits_ = 0;

	enum class Event {
		Zero, One, Start, Stop, FinishedOutput,
	};
	void signal(Event);

	enum class State {
		AwaitingAddress,
		CollectingAddress,

		CompletingReadAcknowledge,
		AwaitingByteAcknowledge,

		ReceivingByte,
	} state_ = State::AwaitingAddress;
};

}
