#ifndef SETTINGS_H
#define SETTINGS_H

#include <QSettings>

class Settings: public QSettings {
	public:
		Settings() : QSettings("thomasharte", "Clock Signal") {}
};

#endif // SETTINGS_H
