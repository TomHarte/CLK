//
//  PIT.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 20/11/2023.
//  Copyright © 2023 Thomas Harte. All rights reserved.
//

#ifndef PIT_hpp
#define PIT_hpp

namespace PCCompatible {

template <bool is_8254>
class PIT {
	public:
		template <int channel> uint8_t read() {
			const auto result = channels_[channel].read();
			printf("PIT: read from %d; %02x\n", channel, result);
			return result;
		}

		template <int channel> void write(uint8_t value) {
			printf("PIT: write to %d\n", channel);
			channels_[channel].write(value);
		}

		void set_mode(uint8_t value) {
			const int channel_id = (value >> 6) & 3;
			if(channel_id == 3) {
				read_back_ = is_8254;

				// TODO: decode rest of read-back command.
				return;
			}

			printf("PIT: set mode on %d\n", channel_id);

			Channel &channel = channels_[channel_id];
			switch((value >> 4) & 3) {
				default:
					channel.latch_value();
				return;

				case 1:		channel.latch_mode = LatchMode::LowOnly;	break;
				case 2:		channel.latch_mode = LatchMode::HighOnly;	break;
				case 3:		channel.latch_mode = LatchMode::LowHigh;	break;
			}
			channel.is_bcd = value & 1;
			channel.next_access_high = false;

			const auto operating_mode = (value >> 1) & 7;
			switch(operating_mode) {
				default:	channel.mode = OperatingMode(operating_mode);		break;
				case 6:		channel.mode = OperatingMode::RateGenerator;		break;
				case 7:		channel.mode = OperatingMode::SquareWaveGenerator;	break;
			}

			// Set up operating mode.
			switch(channel.mode) {
				default:
					printf("PIT: %d switches to unimplemented mode %d\n", channel_id, int(channel.mode));
				break;

				case OperatingMode::InterruptOnTerminalCount:
					channel.output = false;
					channel.awaiting_reload = true;
				break;

				case OperatingMode::RateGenerator:
					channel.output = true;
					channel.awaiting_reload = true;
				break;
			}
		}

		void run_for(Cycles cycles) {
			// TODO: be intelligent enough to take ticks outside the loop when appropriate.
			auto ticks = cycles.as<int>();
			while(ticks--) {
				bool output_changed;
				output_changed = channels_[0].advance(1);
				output_changed |= channels_[1].advance(1);
				output_changed |= channels_[2].advance(1);
			}
		}

	private:
		// Supported only on 8254s.
		bool read_back_ = false;

		enum class LatchMode {
			LowOnly,
			HighOnly,
			LowHigh,
		};

		enum class OperatingMode {
			InterruptOnTerminalCount		= 0,
			HardwareRetriggerableOneShot	= 1,
			RateGenerator					= 2,
			SquareWaveGenerator				= 3,
			SoftwareTriggeredStrobe			= 4,
			HardwareTriggeredStrobe			= 5,
		};

		struct Channel {
			LatchMode latch_mode = LatchMode::LowHigh;
			OperatingMode mode = OperatingMode::InterruptOnTerminalCount;
			bool is_bcd = false;

			bool gated = false;
			bool awaiting_reload = true;

			uint16_t counter = 0;
			uint16_t reload = 0;
			uint16_t latch = 0;
			bool output = false;

			bool next_access_high = false;

			void latch_value() {
				latch = counter;
			}

			bool advance(int ticks) {
				if(gated || awaiting_reload) return false;

				// TODO: BCD mode is completely ignored below. Possibly not too important.
				const bool initial_output = output;
				switch(mode) {
					case OperatingMode::InterruptOnTerminalCount:
						// Output goes permanently high upon a tick from 1 to 0; reload value is not used on wraparound.
						output |= counter <= ticks;
						counter -= ticks;
					break;

					case OperatingMode::RateGenerator:
						// Output goes low upon a tick from 2 to 1. It goes high again on 1 to 0, and the reload value is used.
						if(counter <= ticks) {
							counter = reload - ticks + counter;
						} else {
							counter -= ticks;
						}
						output = counter != 1;
					break;

					default:
						// TODO.
						break;
				}

				return output != initial_output;
			}

			void write(uint8_t value) {
				switch(latch_mode) {
					case LatchMode::LowOnly:
						reload = (reload & 0xff00) | value;
					break;
					case LatchMode::HighOnly:
						reload = uint16_t((reload & 0x00ff) | (value << 8));
					break;
					case LatchMode::LowHigh:
						next_access_high ^= true;
						if(next_access_high) {
							reload = (reload & 0xff00) | value;
							return;
						}

						reload = uint16_t((reload & 0x00ff) | (value << 8));
					break;
				}

				awaiting_reload = false;

				switch(mode) {
					case OperatingMode::InterruptOnTerminalCount:
					case OperatingMode::RateGenerator:
						counter = reload;
					break;
				}
			}

			uint8_t read() {
				switch(latch_mode) {
					case LatchMode::LowOnly:	return uint8_t(latch);
					case LatchMode::HighOnly:	return uint8_t(latch >> 8);
					default:
					case LatchMode::LowHigh:
						next_access_high ^= true;
						return next_access_high ? uint8_t(latch) : uint8_t(latch >> 8);
					break;
				}
			}
		} channels_[3];

		// TODO:
		//
		//	channel 0 is connected to IRQ 0;
		//	channel 1 is used for DRAM refresh;
		//	channel 2 is gated by a PPI output and feeds into the speaker.
		//
		//	RateGenerator: output goes high if gated.
};

}

#endif /* PIT_hpp */
