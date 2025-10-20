//
//  6502.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/10/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace CPU::MOS6502Mk2 {

template <Model model, typename Traits>
void Processor<model, Traits>::restart_operation_fetch() {
	resume_point_ = ResumePoint::FetchDecode;
}

template <Model model, typename Traits>
void Processor<model, Traits>::run_for(const Cycles cycles) {
	cycles_ += cycles;
	if(cycles_ <= Cycles(0)) return;

	#define restore_point()	(__COUNTER__ + ResumePoint::Max)

	#define test_cycles(precision) 								\
		if constexpr (Traints::pause_precision >= precision) {	\
			static constexpr int location = restore_point();	\
			if(cycles_ <= Cycles(0)) {							\
				resume_point_ = location;						\
				return;											\
			}													\
																\
			[[fallthrough]] case location:						\
		}

	#define read(type, addr, value)	{						\
		static_assert(is_read(type));						\
		static constexpr int location = restore_point();	\
															\
		if(Traits::uses_ready_line && ready_is_active_) {	\
			return_from_ready_ = location;					\
			resume_point_ = ResumePoint::SpinReady;			\
			goto spin_ready;								\
		}													\
															\
		[[fallthrough]] case location:						\
		cycles_ -= bus_handler_.perform<type>(addr, value);	\
		test_cycles(PausePrecision::AnyCycle)				\
	}

	// TODO: test RDY if a suitable-advanced 6502.
	#define write(type, addr, value)	{					\
		static_assert(is_write(type));						\
		cycles_ -= bus_handler_.perform<type>(addr, value);	\
		test_cycles(PausePrecision::AnyCycle)				\
	}

//	while(true) switch(resume_point_) {
//		//
//		// Spin endlessly while RDY is active, repeating ready_address_ on the address bus.
//		//
//		spin_ready:
//			cycles_ -= bus_handler_.perform<BusOperation::Ready>(Address::Literal(ready_address_), Data::NoValue());
//			test_cycles(PausePrecision::AnyCycle);
//			if(ready_is_active_) {
//				goto spin_ready;
//			}
//
//			resume_point_ = return_from_ready_;
//			break;
//
//		//
//		// Grab the opcode and an initial byte of operand, then decode and segue into the proper
//		// addressing mode logic.
//		//
//		fetch_decode:
//		case ResumePoint::FetchDecode:
//
//			// Pause precision will always be at least operation by operation.
//			if(cycles_ <= Cycles(0)) {
//				resume_point_ = ResumePoint::FetchDecode;
//				return;
//			}
//
//			read(Address::Literal(pc_), opcode_);
//			++pc_;
//			read(Address::Literal(pc_), operand_);
//
//			// TODO: decode. Kind of important!
//	}

	#undef write
	#undef read
	#undef test_cycles
	#undef restore_point
}

}
