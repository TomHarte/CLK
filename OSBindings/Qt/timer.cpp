#include "timer.h"

#include "../../ClockReceiver/TimeTypes.hpp"

#include <algorithm>
#include <QDebug>

Timer::Timer(QObject *parent) : QObject(parent) {}

void Timer::setMachine(MachineTypes::TimedMachine *machine, std::mutex *machineMutex) {
	this->machine = machine;
	this->machineMutex = machineMutex;
}

void Timer::tick() {
	const auto now = Time::nanos_now();
	const auto duration = std::min(now - lastTickNanos, int64_t(500'000));
	lastTickNanos = now;

	std::lock_guard lock_guard(*machineMutex);
	machine->run_for(double(duration) / 1e9);
}
