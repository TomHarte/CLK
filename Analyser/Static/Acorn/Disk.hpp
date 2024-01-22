//
//  Disk.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 18/09/2016.
//  Copyright 2016 Thomas Harte. All rights reserved.
//

#pragma once

#include "File.hpp"
#include "../../../Storage/Disk/Disk.hpp"

namespace Analyser::Static::Acorn {

/// Describes a DFS- or ADFS-format catalogue(/directory): the list of files available and the catalogue's boot option.
struct Catalogue {
	std::string name;
	std::vector<File> files;
	enum class BootOption {
		None,
		LoadBOOT,
		RunBOOT,
		ExecBOOT
	} bootOption;
};

std::unique_ptr<Catalogue> GetDFSCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk);
std::unique_ptr<Catalogue> GetADFSCatalogue(const std::shared_ptr<Storage::Disk::Disk> &disk);

}
