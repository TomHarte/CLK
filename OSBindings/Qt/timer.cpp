#include "timer.h"

#include "../../ClockReceiver/TimeTypes.hpp"

#include <algorithm>
#include <QDebug>

Timer::Timer(QObject *parent) : QObject(parent) {}

void Timer::startWithMachine(MachineTypes::TimedMachine *machine, std::mutex *machineMutex) {
	this->machine = machine;
	this->machineMutex = machineMutex;

	thread.performAsync([this] {
		// Set up the emulation timer. Bluffer's guide: the QTimer will post an
		// event to an event loop. QThread is a thread with an event loop.
		// My class, Timer, will be wired up to receive the QTimer's events.
		timer = std::make_unique<QTimer>();
		timer->setInterval(1);

		connect(timer.get(), &QTimer::timeout, this, &Timer::tick, Qt::DirectConnection);
		timer->start();
	});
}

void Timer::tick() {
	const auto now = Time::nanos_now();
	const auto duration = std::min(now - lastTickNanos, int64_t(500'000'000));
	lastTickNanos = now;

	std::lock_guard lock_guard(*machineMutex);
	machine->run_for(double(duration) / 1e9);
}

Timer::~Timer() {
	thread.performAsync([this] {
		if(timer) {
			timer->stop();
		}
	});
	thread.stop();
}
