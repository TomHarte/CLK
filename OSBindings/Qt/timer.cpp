#include "timer.h"

#include "../../ClockReceiver/TimeTypes.hpp"

#include <algorithm>
#include <QDebug>

Timer::Timer(QObject *parent) : QObject(parent) {
	// Set up the emulation timer. Bluffer's guide: the QTimer will post an
	// event to an event loop. QThread is a thread with an event loop.
	// My class, Timer, will be wired up to receive the QTimer's events.
	timer = std::make_unique<QTimer>(this);
	timer->setInterval(1);

	thread = std::make_unique<QThread>(this);

	moveToThread(thread.get());
	connect(timer.get(), SIGNAL(timeout()), this, SLOT(tick()));
}

void Timer::setMachine(MachineTypes::TimedMachine *machine, std::mutex *machineMutex) {
	this->machine = machine;
	this->machineMutex = machineMutex;
}

void Timer::tick() {
	const auto now = Time::nanos_now();
	const auto duration = std::min(now - lastTickNanos, int64_t(500'000'000));
//	qDebug() << duration << " [not " << now - lastTickNanos << "]";
	lastTickNanos = now;

	std::lock_guard lock_guard(*machineMutex);
	machine->run_for(double(duration) / 1e9);
}

void Timer::start() {
	thread->start();
	timer->start();
}

Timer::~Timer() {
	// Stop the timer, then ask the QThread to exit and wait for it to do so.
	timer->stop();
	thread->exit();
	while(thread->isRunning());
}
