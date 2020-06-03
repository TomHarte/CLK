#ifndef TIMER_H
#define TIMER_H

#include <QObject>

#include "../../Machines/Utility/MachineForTarget.hpp"

class Timer : public QObject
{
		Q_OBJECT

	public:
		explicit Timer(QObject *parent = nullptr);
		void setMachine(MachineTypes::TimedMachine *machine);

	public slots:
		void tick();

	private:
		MachineTypes::TimedMachine *machine = nullptr;
		int64_t lastTickNanos = 0;
};

#endif // TIMER_H
