#include "timer.h"

#include "../../ClockReceiver/TimeTypes.hpp"

#include <algorithm>
#include <QDebug>

Timer::Timer(QObject *parent) : QObject(parent) {}

void Timer::setMachine(MachineTypes::TimedMachine *machine) {
	this->machine = machine;
}

void Timer::tick() {
	const auto now = Time::nanos_now();
	const auto duration = std::min(now - lastTickNanos, int64_t(500'000));
	lastTickNanos = now;

	machine->run_for(double(duration) / 1e9);
}
