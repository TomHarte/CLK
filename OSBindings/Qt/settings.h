#pragma once

#include <QSettings>

class Settings: public QSettings {
	public:
		Settings() : QSettings("thomasharte", "Clock Signal") {}
};
