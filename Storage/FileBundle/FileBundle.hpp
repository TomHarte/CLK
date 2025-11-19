//
//  FileBundle.hpp
//  Clock Signal
//
//  Created by Thomas Harte on 19/11/2025.
//  Copyright © 2025 Thomas Harte. All rights reserved.
//

#pragma once

namespace Storage::FileBundle {

/*!
	A File Bundle is a collection of individual files, abstracted from whatever media they might
	be one.

	Initial motivation is allowing some machines direct local filesystem access. An attempt has
	been made to draft this in such a way as to allow it to do things like expose ZIP files as
	bundles in the future.
*/
struct FileBundle {


};


struct LocalFSFileBundle {

};

};
