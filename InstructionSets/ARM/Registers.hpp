//
//  Status.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 25/02/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#pragma once

#include "OperationMapper.hpp"

#include <array>
#include <cstdint>

namespace InstructionSet::ARM {

namespace ConditionCode {

static constexpr uint32_t Negative		= 1u << 31;
static constexpr uint32_t Zero			= 1u << 30;
static constexpr uint32_t Carry			= 1u << 29;
static constexpr uint32_t Overflow		= 1u << 28;
static constexpr uint32_t IRQDisable	= 1u << 27;
static constexpr uint32_t FIQDisable	= 1u << 26;
static constexpr uint32_t Mode			= (1u << 1) | (1u << 0);

static constexpr uint32_t Address		= FIQDisable - Mode - 1;

}

enum class Mode {
	User = 0b00,
	FIQ = 0b01,
	IRQ = 0b10,
	Supervisor = 0b11,
};

/// Combines the ARM registers and status flags into a single whole, given that the architecture
/// doesn't have the same degree of separation as others.
///
/// The PC contained here is always taken to be **the address of the current instruction + 4**,
/// i.e. whatever should be executed next, disregarding pipeline differences.
///
/// Appropriate prefetch offsets are left to other code to handle.
/// This is to try to keep this structure independent of a specific ARM implementation.
struct Registers {
	public:
		// Don't allow copying.
		Registers(const Registers &) = delete;
		Registers &operator =(const Registers &) = delete;
		Registers() = default;

		/// Sets the N and Z flags according to the value of @c result.
		void set_nz(uint32_t value) {
			zero_result_ = negative_flag_ = value;
		}

		/// Sets C if @c value is non-zero; resets it otherwise.
		void set_c(uint32_t value) {
			carry_flag_ = value;
		}

		/// @returns @c 1 if carry is set; @c 0 otherwise.
		uint32_t c() const {
			return carry_flag_ ? 1 : 0;
		}

		/// Sets V if the highest bit of @c value is set; resets it otherwise.
		void set_v(uint32_t value) {
			overflow_flag_ = value;
		}

		/// @returns The current status bits, separate from the PC — mode, NVCZ and the two interrupt flags.
		uint32_t status() const {
			return
				uint32_t(mode_) |
				(negative_flag_ & ConditionCode::Negative) |
				(zero_result_ ? 0 : ConditionCode::Zero) |
				(carry_flag_ ? ConditionCode::Carry : 0) |
				((overflow_flag_ >> 3) & ConditionCode::Overflow) |
				interrupt_flags_;
		}

		/// @returns The full PC + status bits.
		uint32_t pc_status(uint32_t offset) const {
			return
				((active_[15] + offset) & ConditionCode::Address) |
				status();
		}

		/// Sets status bits only, subject to mode.
		void set_status(uint32_t status) {
			// ... in user mode the other flags (I, F, M1, M0) are protected from direct change
			// but in non-user modes these will also be affected, accepting copies of bits 27, 26,
			// 1 and 0 of the result respectively.

			negative_flag_ = status;
			overflow_flag_ = status << 3;
			carry_flag_ = status & ConditionCode::Carry;
			zero_result_ = ~status & ConditionCode::Zero;

			if(mode_ != Mode::User) {
				set_mode(Mode(status & 3));
				interrupt_flags_ = status & (ConditionCode::IRQDisable | ConditionCode::FIQDisable);
			}
		}

		/// @returns The current mode.
		Mode mode() const {
			return mode_;
		}

		/// Sets a new PC.
		void set_pc(uint32_t value) {
			active_[15] = value & ConditionCode::Address;
		}

		/// @returns The stored PC plus @c offset limited to 26 bits.
		uint32_t pc(uint32_t offset) const {
			return (active_[15] + offset) & ConditionCode::Address;
		}

		// MARK: - Exceptions.

		enum class Exception {
			/// Reset line went from high to low.
			Reset = 0x00,
			/// Either an undefined instruction or a coprocessor instruction for which no coprocessor was found.
			UndefinedInstruction = 0x04,
			/// Code executed a software interrupt.
			SoftwareInterrupt = 0x08,
			/// The memory subsystem indicated an abort during prefetch and that instruction has now come to the head of the queue.
			PrefetchAbort = 0x0c,
			/// The memory subsystem indicated an abort during an instruction; if it is an LDR or STR then this should be signalled
			/// before any instruction execution. If it was an LDM then loading stops upon a data abort but both an LDM and STM
			/// otherwise complete, including pointer writeback.
			DataAbort = 0x10,
			/// The first data transfer attempted within an instruction was above address 0x3ff'ffff.
			Address = 0x14,
			/// The IRQ line was low at the end of an instruction and ConditionCode::IRQDisable was not set.
			IRQ = 0x18,
			/// The FIQ went low at least one cycle ago and ConditionCode::FIQDisable was not set.
			FIQ = 0x1c,
		};
		static constexpr uint32_t pc_offset_during(Exception exception) {
			// The below is somewhat convoluted by the assumed execution model:
			//
			//	*	exceptions occuring during execution of an instruction are taken
			//		to occur after R15 has already been incremented by 4; but
			//	*	exceptions occurring instead of execution of an instruction are
			//		taken to occur with R15 pointing to an instruction that hasn't begun.
			//
			// i.e. in net R15 always refers to the next instruction
			// that has not yet started.
			switch(exception) {
				// "To return normally from FIQ use SUBS PC, R14_fiq, #4".
				case Exception::FIQ:					return 4;

				// "To return normally from IRQ use SUBS PC, R14_irq, #4".
				case Exception::IRQ:					return 4;

				// "If a return is required from [address exception traps], use
				// SUBS PC, R14_svc, #4. This will return to the instruction after
				// the one causing the trap".
				case Exception::Address:				return 4;

				// "A Data Abort requires [work before a return], the return being
				// done by SUBS PC, R14_svc, #8".
				case Exception::DataAbort:				return 8;

				// "To continue after a Prefetch Abort use SUBS PC, R14_svc #4".
				case Exception::PrefetchAbort:			return 4;

				// "To return from a SWI, use MOVS PC, R14_svc. This returns to the instruction
				// following the SWI".
				case Exception::SoftwareInterrupt:		return 0;

				// "To return from [an undefined instruction trap] use MOVS PC, R14_svc.
				// This returns to the instruction following the undefined instruction".
				case Exception::UndefinedInstruction:	return 0;

				// Unspecified; a guess.
				case Exception::Reset:					return 0;
			}
		}

		/// Updates the program counter, interupt flags and link register if applicable to begin @c exception.
		template <Exception type>
		void exception() {
			const auto r14 = pc_status(pc_offset_during(type));
			switch(type) {
				case Exception::IRQ:	set_mode(Mode::IRQ);		break;
				case Exception::FIQ: 	set_mode(Mode::FIQ);		break;
				default:				set_mode(Mode::Supervisor);	break;
			}
			active_[14] = r14;

			interrupt_flags_ |= ConditionCode::IRQDisable;
			if constexpr (type == Exception::Reset || type == Exception::FIQ) {
				interrupt_flags_ |= ConditionCode::FIQDisable;
			}
			set_pc(uint32_t(type));
		}

		/// Applies an exception of @c type and returns @c true if: (i) it is IRQ or FIQ; (ii) the processor is currently accepting such interrupts.
		/// Otherwise returns @c false.
		template <Exception type>
		bool interrupt() {
			switch(type) {
				case Exception::IRQ:
					if(interrupt_flags_ & ConditionCode::IRQDisable) {
						return false;
					}
				break;

				case Exception::FIQ:
					if(interrupt_flags_ & ConditionCode::FIQDisable) {
						return false;
					}
				break;

				default: return false;
			}

			exception<type>();
			return true;
		}

		// MARK: - Condition tests.

		/// @returns @c true if @c condition tests as true; @c false otherwise.
		bool test(Condition condition) {
			const auto ne = [&]() -> bool {
				return zero_result_;
			};
			const auto cs = [&]() -> bool {
				return carry_flag_;
			};
			const auto mi = [&]() -> bool {
				return negative_flag_ & ConditionCode::Negative;
			};
			const auto vs = [&]() -> bool {
				return overflow_flag_ & ConditionCode::Negative;
			};
			const auto hi = [&]() -> bool {
				return carry_flag_ && zero_result_;
			};
			const auto lt = [&]() -> bool {
				return (negative_flag_ ^ overflow_flag_) & ConditionCode::Negative;
			};
			const auto le = [&]() -> bool {
				return !zero_result_ || lt();
			};

			switch(condition) {
				case Condition::EQ:	return !ne();
				case Condition::NE:	return ne();
				case Condition::CS:	return cs();
				case Condition::CC:	return !cs();
				case Condition::MI:	return mi();
				case Condition::PL:	return !mi();
				case Condition::VS:	return vs();
				case Condition::VC:	return !vs();

				case Condition::HI:	return hi();
				case Condition::LS:	return !hi();
				case Condition::GE:	return !lt();
				case Condition::LT:	return lt();
				case Condition::GT:	return !le();
				case Condition::LE:	return le();

				case Condition::AL:	return true;
				case Condition::NV:	return false;
			}
		}

		/// Sets current execution mode.
		void set_mode(Mode target_mode) {
			if(mode_ == target_mode) {
				return;
			}

			// For outgoing modes other than FIQ, only save the final two registers for now;
			// if the incoming mode is FIQ then the other five will be saved in the next switch.
			// For FIQ, save all seven up front.
			switch(mode_) {
				// FIQ outgoing: save R8 to R14.
				case Mode::FIQ:
					std::copy(active_.begin() + 8, active_.begin() + 15, fiq_registers_.begin());
				break;

				// Non-FIQ outgoing: save R13 and R14. If saving to the user registers,
				// use only the final two slots.
				case Mode::User:
					std::copy(active_.begin() + 13, active_.begin() + 15, user_registers_.begin() + 5);
				break;
				case Mode::Supervisor:
					std::copy(active_.begin() + 13, active_.begin() + 15, supervisor_registers_.begin());
				break;
				case Mode::IRQ:
					std::copy(active_.begin() + 13, active_.begin() + 15, irq_registers_.begin());
				break;
			}

			// For all modes except FIQ: restore the final two registers to their appropriate values.
			// For FIQ: save an additional five, then overwrite seven.
			switch(target_mode) {
				case Mode::FIQ:
					// FIQ is incoming, so save registers 8 to 12 to the first five slots of the user registers.
					std::copy(active_.begin() + 8, active_.begin() + 13, user_registers_.begin());

					// Replace R8 to R14.
					std::copy(fiq_registers_.begin(), fiq_registers_.end(), active_.begin() + 8);
				break;
				case Mode::User:
					std::copy(user_registers_.begin() + 5, user_registers_.end(), active_.begin() + 13);
				break;
				case Mode::Supervisor:
					std::copy(supervisor_registers_.begin(), supervisor_registers_.end(), active_.begin() + 13);
				break;
				case Mode::IRQ:
					std::copy(irq_registers_.begin(), irq_registers_.end(), active_.begin() + 13);
				break;
			}

			// If FIQ is outgoing then there's another five registers to restore.
			if(mode_ == Mode::FIQ) {
				std::copy(user_registers_.begin(), user_registers_.begin() + 5, active_.begin() + 8);
			}

			mode_ = target_mode;
		}

		uint32_t &operator[](uint32_t offset) {
			return active_[static_cast<size_t>(offset)];
		}

		uint32_t operator[](uint32_t offset) const {
			return active_[static_cast<size_t>(offset)];
		}

	private:
		Mode mode_ = Mode::Supervisor;

		uint32_t zero_result_ = 1;
		uint32_t negative_flag_ = 0;
		uint32_t interrupt_flags_ = ConditionCode::IRQDisable | ConditionCode::FIQDisable;
		uint32_t carry_flag_ = 0;
		uint32_t overflow_flag_ = 0;

		// Various shadow registers.
		std::array<uint32_t, 7> user_registers_{};
		std::array<uint32_t, 7> fiq_registers_{};
		std::array<uint32_t, 2> irq_registers_{};
		std::array<uint32_t, 2> supervisor_registers_{};

		// The active register set.
		std::array<uint32_t, 16> active_{};
};

}
