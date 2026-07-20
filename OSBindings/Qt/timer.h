#pragma once

#include <atomic>

#include <QObject>
#include <QThread>
#include <QTimer>

#include "../../ClockReceiver/TimeTypes.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"
#include "functionthread.h"

class Timer : public QObject
{
		Q_OBJECT

	public:
		explicit Timer(QObject *parent = nullptr);
		~Timer();

		void startWithMachine(MachineTypes::TimedMachine *machine, std::mutex *machineMutex);

	public slots:
		void tick();

	private:
		MachineTypes::TimedMachine *machine = nullptr;
		std::mutex *machineMutex = nullptr;
		int64_t lastTickNanos = Time::nanos_now();
		FunctionThread thread;
		std::unique_ptr<QTimer> timer;
};
