#ifndef FUNCTIONTHREAD_H
#define FUNCTIONTHREAD_H

#include <QThread>

/*!
 * \brief The LambdaThread class
 *
 * Provides a QThread which performs a supplied lambda before kicking off its event loop.
 *
 * Disclaimer: this might be a crutch that reveals a misunderstanding of the Qt
 * threading infrastructure. We'll see.
 */
class FunctionThread: public QThread {
	public:
		FunctionThread() : QThread() {}

		void setFunction(const std::function<void(void)> &function) {
			this->function = function;
		}

		void run() override {
			function();
			exec();
		}

		void stop() {
			QMetaObject::invokeMethod(this, "quit", Qt::QueuedConnection);
			while(isRunning());
		}

	private:
		std::function<void(void)> function;
};

#endif // FUNCTIONTHREAD_H
