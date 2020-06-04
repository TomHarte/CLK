#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>
#include "timer.h"
#include "ui_mainwindow.h"

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
		Q_OBJECT

		void createActions();

	public:
		MainWindow(QWidget *parent = nullptr);
		~MainWindow();

	private:
		std::unique_ptr<Ui::MainWindow> ui;
		std::unique_ptr<QTimer> qTimer;
		std::unique_ptr<QThread> timerThread;
		std::unique_ptr<Timer> timer;

		// Initial setup stuff.
		Analyser::Static::TargetList targets;
		enum class UIPhase {
			NoFileSelected, RequestingROMs, RunningMachine
		} uiPhase = UIPhase::NoFileSelected;
		void launchMachine();

		QString romRequestBaseText;
		std::vector<ROMMachine::ROM> missingRoms;

		// File drag and drop is supported.
		void dragEnterEvent(QDragEnterEvent* event) override;
		void dropEvent(QDropEvent* event) override;

		// Ongoing state.
		std::unique_ptr<Machine::DynamicMachine> machine;

	private slots:
		void open();
};

#endif // MAINWINDOW_H
