#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <QMainWindow>


class KeyboardMapper {
	public:
		KeyboardMapper();
		std::optional<Inputs::Keyboard::Key> keyForEvent(QKeyEvent *);

	private:
};

#endif // MAINWINDOW_H
