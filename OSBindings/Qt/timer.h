#ifndef TIMER_H
#define TIMER_H

#include <atomic>
#include <QObject>

#include "../../Machines/Utility/MachineForTarget.hpp"

class Timer : public QObject
{
		Q_OBJECT

	public:
		explicit Timer(QObject *parent = nullptr);
		void setMachine(MachineTypes::TimedMachine *machine, std::mutex *machineMutex);

	public slots:
		void tick();

	private:
		MachineTypes::TimedMachine *machine = nullptr;
		std::mutex *machineMutex = nullptr;
		int64_t lastTickNanos = 0;
};

#endif // TIMER_H
