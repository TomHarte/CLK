#ifndef KEYBOARD_H
#define KEYBOARD_H

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
};

#endif // MAINWINDOW_H
