//
//  Archimedes.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 04/03/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "Archimedes.hpp"

#include "HalfDuplexSerial.hpp"
#include "InputOutputController.hpp"
#include "Keyboard.hpp"
#include "KeyboardMapper.hpp"
#include "MemoryController.hpp"
#include "Sound.hpp"

#include "../../AudioProducer.hpp"
#include "../../KeyboardMachine.hpp"
#include "../../MediaTarget.hpp"
#include "../../MouseMachine.hpp"
#include "../../ScanProducer.hpp"
#include "../../TimedMachine.hpp"

#include "../../../Activity/Source.hpp"

#include "../../../InstructionSets/ARM/Disassembler.hpp"
#include "../../../InstructionSets/ARM/Executor.hpp"
#include "../../../Outputs/Log.hpp"
#include "../../../Components/I2C/I2C.hpp"

#include "../../../Analyser/Static/Acorn/Target.hpp"

#include <algorithm>
#include <array>
#include <set>
#include <vector>

namespace Archimedes {

class ConcreteMachine:
	public Machine,
	public MachineTypes::AudioProducer,
	public MachineTypes::MappedKeyboardMachine,
	public MachineTypes::MediaTarget,
	public MachineTypes::MouseMachine,
	public MachineTypes::TimedMachine,
	public MachineTypes::ScanProducer,
	public Activity::Source
{
	private:
		Log::Logger<Log::Source::Archimedes> logger;

		// This fictitious clock rate just means '24 MIPS, please'; it's divided elsewhere.
		static constexpr int ClockRate = 24'000'000;

		// Runs for 24 cycles, distributing calls to the various ticking subsystems
		// 'correctly' (i.e. correctly for the approximation in use).
		//
		// The implementation of this is coupled to the ClockRate above, hence its
		// appearance here.
		template <int video_divider, bool original_speed>
		void macro_tick() {
			macro_counter_ -= 24;

			// This is a 24-cycle window, so at 24Mhz macro_tick() is called at 1Mhz.
			// Hence, required ticks are:
			//
			// 	* CPU: 24;
			//	* video: 24 / video_divider;
			//	* floppy: 8;
			//	* timers: 2;
			//	* sound: 1.

			tick_cpu_video<0, video_divider, original_speed>();		tick_cpu_video<1, video_divider, original_speed>();
			tick_cpu_video<2, video_divider, original_speed>();		tick_floppy();
			tick_cpu_video<3, video_divider, original_speed>();		tick_cpu_video<4, video_divider, original_speed>();
			tick_cpu_video<5, video_divider, original_speed>();		tick_floppy();
			tick_cpu_video<6, video_divider, original_speed>();		tick_cpu_video<7, video_divider, original_speed>();
			tick_cpu_video<8, video_divider, original_speed>();		tick_floppy();
			tick_cpu_video<9, video_divider, original_speed>();		tick_cpu_video<10, video_divider, original_speed>();
			tick_cpu_video<11, video_divider, original_speed>();	tick_floppy();
			tick_timers();

			tick_cpu_video<12, video_divider, original_speed>();	tick_cpu_video<13, video_divider, original_speed>();
			tick_cpu_video<14, video_divider, original_speed>();	tick_floppy();
			tick_cpu_video<15, video_divider, original_speed>();	tick_cpu_video<16, video_divider, original_speed>();
			tick_cpu_video<17, video_divider, original_speed>();	tick_floppy();
			tick_cpu_video<18, video_divider, original_speed>();	tick_cpu_video<19, video_divider, original_speed>();
			tick_cpu_video<20, video_divider, original_speed>();	tick_floppy();
			tick_cpu_video<21, video_divider, original_speed>();	tick_cpu_video<22, video_divider, original_speed>();
			tick_cpu_video<23, video_divider, original_speed>();	tick_floppy();
			tick_timers();
			tick_sound();
		}
		int macro_counter_ = 0;

		template <int offset, int video_divider, bool original_speed>
		void tick_cpu_video() {
			if constexpr (!(offset % video_divider)) {
				tick_video();
			}

			// Debug mode: run CPU a lot slower. Actually at close to original advertised MIPS speed.
			if constexpr (original_speed && (offset & 7)) return;
			if constexpr (offset & 1) return;
			tick_cpu();
		}

	public:
		ConcreteMachine(
			const Analyser::Static::Acorn::ArchimedesTarget &target,
			const ROMMachine::ROMFetcher &rom_fetcher
		) : executor_(*this, *this, *this) {
			set_clock_rate(ClockRate);

			constexpr ROM::Name risc_os = ROM::Name::AcornRISCOS311;
			ROM::Request request(risc_os);
			auto roms = rom_fetcher(request);
			if(!request.validate(roms)) {
				throw ROMMachine::Error::MissingROMs;
			}

			executor_.bus.set_rom(roms.find(risc_os)->second);
			insert_media(target.media);

			if(!target.media.disks.empty()) {
				autoload_phase_ = AutoloadPhase::WaitingForStartup;
				target_program_ = target.main_program;

				printf("Will seek %s?\n", target_program_.c_str());
			}

			fill_pipeline(0);
		}

		void update_interrupts() {
			using Exception = InstructionSet::ARM::Registers::Exception;

			const int requests = executor_.bus.interrupt_mask();
			if((requests & InterruptRequests::FIQ) && executor_.registers().would_interrupt<Exception::FIQ>()) {
				pipeline_.reschedule(Pipeline::SWISubversion::FIQ);
				return;
			}
			if((requests & InterruptRequests::IRQ) && executor_.registers().would_interrupt<Exception::IRQ>()) {
				pipeline_.reschedule(Pipeline::SWISubversion::IRQ);
			}
		}

		void did_set_status() {
			// This might have been a change of mode, so...
			trans_ = executor_.registers().mode() == InstructionSet::ARM::Mode::User;
			fill_pipeline(executor_.pc());
			update_interrupts();
		}

		void did_set_pc() {
			fill_pipeline(executor_.pc());
		}

		bool should_swi(uint32_t comment) {
			using Exception = InstructionSet::ARM::Registers::Exception;
			using SWISubversion = Pipeline::SWISubversion;

			switch(pipeline_.swi_subversion()) {
				case Pipeline::SWISubversion::None: {
					// TODO: 400C1 to intercept create window 400C1 and positioning; then
					// plot icon 400e2 to listen for icons in window. That'll give a click area.
					// Probably also 400c2 which seems to be used to add icons to the icon bar.
					//
					// 400D4 for menus?

					const auto get_string = [&](uint32_t address, bool indirect) -> std::string {
						std::string desc;
						if(indirect) {
							executor_.bus.read(address, address, false);
						}
						while(true) {
							uint8_t next;
							executor_.bus.read(address, next, false);
							if(next < 0x20) break;
							desc.push_back(static_cast<char>(next) & 0x7f);
							++address;
						}
						return desc;
					};

					const uint32_t swi_code = comment & static_cast<uint32_t>(~(1 << 17));
					switch(swi_code) {
						// To consider: catching VDU 22, though that means parsing the output stream
						// via OS_WriteC, SWI &00, sufficiently to be able to spot VDUs.

						case 0x400e3:	// Wimp_SetMode
						case 0x65:		// OS_ScreenMode
						case 0x3f:		// OS_CheckModeValid
							if(autoload_phase_ == AutoloadPhase::OpeningProgram) {
								autoload_phase_ = AutoloadPhase::Ended;
							}
						break;

						case 0x400d4: {
							if(autoload_phase_ == AutoloadPhase::TestingMenu) {
								autoload_phase_ = AutoloadPhase::Ended;

								uint32_t address = executor_.registers()[1] + 28;
								bool should_left_click = true;

								while(true) {
									uint32_t icon_flags;
									uint32_t item_flags;
									executor_.bus.read(address, item_flags, false);
									executor_.bus.read(address + 8, icon_flags, false);
									auto desc = get_string(address + 12, icon_flags & (1 << 8));

									should_left_click &=
										(desc == "Info") ||
										(desc == "Quit");

									address += 24;
									if(item_flags & (1 << 7)) break;
								}

								if(should_left_click) {
									// Exit the application menu, then click once further to launch.
									CursorActionBuilder(cursor_actions_)
										.move_to(IconBarProgramX - 128, IconBarY - 32)
										.click(0)
										.move_to(IconBarProgramX, IconBarY)
										.click(0);
								}
							}
						} break;

						// Wimp_OpenWindow.
						case 0x400c5: {
							const uint32_t address = executor_.registers()[1];
							uint32_t x1, y1, x2, y2;
							executor_.bus.read(address + 4, x1, false);
							executor_.bus.read(address + 8, y1, false);
							executor_.bus.read(address + 12, x2, false);
							executor_.bus.read(address + 16, y2, false);

							switch(autoload_phase_) {

								default: break;

								case AutoloadPhase::WaitingForDiskContents: {
									autoload_phase_ = AutoloadPhase::WaitingForTargetIcon;

									// Crib top left of window content.
									target_window_[0] = static_cast<int32_t>(x1);
									target_window_[1] = static_cast<int32_t>(y2);
								} break;

								case AutoloadPhase::WaitingForStartup:
									printf("%d %d %d %d\n", x1, y1, x2, y2);
									if(static_cast<int32_t>(y1) == -268435472) {		// VERY TEMPORARY. TODO: find better trigger.
										// Creation of any icon is used to spot that RISC OS has started up.
										//
										// Wait a further second, mouse down to (32, 240), left click.
										// That'll trigger disk access. Then move up to the top left,
										// in anticipation of the appearance of a window.
										CursorActionBuilder(cursor_actions_)
//											.wait(5 * 24'000'000)
											.move_to(IconBarDriveX, IconBarY)
											.click(0)
											.set_phase(
												target_program_.empty() ? AutoloadPhase::Ended : AutoloadPhase::WaitingForDiskContents
											)
											.move_to(IconBarDriveX, 36);	// Just a guess of 'close' to where the program to launch
																			// will probably be, to have the cursor already nearby.

										autoload_phase_ = AutoloadPhase::OpeningDisk;
									}
								break;
//								printf("Wimp_OpenWindow: %d, %d -> %d, %d\n", x1, y1, x2, y2);
							}
						} break;

						// Wimp_CreateIcon, which also adds to the icon bar.
						case 0x400c2:
							switch(autoload_phase_) {
//								case AutoloadPhase::WaitingForStartup:
//									// Creation of any icon is used to spot that RISC OS has started up.
//									//
//									// Wait a further second, mouse down to (32, 240), left click.
//									// That'll trigger disk access. Then move up to the top left,
//									// in anticipation of the appearance of a window.
//									CursorActionBuilder(cursor_actions_)
//										.wait(24'000'000)
//										.move_to(IconBarDriveX, IconBarY)
//										.click(0)
//										.set_phase(
//											target_program_.empty() ? AutoloadPhase::Ended : AutoloadPhase::WaitingForDiskContents
//										)
//										.move_to(IconBarDriveX, 36);	// Just a guess of 'close' to where the program to launch
//																		// will probably be, to have the cursor already nearby.
//
//									autoload_phase_ = AutoloadPhase::OpeningDisk;
//								break;

								case AutoloadPhase::OpeningProgram: {
									const uint32_t address = executor_.registers()[1];
									uint32_t handle;
									executor_.bus.read(address, handle, false);

									// Test whether the program has added an icon on the right.
									if(static_cast<int32_t>(handle) == -1) {
										CursorActionBuilder(cursor_actions_)
											.move_to(IconBarProgramX, IconBarY)
											.click(1);
										autoload_phase_ = AutoloadPhase::TestingMenu;
									}
								} break;

								default: break;
							}
						break;

						// Wimp_PlotIcon.
						case 0x400e2: {
							if(autoload_phase_ == AutoloadPhase::WaitingForTargetIcon) {
								const uint32_t address = executor_.registers()[1];
								uint32_t flags;
								executor_.bus.read(address + 16, flags, false);

								std::string desc;
								if(flags & 1) {
									desc = get_string(address + 20, flags & (1 << 8));
								}

								if(desc == target_program_) {
									uint32_t x1, y1, x2, y2;
									executor_.bus.read(address + 0, x1, false);
									executor_.bus.read(address + 4, y1, false);
									executor_.bus.read(address + 8, x2, false);
									executor_.bus.read(address + 12, y2, false);

									autoload_phase_ = AutoloadPhase::OpeningProgram;

									// Some default icon sizing assumptions are baked in here.
									const auto x_target = target_window_[0] + (static_cast<int32_t>(x1) + static_cast<int32_t>(x2)) / 2;
									const auto y_target = target_window_[1] + static_cast<int32_t>(y1) + 24;
									CursorActionBuilder(cursor_actions_)
										.move_to(x_target >> 1, 256 - (y_target >> 2))
										.double_click(0);
								}
							}
						} break;
					}
				} return true;

				case SWISubversion::DataAbort:
//					executor_.set_pc(executor_.pc() - 4);
					executor_.registers().exception<Exception::DataAbort>();
				break;

				// FIQ and IRQ decrement the PC because their apperance in the pipeline causes
				// it to look as though they were fetched, but they weren't.
				case SWISubversion::FIQ:
					executor_.set_pc(executor_.pc() - 4);
					executor_.registers().exception<Exception::FIQ>();
				break;
				case SWISubversion::IRQ:
					executor_.set_pc(executor_.pc() - 4);
					executor_.registers().exception<Exception::IRQ>();
				break;
			}

			did_set_pc();
			return false;
		}

		void update_clock_rates() {
			video_divider_ = executor_.bus.video().clock_divider();
		}

	private:
		// MARK: - ScanProducer.
		void set_scan_target(Outputs::Display::ScanTarget *scan_target) override {
			executor_.bus.video().crt().set_scan_target(scan_target);
		}
		Outputs::Display::ScanStatus get_scaled_scan_status() const override {
			return executor_.bus.video().crt().get_scaled_scan_status() * video_divider_;
		}

		// MARK: - TimedMachine.
		int video_divider_ = 1;
		void run_for(Cycles cycles) override {
#ifndef NDEBUG
			// Debug mode: always run 'slowly' because that's less of a burden, and
			// because it allows me to peer at problems with greater leisure.
			const bool use_original_speed = true;
#else
			// As a first, blunt implementation: try to model something close
			// to original speed if there have been 10 frame rate overages in total.
			const bool use_original_speed = executor_.bus.video().frame_rate_overages() > 10;
#endif

			const auto run = [&](Cycles cycles) {
				if(use_original_speed) run_for<true>(cycles);
				else run_for<false>(cycles);
			};

			//
			// Short-circuit: no cursor actions means **just run**.
			//
			if(cursor_actions_.empty()) {
				run(cycles);
				return;
			}

			//
			// Mouse scripting; tick at a minimum of frame length.
			//
			static constexpr int TickFrequency = 24'000'000 / 50;
			cursor_action_subcycle_ += cycles;
			auto segments = cursor_action_subcycle_.divide(Cycles(TickFrequency)).as<int>();
			while(segments--) {
				Cycles next = Cycles(TickFrequency);
				if(next > cycles) next = cycles;
				cycles -= next;

				if(!cursor_actions_.empty()) {
					const auto move_to_next = [&]() {
						cursor_action_waited_ = 0;
						cursor_actions_.erase(cursor_actions_.begin());
					};

					const auto &action = cursor_actions_.front();
					switch(action.type) {
						case CursorAction::Type::MoveTo: {
							// A measure of where within the tip lies within
							// the default RISC OS cursor.
							constexpr int ActionPointOffset = 20;
							constexpr int MaxStep = 24;

							const auto position = executor_.bus.video().cursor_location();
							if(!position) break;
							const auto [x, y] = *position;

							auto x_diff = action.value.move_to.x - (x + ActionPointOffset);
							auto y_diff = action.value.move_to.y - y;

							if(abs(x_diff) < 2 && abs(y_diff) < 2) {
								move_to_next();
								break;
							}

							if(abs(y_diff) > MaxStep || abs(x_diff) > MaxStep) {
								if(abs(y_diff) > abs(x_diff)) {
									x_diff = (x_diff * MaxStep + (abs(y_diff) >> 1)) / abs(y_diff);
									y_diff = std::clamp(y_diff, -MaxStep, MaxStep);
								} else {
									y_diff = (y_diff * MaxStep + (abs(x_diff) >> 1)) / abs(x_diff);
									x_diff = std::clamp(x_diff, -MaxStep, MaxStep);
								}
							}
							get_mouse().move(x_diff, y_diff);
						} break;
						case CursorAction::Type::Wait:
							cursor_action_waited_ += next.as<int>();
							if(cursor_action_waited_ >= action.value.wait.duration) {
								move_to_next();
							}
						break;
						case CursorAction::Type::Button:
							get_mouse().set_button_pressed(action.value.button.button, action.value.button.down);
							move_to_next();
						break;
						case CursorAction::Type::SetPhase:
							autoload_phase_ = action.value.set_phase.phase;
							move_to_next();
						break;
					}
				}

				//
				// Execution proper.
				//
				run(next);
			}
		}

		template <bool original_speed>
		void run_for(Cycles cycles) {
			macro_counter_ += cycles.as<int>();

			while(macro_counter_ > 0) {
				switch(video_divider_) {
					default:	macro_tick<2, original_speed>();	break;
					case 3:		macro_tick<3, original_speed>();	break;
					case 4:		macro_tick<4, original_speed>();	break;
					case 6:		macro_tick<6, original_speed>();	break;
				}
			}
		}

		void tick_cpu() {
			const uint32_t instruction = advance_pipeline(executor_.pc() + 8);
			InstructionSet::ARM::execute(instruction, executor_);
		}

		void tick_timers()	{	executor_.bus.tick_timers();	}
		void tick_sound()	{	executor_.bus.sound().tick();	}
		void tick_video()	{	executor_.bus.video().tick();	}
		void tick_floppy()	{	executor_.bus.tick_floppy();	}

		// MARK: - MediaTarget
		bool insert_media(const Analyser::Static::Media &media) override {
			size_t c = 0;
			for(auto &disk : media.disks) {
				executor_.bus.set_disk(disk, c);
				c++;
				if(c == 4) break;
			}
			return true;
		}

		// MARK: - AudioProducer
		Outputs::Speaker::Speaker *get_speaker() override {
			return executor_.bus.speaker();
		}

		// MARK: - Activity::Source.
		void set_activity_observer(Activity::Observer *observer) final {
			executor_.bus.set_activity_observer(observer);
		}

		// MARK: - MappedKeyboardMachine.
		MappedKeyboardMachine::KeyboardMapper *get_keyboard_mapper() override {
			return &keyboard_mapper_;
		}
		Archimedes::KeyboardMapper keyboard_mapper_;

		void set_key_state(uint16_t key, bool is_pressed) override {
			executor_.bus.keyboard().set_key_state(key, is_pressed);
		}

		// MARK: - MouseMachine.
		Inputs::Mouse &get_mouse() override {
			return executor_.bus.keyboard().mouse();
		}

		// MARK: - ARM execution.
		static constexpr auto arm_model = InstructionSet::ARM::Model::ARMv2;
		using Executor = InstructionSet::ARM::Executor<arm_model, MemoryController<ConcreteMachine, ConcreteMachine>, ConcreteMachine>;
		Executor executor_;
		bool trans_ = false;

		void fill_pipeline(uint32_t pc) {
			if(pipeline_.interrupt_next()) return;
			advance_pipeline(pc);
			advance_pipeline(pc + 4);
		}

		uint32_t advance_pipeline(uint32_t pc) {
			uint32_t instruction = 0;	// Value should never be used; this avoids a spurious GCC warning.
			const bool did_read = executor_.bus.read(pc, instruction, trans_);
			return pipeline_.exchange(
				did_read ? instruction : Pipeline::SWI,
				did_read ? Pipeline::SWISubversion::None : Pipeline::SWISubversion::DataAbort);
		}

		struct Pipeline {
			enum SWISubversion: uint8_t {
				None,
				DataAbort,
				IRQ,
				FIQ,
			};

			static constexpr uint32_t SWI = 0xef'000000;

			uint32_t exchange(uint32_t next, SWISubversion subversion) {
				const uint32_t result = upcoming_[active_].opcode;
				latched_subversion_ = upcoming_[active_].subversion;

				upcoming_[active_].opcode = next;
				upcoming_[active_].subversion = subversion;
				active_ ^= 1;

				return result;
			}

			SWISubversion swi_subversion() const {
				return latched_subversion_;
			}

			// TODO: one day, possibly: schedule the subversion one slot further into the future
			// (i.e. active_ ^ 1) to allow one further instruction to occur as usual before the
			// action paplies. That is, if interrupts take effect one instruction later after a flags
			// change, which I don't yet know.
			//
			// In practice I got into a bit of a race condition between interrupt scheduling and
			// flags changes, so have backed off for now.
			void reschedule(SWISubversion subversion) {
				upcoming_[active_].opcode = SWI;
				upcoming_[active_].subversion = subversion;
			}

			bool interrupt_next() const {
				return upcoming_[active_].subversion == SWISubversion::IRQ || upcoming_[active_].subversion == SWISubversion::FIQ;
			}

		private:
			struct Stage {
				uint32_t opcode;
				SWISubversion subversion = SWISubversion::None;
			};
			Stage upcoming_[2];
			int active_ = 0;

			SWISubversion latched_subversion_;
		} pipeline_;

		// MARK: - Autoload, including cursor scripting.

		enum class AutoloadPhase {
			WaitingForStartup,
			OpeningDisk,
			WaitingForDiskContents,
			WaitingForTargetIcon,
			OpeningProgram,
			TestingMenu,
			Ended,
		};
		AutoloadPhase autoload_phase_ = AutoloadPhase::Ended;
		std::string target_program_;

		struct CursorAction {
			enum class Type {
				MoveTo,
				Button,
				Wait,
				SetPhase,
			} type;

			union {
				struct {
					int x, y;
				} move_to;
				struct {
					int duration;
				} wait;
				struct {
					int button;
					bool down;
				} button;
				struct {
					AutoloadPhase phase;
				} set_phase;
			} value;

			static CursorAction move_to(int x, int y) {
				CursorAction action;
				action.type = Type::MoveTo;
				action.value.move_to.x = x;
				action.value.move_to.y = y;
				return action;
			}
			static CursorAction wait(int duration) {
				CursorAction action;
				action.type = Type::Wait;
				action.value.wait.duration = duration;
				return action;
			}
			static CursorAction button(int button, bool down) {
				CursorAction action;
				action.type = Type::Button;
				action.value.button.button = button;
				action.value.button.down = down;
				return action;
			}
			static CursorAction set_phase(AutoloadPhase phase) {
				CursorAction action;
				action.type = Type::SetPhase;
				action.value.set_phase.phase = phase;
				return action;
			}
		};

		std::vector<CursorAction> cursor_actions_;

		struct CursorActionBuilder {
			CursorActionBuilder(std::vector<CursorAction> &actions) : actions_(actions) {}

			CursorActionBuilder &wait(int duration) {
				actions_.push_back(CursorAction::wait(duration));
				return *this;
			}

			CursorActionBuilder &move_to(int x, int y) {
				// Special case: if this sets a move_to when one is in progress,
				// just update the target.
				if(!actions_.empty() && actions_.back().type == CursorAction::Type::MoveTo) {
					actions_.back().value.move_to.x = x;
					actions_.back().value.move_to.y = y;
					return *this;
				}

				actions_.push_back(CursorAction::move_to(x, y));
				return *this;
			}

			CursorActionBuilder &click(int button) {
				actions_.push_back(CursorAction::button(button, true));
				actions_.push_back(CursorAction::wait(6'000'000));
				actions_.push_back(CursorAction::button(button, false));
				return *this;
			}

			CursorActionBuilder &double_click(int button) {
				actions_.push_back(CursorAction::button(button, true));
				actions_.push_back(CursorAction::wait(6'000'000));
				actions_.push_back(CursorAction::button(button, false));
				actions_.push_back(CursorAction::wait(6'000'000));
				actions_.push_back(CursorAction::button(button, true));
				actions_.push_back(CursorAction::wait(6'000'000));
				actions_.push_back(CursorAction::button(button, false));
				return *this;
			}

			CursorActionBuilder &set_phase(AutoloadPhase phase) {
				actions_.push_back(CursorAction::set_phase(phase));
				return *this;
			}

			std::vector<CursorAction> &actions_;
		};
		static constexpr int IconBarY = 240;
		static constexpr int IconBarProgramX = 532;
		static constexpr int IconBarDriveX = 32;

		std::vector<CursorAction> &begin();

		Cycles cursor_action_subcycle_;
		int cursor_action_waited_;
		int32_t target_window_[2];
};

}

using namespace Archimedes;

std::unique_ptr<Machine> Machine::Archimedes(
	const Analyser::Static::Target *target,
	const ROMMachine::ROMFetcher &rom_fetcher
) {
	const auto archimedes_target = dynamic_cast<const Analyser::Static::Acorn::ArchimedesTarget *>(target);
	return std::make_unique<ConcreteMachine>(*archimedes_target, rom_fetcher);
}
