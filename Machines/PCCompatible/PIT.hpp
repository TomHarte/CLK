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
			return channels_[channel].read();
		}

		template <int channel> void write(uint8_t value) {
			channels_[channel].write(value);
		}

		void set_mode(uint8_t value) {
			const int channel_id = (value >> 6) & 3;
			if(channel_id == 3) {
				// TODO: decode rest of read-back command.
				read_back_ = is_8254;
				return;
			}
			channels_[channel_id].set_mode(value);
		}

		void run_for(Cycles cycles) {
			// TODO: be intelligent enough to take ticks outside the loop when appropriate.
			auto ticks = cycles.as<int>();
			while(ticks--) {
				channels_[0].advance(1);
				channels_[1].advance(1);
				channels_[2].advance(1);
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

			void set_mode(uint8_t value) {
				switch((value >> 4) & 3) {
					default:
						latch_value();
					return;

					case 1:		latch_mode = LatchMode::LowOnly;	break;
					case 2:		latch_mode = LatchMode::HighOnly;	break;
					case 3:		latch_mode = LatchMode::LowHigh;	break;
				}
				is_bcd = value & 1;
				next_access_high = false;

				const auto operating_mode = (value >> 1) & 7;
				switch(operating_mode) {
					default:	mode = OperatingMode(operating_mode);		break;
					case 6:		mode = OperatingMode::RateGenerator;		break;
					case 7:		mode = OperatingMode::SquareWaveGenerator;	break;
				}

				// Set up operating mode.
				switch(mode) {
					default:
						printf("PIT: unimplemented mode %d\n", int(mode));
					break;

					case OperatingMode::InterruptOnTerminalCount:
					case OperatingMode::HardwareRetriggerableOneShot:
						set_output(false);
						awaiting_reload = true;
					break;

					case OperatingMode::RateGenerator:
					case OperatingMode::SquareWaveGenerator:
						set_output(true);
						awaiting_reload = true;
					break;
				}
			}

			void advance(int ticks) {
				if(gated || awaiting_reload) return;

				// TODO: BCD mode is completely ignored below. Possibly not too important.
				switch(mode) {
					case OperatingMode::InterruptOnTerminalCount:
					case OperatingMode::HardwareRetriggerableOneShot:
						// Output goes permanently high upon a tick from 1 to 0; reload value is not reused.
						set_output(output | (counter <= ticks));
						counter -= ticks;
					break;

					case OperatingMode::SquareWaveGenerator: {
						ticks <<= 1;
						do {
							// If there's a step from 1 to 0 within the next batch of ticks,
							// toggle output and apply a reload.
							if(counter && ticks >= counter) {
								set_output(output ^ true);
								ticks -= counter;

								const uint16_t reload_mask = output ? 0xffff : 0xfffe;
								counter = reload & reload_mask;

								continue;
							}
							counter -= ticks;
						} while(false);
					} break;

					case OperatingMode::RateGenerator:
						do {
							// Check for a step from 2 to 1 within the next batch of ticks, which would cause output
							// to go high.
							if(counter > 1 && ticks >= counter - 1) {
								set_output(true);
								ticks -= counter - 1;
								counter = 1;
								continue;
							}

							// If there is a step from 1 to 0, reload and set output back to low.
							if(counter && ticks >= counter) {
								set_output(false);
								ticks -= counter;
								counter = reload;
								continue;
							}

							// Otherwise, just continue.
							counter -= ticks;
						} while(false);
					break;

					default:
						// TODO.
						break;
				}
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
							awaiting_reload = true;
							return;
						}

						reload = uint16_t((reload & 0x00ff) | (value << 8));
					break;
				}

				awaiting_reload = false;

				switch(mode) {
					default:
						counter = reload;
					break;

					case OperatingMode::SquareWaveGenerator:
						counter = reload & ~1;
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

			void set_output(bool level) {
				if(output == level) {
					return;
				}

				output = level;
				// TODO: notify _someone_.
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
