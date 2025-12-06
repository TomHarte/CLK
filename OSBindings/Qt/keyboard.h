#pragma once

#include <QKeyEvent>
#include <map>
#include <optional>
#include "../../Inputs/Keyboard.hpp"

class KeyboardMapper {
	public:
		KeyboardMapper();
		std::optional<Inputs::Keyboard::Key> keyForEvent(QKeyEvent *);

	private:
		std::map<quint32, Inputs::Keyboard::Key> keyByKeySym;
		bool is_x11_ = false;
};
