//
//  SWIIndex.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/05/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "SWIIndex.hpp"

using namespace Analyser::Static::Acorn;

const SWIDescription &Analyser::Static::Acorn::describe_swi(uint32_t comment) {
	static SWIDescription none;

	(void)comment;
	return none;
}


//			if(
//				executor_.pc() > 0x038021d0 &&
//					last_r1 != executor_.registers()[1]
//					 ||
//				(
//					last_link != executor_.registers()[14] ||
//					last_r0 != executor_.registers()[0] ||
//					last_r10 != executor_.registers()[10] ||
//					last_r1 != executor_.registers()[1]
//				)
//			) {
//				logger.info().append("%08x modified R14 to %08x; R0 to %08x; R10 to %08x; R1 to %08x",
//					last_pc,
//					executor_.registers()[14],
//					executor_.registers()[0],
//					executor_.registers()[10],
//					executor_.registers()[1]
//				);
//				logger.info().append("%08x modified R1 to %08x",
//					last_pc,
//					executor_.registers()[1]
//				);
//				last_link = executor_.registers()[14];
//				last_r0 = executor_.registers()[0];
//				last_r10 = executor_.registers()[10];
//				last_r1 = executor_.registers()[1];
//			}

//			if(instruction == 0xe8fd7fff) {
//				printf("At %08x [%d]; after last PC %08x and %zu ago was %08x\n",
//					address,
//					instr_count,
//					pc_history[(pc_history_ptr - 2 + pc_history.size()) % pc_history.size()],
//					pc_history.size(),
//					pc_history[pc_history_ptr]);
//			}
//			last_r9 = executor_.registers()[9];

//			log |= address == 0x038031c4;
//			log |= instr_count == 53552731 - 30;
//			log &= executor_.pc() != 0x000000a0;

//			log = (executor_.pc() == 0x038162afc) || (executor_.pc() == 0x03824b00);
//			log |= instruction & ;

		// The following has the effect of logging all taken SWIs and their return codes.
/*		if(
			(instruction & 0x0f00'0000) == 0x0f00'0000 &&
			executor.registers().test(InstructionSet::ARM::Condition(instruction >> 28))
		) {
			if(instruction & 0x2'0000) {
				swis.emplace_back();
				swis.back().count = swi_count++;
				swis.back().opcode = instruction;
				swis.back().address = executor.pc();
				swis.back().return_address = executor.registers().pc(4);
				for(int c = 0; c < 10; c++) swis.back().regs[c] = executor.registers()[uint32_t(c)];

				// Possibly capture more detail.
				//
				// Cf. http://productsdb.riscos.com/support/developers/prm_index/numswilist.html
				uint32_t pointer = 0;
				switch(instruction & 0xfd'ffff) {
					case 0x41501:
						swis.back().swi_name = "MessageTrans_OpenFile";

						// R0: pointer to file descriptor; R1: pointer to filename; R2: pointer to hold file data.
						// (R0 and R1 are in the RMA if R2 = 0)
						pointer = executor.registers()[1];
					break;
					case 0x41502:
						swis.back().swi_name = "MessageTrans_Lookup";
					break;
					case 0x41506:
						swis.back().swi_name = "MessageTrans_ErrorLookup";
					break;

					case 0x4028a:
						swis.back().swi_name = "Podule_EnumerateChunksWithInfo";
					break;

					case 0x4000a:
						swis.back().swi_name = "Econet_ReadLocalStationAndNet";
					break;
					case 0x4000e:
						swis.back().swi_name = "Econet_SetProtection";
					break;
					case 0x40015:
						swis.back().swi_name = "Econet_ClaimPort";
					break;

					case 0x40541:
						swis.back().swi_name = "FileCore_Create";
					break;

					case 0x80156:
					case 0x8015b:
						swis.back().swi_name = "PDriver_MiscOpForDriver";
					break;

					case 0x05:
						swis.back().swi_name = "OS_CLI";
						pointer = executor.registers()[0];
					break;
					case 0x0d:
						swis.back().swi_name = "OS_Find";
						if(executor.registers()[0] >= 0x40) {
							pointer = executor.registers()[1];
						}
					break;
					case 0x1d:
						swis.back().swi_name = "OS_Heap";
					break;
					case 0x1e:
						swis.back().swi_name = "OS_Module";
					break;

					case 0x20:
						swis.back().swi_name = "OS_Release";
					break;
					case 0x21:
						swis.back().swi_name = "OS_ReadUnsigned";
					break;
					case 0x23:
						swis.back().swi_name = "OS_ReadVarVal";

						// R0: pointer to variable name.
						pointer = executor.registers()[0];
					break;
					case 0x24:
						swis.back().swi_name = "OS_SetVarVal";

						// R0: pointer to variable name.
						pointer = executor.registers()[0];
					break;
					case 0x26:
						swis.back().swi_name = "OS_GSRead";
					break;
					case 0x27:
						swis.back().swi_name = "OS_GSTrans";
						pointer = executor.registers()[0];
					break;
					case 0x29:
						swis.back().swi_name = "OS_FSControl";
					break;
					case 0x2a:
						swis.back().swi_name = "OS_ChangeDynamicArea";
					break;

					case 0x4c:
						swis.back().swi_name = "OS_ReleaseDeviceVector";
					break;

					case 0x43057:
						swis.back().swi_name = "Territory_LowerCaseTable";
					break;
					case 0x43058:
						swis.back().swi_name = "Territory_UpperCaseTable";
					break;

					case 0x42fc0:
						swis.back().swi_name = "Portable_Speed";
					break;
					case 0x42fc1:
						swis.back().swi_name = "Portable_Control";
					break;
				}

				if(pointer) {
					while(true) {
						uint8_t next;
						executor.bus.template read<uint8_t>(pointer, next, InstructionSet::ARM::Mode::Supervisor, false);
						++pointer;

						if(next < 32) break;
						swis.back().value_name.push_back(static_cast<char>(next));
					}
				}

			}

			if(executor.registers().pc_status(0) & InstructionSet::ARM::ConditionCode::Overflow) {
				logger.error().append("SWI called with V set");
			}
		}
		if(!swis.empty() && executor.pc() == swis.back().return_address) {
			// Overflow set => SWI failure.
			auto &back = swis.back();
			if(executor.registers().pc_status(0) & InstructionSet::ARM::ConditionCode::Overflow) {
				auto info = logger.info();

				info.append("[%d] Failed swi ", back.count);
				if(back.swi_name.empty()) {
					info.append("&%x", back.opcode & 0xfd'ffff);
				} else {
					info.append("%s", back.swi_name.c_str());
				}

				if(!back.value_name.empty()) {
					info.append(" %s", back.value_name.c_str());
				}

				info.append(" @ %08x ", back.address);
				for(uint32_t c = 0; c < 10; c++) {
					info.append("r%d:%08x ", c, back.regs[c]);
				}
			}

			swis.pop_back();
		}*/
