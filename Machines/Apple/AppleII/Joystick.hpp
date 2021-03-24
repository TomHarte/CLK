//
//  Joystick.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 16/02/2021.
//  Copyright © 2021 Thomas Harte. All rights reserved.
//

#ifndef AppleII_Joystick_hpp
#define AppleII_Joystick_hpp

#include "../../../Inputs/Joystick.hpp"

#include <memory>
#include <vector>

namespace Apple {
namespace II {

class JoystickPair {
	public:
		JoystickPair() {
			// Add a couple of joysticks.
			joysticks_.emplace_back(new Joystick);
			joysticks_.emplace_back(new Joystick);
		}

		class Joystick: public Inputs::ConcreteJoystick {
			public:
				Joystick() :
					ConcreteJoystick({
						Input(Input::Horizontal),
						Input(Input::Vertical),

						// The Apple II offers three buttons between two joysticks;
						// this emulator puts three buttons on each joystick and
						// combines them.
						Input(Input::Fire, 0),
						Input(Input::Fire, 1),
						Input(Input::Fire, 2),
					}) {}

					void did_set_input(const Input &input, float value) final {
						if(!input.info.control.index && (input.type == Input::Type::Horizontal || input.type == Input::Type::Vertical))
							axes[(input.type == Input::Type::Horizontal) ? 0 : 1] = 1.0f - value;
					}

					void did_set_input(const Input &input, bool value) final {
						if(input.type == Input::Type::Fire && input.info.control.index < 3) {
							buttons[input.info.control.index] = value;
						}
					}

				bool buttons[3] = {false, false, false};
				float axes[2] = {0.5f, 0.5f};
		};

		inline bool button(size_t index) {
			return joystick(0)->buttons[index] || joystick(1)->buttons[2-index];
		}

		inline bool analogue_channel_is_discharged(size_t channel) {
			return (1.0f - static_cast<Joystick *>(joysticks_[channel >> 1].get())->axes[channel & 1]) < analogue_charge_ + analogue_biases_[channel];
		}

		inline void update_charge(float one_mhz_cycles = 1.0f) {
			analogue_charge_ = std::min(analogue_charge_ + one_mhz_cycles * (1.0f / 2820.0f), 1.1f);
		}

		inline void access_c070() {
			// Permit analogue inputs that are currently discharged to begin a charge cycle.
			// Ensure those that were still charging retain that state.
			for(size_t c = 0; c < 4; ++c) {
				if(analogue_channel_is_discharged(c)) {
					analogue_biases_[c] = 0.0f;
				} else {
					analogue_biases_[c] += analogue_charge_;
				}
			}
			analogue_charge_ = 0.0f;
		}

		const std::vector<std::unique_ptr<Inputs::Joystick>> &get_joysticks() {
			return joysticks_;
		}

	private:
		// On an Apple II, the programmer strobes 0xc070 and that causes each analogue input
		// to begin a charge and discharge cycle **if they are not already charging**.
		// The greater the analogue input, the faster they will charge and therefore the sooner
		// they will discharge.
		//
		// This emulator models that with analogue_charge_ being essentially the amount of time,
		// in charge threshold units, since 0xc070 was last strobed. But if any of the analogue
		// inputs were already partially charged then they gain a bias in analogue_biases_.
		//
		// It's a little indirect, but it means only having to increment the one value in the
		// main loop.
		float analogue_charge_ = 0.0f;
		float analogue_biases_[4] = {0.0f, 0.0f, 0.0f, 0.0f};

		std::vector<std::unique_ptr<Inputs::Joystick>> joysticks_;

		inline Joystick *joystick(size_t index) {
			return static_cast<Joystick *>(joysticks_[index].get());
		}
};

}
}

#endif /* AppleII_Joystick_hpp */
