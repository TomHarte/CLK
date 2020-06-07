#ifndef TIMER_H
#define TIMER_H

#include <atomic>

#include <QObject>
#include <QThread>
#include <QTimer>

#include "../../Machines/Utility/MachineForTarget.hpp"

class Timer : public QObject
{
		Q_OBJECT

	public:
		explicit Timer(QObject *parent = nullptr);
		~Timer();

		void setMachine(MachineTypes::TimedMachine *machine, std::mutex *machineMutex);
		void start();

	public slots:
		void tick();

	private:
		MachineTypes::TimedMachine *machine = nullptr;
		std::mutex *machineMutex = nullptr;
		int64_t lastTickNanos = 0;
		std::unique_ptr<QThread> thread;
		std::unique_ptr<QTimer> timer;
};

#endif // TIMER_H
