#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAudioOutput>
#include <QMainWindow>

#include <memory>
#include "audiobuffer.h"
#include "timer.h"
#include "ui_mainwindow.h"
#include "functionthread.h"

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow, public Outputs::Speaker::Speaker::Delegate {
		Q_OBJECT

		void createActions();

	public:
		MainWindow(QWidget *parent = nullptr);
		~MainWindow();

	protected:
		bool eventFilter(QObject *obj, QEvent *event) override;

	private:
		std::unique_ptr<Ui::MainWindow> ui;
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
		std::mutex machineMutex;

		std::unique_ptr<QAudioOutput> audioOutput;
		bool audioIs8bit = false, audioIsStereo = false;
		void speaker_did_complete_samples(Outputs::Speaker::Speaker *speaker, const std::vector<int16_t> &buffer) override;
		AudioBuffer audioBuffer;
		FunctionThread audioThread;

		bool processEvent(QKeyEvent *);

		enum class WidgetSet {
			MachinePicker,
			ROMRequester,
			RunningMachine,
		};
		void setVisibleWidgetSet(WidgetSet);

	private slots:
		void open();
		void newFile();
		void about();
};

#endif // MAINWINDOW_H
