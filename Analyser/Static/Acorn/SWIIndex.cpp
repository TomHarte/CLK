//
//  SWIIndex.cpp
//  Clock Signal
//
//  Created by Thomas Harte on 05/05/2024.
//  Copyright © 2024 Thomas Harte. All rights reserved.
//

#include "SWIIndex.hpp"

#include <cstdio>
#include <set>

using namespace Analyser::Static::Acorn;

SWIDescription::SWIDescription(uint32_t comment) {
	chunk_offset = comment & 0b111111;
	chunk_number = (comment >> 6) & 0b11111111111;
	error_flag = comment & (1 << 17);
	swi_group = SWIGroup((comment >> 18) & 0b11);
	os_flag = (comment >> 20) & 0b1111;

	static std::set<uint32_t> encountered;

	const uint32_t number = comment & uint32_t(~(1 << 17));
	switch(number) {
		case 0x00:
			name = "OS_WriteC";
			registers[0].type = Register::Type::Character;
		break;
		case 0x01:
			name = "OS_WriteS";
			registers[0].type = Register::Type::FollowingString;
		break;
		case 0x02:
			name = "OS_Write0";
			registers[0].type = Register::Type::PointerToString;
		break;
		case 0x03:
			name = "OS_NewLine";
		break;
		case 0x04:
			name = "OS_ReadC";
		break;
		case 0x05:
			name = "OS_CLI";
			registers[0].type = Register::Type::PointerToString;
		break;
		case 0x06:
			name = "OS_Byte";
			registers[0].type = Register::Type::ReasonCode;
			registers[1].type = Register::Type::ReasonCodeDependent;
			registers[2].type = Register::Type::ReasonCodeDependent;
		break;
		case 0x07:
			name = "OS_Word";
			registers[0].type = Register::Type::ReasonCode;
			registers[1].type = Register::Type::Pointer;
		break;
		case 0x08:
			name = "OS_File";
			registers[0].type = Register::Type::ReasonCode;
		break;
		case 0x09:
			name = "OS_Args";
			registers[0].type = Register::Type::ReasonCode;
			registers[1].type = Register::Type::Pointer;
			registers[2].type = Register::Type::ReasonCodeDependent;
		break;
		case 0x0c:
			name = "OS_GBPB";
			registers[0].type = Register::Type::ReasonCode;
		break;
		case 0x0d:
			name = "OS_Find";
			registers[0].type = Register::Type::ReasonCode;
		break;
		case 0x0f:
			name = "OS_Control";
			registers[0].type = Register::Type::Pointer;
			registers[1].type = Register::Type::Pointer;
			registers[2].type = Register::Type::Pointer;
			registers[3].type = Register::Type::Pointer;
		break;
		case 0x1d:
			name = "OS_Heap";
			registers[0].type = Register::Type::ReasonCode;
			registers[1].type = Register::Type::Pointer;
			registers[2].type = Register::Type::Pointer;
			registers[3].type = Register::Type::ReasonCodeDependent;
		break;
		case 0x3a:
			name = "OS_ValidateAddress";
			registers[0].type = Register::Type::Pointer;
			registers[1].type = Register::Type::Pointer;
		break;

		case 0x400e2:
			name = "Wimp_PlotIcon";
			registers[1].type = Register::Type::Pointer;
		break;


		default:
			if(encountered.find(number) == encountered.end()) {
				encountered.insert(number);
				printf("SWI: %08x\n", number);
			}
		break;
	}
}



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

	
		}*/
