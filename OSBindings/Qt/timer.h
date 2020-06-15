#ifndef TIMER_H
#define TIMER_H

#include <atomic>

#include <QObject>
#include <QThread>
#include <QTimer>

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
		int64_t lastTickNanos = 0;
		FunctionThread thread;
		std::unique_ptr<QTimer> timer;
};

#endif // TIMER_H
