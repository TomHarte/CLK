//
//  Registry.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 03/05/2026.
//  Copyright © 2026 Thomas Harte. All rights reserved.
//

#pragma once

// Options and machines.
#include "Machines/Acorn/Archimedes/Archimedes.hpp"
#include "Machines/Acorn/BBCMicro/BBCMicro.hpp"
#include "Machines/Acorn/Electron/Electron.hpp"
#include "Machines/Amiga/Amiga.hpp"
#include "Machines/AmstradCPC/AmstradCPC.hpp"
#include "Machines/Apple/AppleII/AppleII.hpp"
#include "Machines/Apple/AppleIIgs/AppleIIgs.hpp"
#include "Machines/Apple/Macintosh/Macintosh.hpp"
#include "Machines/Atari/2600/Atari2600.hpp"
#include "Machines/Atari/ST/AtariST.hpp"
#include "Machines/ColecoVision/ColecoVision.hpp"
#include "Machines/Commodore/Plus4/Plus4.hpp"
#include "Machines/Commodore/Vic-20/Vic20.hpp"
#include "Machines/Enterprise/Enterprise.hpp"
#include "Machines/MasterSystem/MasterSystem.hpp"
#include "Machines/MSX/MSX.hpp"
#include "Machines/Oric/Oric.hpp"
#include "Machines/PCCompatible/PCCompatible.hpp"
#include "Machines/Tandy/CoCo/CoCo.hpp"
#include "Machines/Thomson/MO/MO.hpp"
#include "Machines/Sinclair/ZX8081/ZX8081.hpp"
#include "Machines/Sinclair/ZXSpectrum/ZXSpectrum.hpp"

// Targets.
#include "Analyser/Static/Acorn/Target.hpp"
#include "Analyser/Static/Amiga/Target.hpp"
#include "Analyser/Static/AmstradCPC/Target.hpp"
#include "Analyser/Static/AppleII/Target.hpp"
#include "Analyser/Static/AppleIIgs/Target.hpp"
#include "Analyser/Static/Atari2600/Target.hpp"
#include "Analyser/Static/AtariST/Target.hpp"
#include "Analyser/Static/Commodore/Target.hpp"
#include "Analyser/Static/Enterprise/Target.hpp"
#include "Analyser/Static/Macintosh/Target.hpp"
#include "Analyser/Static/MSX/Target.hpp"
#include "Analyser/Static/Oric/Target.hpp"
#include "Analyser/Static/PCCompatible/Target.hpp"
#include "Analyser/Static/Sega/Target.hpp"
#include "Analyser/Static/TandyCoCo/Target.hpp"
#include "Analyser/Static/Thomson/Target.hpp"
#include "Analyser/Static/ZX8081/Target.hpp"
#include "Analyser/Static/ZXSpectrum/Target.hpp"

namespace MachineRegister {

template <Analyser::Machine machine> struct Details;

template <>
struct Details<Analyser::Machine::Amiga> {
	static constexpr auto name = Analyser::Machine::Amiga;
	static constexpr bool requires_media = false;

	using Machine = ::Amiga::Machine;
	using Target = Analyser::Static::Amiga::Target;
};

template <>
struct Details<Analyser::Machine::AmstradCPC> {
	static constexpr auto name = Analyser::Machine::AmstradCPC;
	static constexpr bool requires_media = false;

	using Machine = ::AmstradCPC::Machine;
	using Target = Analyser::Static::AmstradCPC::Target;
};

template <>
struct Details<Analyser::Machine::AppleII> {
	static constexpr auto name = Analyser::Machine::AppleII;
	static constexpr bool requires_media = false;

	using Machine = Apple::II::Machine;
	using Target = Analyser::Static::AppleII::Target;
};

template <>
struct Details<Analyser::Machine::AppleIIgs> {
	static constexpr auto name = Analyser::Machine::AppleIIgs;
	static constexpr bool requires_media = false;
	static constexpr bool is_incomplete = true;

	using Machine = Apple::IIgs::Machine;
	using Target = Analyser::Static::AppleIIgs::Target;
};

template <>
struct Details<Analyser::Machine::Archimedes> {
	static constexpr auto name = Analyser::Machine::Archimedes;
	static constexpr bool requires_media = false;

	using Machine = ::Archimedes::Machine;
	using Target = Analyser::Static::Acorn::ArchimedesTarget;
};

template <>
struct Details<Analyser::Machine::AtariST> {
	static constexpr auto name = Analyser::Machine::AtariST;
	static constexpr bool requires_media = false;

	using Machine = Atari::ST::Machine;
	using Target = Analyser::Static::AtariST::Target;
};

template <>
struct Details<Analyser::Machine::Atari2600> {
	static constexpr auto name = Analyser::Machine::Atari2600;
	static constexpr bool requires_media = true;

	using Machine = ::Atari2600::Machine;
	using Target = Analyser::Static::Atari2600::Target;
};

template <>
struct Details<Analyser::Machine::BBCMicro> {
	static constexpr auto name = Analyser::Machine::BBCMicro;
	static constexpr bool requires_media = false;

	using Machine = ::BBCMicro::Machine;
	using Target = Analyser::Static::Acorn::BBCMicroTarget;
};

template <>
struct Details<Analyser::Machine::ColecoVision> {
	static constexpr auto name = Analyser::Machine::ColecoVision;
	static constexpr bool requires_media = true;

	using Machine = Coleco::Vision::Machine;
};

template <>
struct Details<Analyser::Machine::Electron> {
	static constexpr auto name = Analyser::Machine::Electron;
	static constexpr bool requires_media = false;

	using Machine = ::Electron::Machine;
	using Target = Analyser::Static::Acorn::ElectronTarget;
};

template <>
struct Details<Analyser::Machine::Enterprise> {
	static constexpr auto name = Analyser::Machine::Enterprise;
	static constexpr bool requires_media = false;

	using Machine = ::Enterprise::Machine;
	using Target = Analyser::Static::Enterprise::Target;
};

template <>
struct Details<Analyser::Machine::Macintosh> {
	static constexpr auto name = Analyser::Machine::Macintosh;
	static constexpr bool requires_media = false;

	using Machine = Apple::Macintosh::Machine;
	using Target = Analyser::Static::Macintosh::Target;
};

template <>
struct Details<Analyser::Machine::MasterSystem> {
	static constexpr auto name = Analyser::Machine::MasterSystem;
	static constexpr bool requires_media = true;

	using Machine = Sega::MasterSystem::Machine;
	using Target = Analyser::Static::Sega::Target;
};

template <>
struct Details<Analyser::Machine::MSX> {
	static constexpr auto name = Analyser::Machine::MSX;
	static constexpr bool requires_media = false;

	using Machine = Sega::MasterSystem::Machine;
	using Target = Analyser::Static::MSX::Target;
};

template <>
struct Details<Analyser::Machine::Oric> {
	static constexpr auto name = Analyser::Machine::Oric;
	static constexpr bool requires_media = false;

	using Machine = ::Oric::Machine;
	using Target = Analyser::Static::Oric::Target;
};

template <>
struct Details<Analyser::Machine::PCCompatible> {
	static constexpr auto name = Analyser::Machine::PCCompatible;
	static constexpr bool requires_media = false;

	using Machine = ::PCCompatible::Machine;
	using Target = Analyser::Static::PCCompatible::Target;
};

template <>
struct Details<Analyser::Machine::Plus4> {
	static constexpr auto name = Analyser::Machine::Plus4;
	static constexpr bool requires_media = false;

	using Machine = Commodore::Plus4::Machine;
	using Target = Analyser::Static::Commodore::Plus4Target;
};

template <>
struct Details<Analyser::Machine::TandyCoCo> {
	static constexpr auto name = Analyser::Machine::TandyCoCo;
	static constexpr bool requires_media = false;
	static constexpr bool is_incomplete = true;

	using Machine = Tandy::CoCo::Machine;
	using Target = Analyser::Static::TandyCoCo::Target;
};

template <>
struct Details<Analyser::Machine::ThomsonMO> {
	static constexpr auto name = Analyser::Machine::ThomsonMO;
	static constexpr bool requires_media = false;

	using Machine = Thomson::MO::Machine;
	using Target = Analyser::Static::Thomson::MOTarget;
};

template <>
struct Details<Analyser::Machine::Vic20> {
	static constexpr auto name = Analyser::Machine::Vic20;
	static constexpr bool requires_media = false;

	using Machine = Commodore::Vic20::Machine;
	using Target = Analyser::Static::Commodore::Vic20Target;
};

template <>
struct Details<Analyser::Machine::ZX8081> {
	static constexpr auto name = Analyser::Machine::ZX8081;
	static constexpr bool requires_media = false;

	using Machine = Sinclair::ZX8081::Machine;
	using Target = Analyser::Static::ZX8081::Target;
};

template <>
struct Details<Analyser::Machine::ZXSpectrum> {
	static constexpr auto name = Analyser::Machine::ZXSpectrum;
	static constexpr bool requires_media = false;

	using Machine = Sinclair::ZXSpectrum::Machine;
	using Target = Analyser::Static::ZXSpectrum::Target;
};


template <template<typename> typename FuncT, Analyser::Machine machine, typename TargetT>
void for_machine(TargetT &target) {
	if constexpr (requires{ TargetT::is_incomplete; }) {
		if(TargetT::is_incomplete) {
			return;
		}
	}
	FuncT<Details<machine>>()(target);
}

template <template<typename> typename FuncT, typename TargetT>
void for_all_machines(TargetT &target) {
	using enum Analyser::Machine;

	for_machine<FuncT, Amiga>(target);
	for_machine<FuncT, AmstradCPC>(target);
	for_machine<FuncT, AppleII>(target);
	for_machine<FuncT, AppleIIgs>(target);
	for_machine<FuncT, Archimedes>(target);
	for_machine<FuncT, AtariST>(target);
	for_machine<FuncT, Atari2600>(target);
	for_machine<FuncT, BBCMicro>(target);
	for_machine<FuncT, ColecoVision>(target);
	for_machine<FuncT, Electron>(target);
	for_machine<FuncT, Enterprise>(target);
	for_machine<FuncT, Macintosh>(target);
	for_machine<FuncT, MasterSystem>(target);
	for_machine<FuncT, MSX>(target);
	for_machine<FuncT, Oric>(target);
	for_machine<FuncT, PCCompatible>(target);
	for_machine<FuncT, Plus4>(target);
	for_machine<FuncT, TandyCoCo>(target);
	for_machine<FuncT, ThomsonMO>(target);
	for_machine<FuncT, Vic20>(target);
	for_machine<FuncT, ZX8081>(target);
	for_machine<FuncT, ZXSpectrum>(target);
}
}
