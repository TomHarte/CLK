//
//  MediaTarget.h
//  Clock Signal
//
//  Created by Thomas Harte on 08/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "../Analyser/Static/StaticAnalyser.hpp"
#include "../Configurable/Configurable.hpp"

#include <string>

namespace MachineTypes {

/*!
	A MediaTarget::Machine is anything that can accept new media while running.
*/
struct MediaTarget {
	/*!
		Requests that the machine insert @c media as a modification to current state.

		@returns @c true if any media was inserted; @c false otherwise.
	*/
	virtual bool insert_media(const Analyser::Static::Media &) = 0;

	enum class ChangeEffect {
		None,
		ReinsertMedia,
		RestartMachine
	};
	/*!
		Queries what action an observed on-disk change in the file with name `file_name`,
		which is guaranteed lexically to match one used earlier with this machine, should be
		performed by the machine's owner.

		It is guaranteed by the caller that the underlying bytes of the file have changed; the caller
		is not required to differentiate changes made by Clock Signal itself from those made
		external to it.

		@c ChangeEffect::None means that no specific action will be taken;
		@c ChangeEffect::ReinsertMedia requests that the owner construct the applicable
			`Analyser::Static::Media` and call `insert_media`;
		@c ChangeEffect::RestartMachine requests that the owner reconsult the static analyer
			and construct a new machine to replace this one.

		In general:
			* if the machine itself has recently modified the file, `::None` is appropriate;
			* if the machine has not recently modified the file, quite often obviously so because the
			file is a ROM or something else that is never modified, then ::ReinsertMedia or ::RestartMachine
			might be appropriate depending on whether it is more likely that execution will continue correctly
			with a simple media swap or whether this implies that previous state should be completely discarded.
	*/
	virtual ChangeEffect effect_for_file_did_change([[maybe_unused]] const std::string &file_name) {
		return ChangeEffect::None;
	}
};

}
