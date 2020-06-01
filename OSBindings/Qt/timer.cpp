#include "timer.h"

#include "../../ClockReceiver/TimeTypes.hpp"

#include <QDebug>

Timer::Timer(QObject *parent) : QObject(parent) {}

void Timer::tick() {
//	static int64_t last = 0;
//	const auto now = Time::nanos_now();
//	qDebug() << now - last;
//	last = now;
}
