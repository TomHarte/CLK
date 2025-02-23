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
		which is guaranteed lexically to match one used earlier for the creation of media that
		was either inserted or provided at machine construction, should be performed by the
		machine's owner.

		@c ChangeEffect::None means that no specific action will be taken;
		@c ChangeEffect::ReinsertMedia requests that the owner construct the applicable
			`Analyser::Static::Media` and call `insert_media`;
		@c ChangeEffect::RestartMachine requests that the owner reconsult the static analyer
			and construct a new machine to replace this one.
	*/
	virtual ChangeEffect effect_for_file_did_change([[maybe_unused]] const std::string &file_name) {
		return ChangeEffect::None;
	}
};

}
