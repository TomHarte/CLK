#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QAudioOutput>
#include <QMainWindow>

#include <memory>
#include <mutex>
#include <optional>

#include "audiobuffer.h"
#include "timer.h"
#include "ui_mainwindow.h"
#include "functionthread.h"

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"

#include "../../Activity/Observer.hpp"

// There are machine-specific controls for the following:
#include "../../Machines/ZX8081/ZX8081.hpp"
#include "../../Machines/Atari/2600/Atari2600.hpp"

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow, public Outputs::Speaker::Speaker::Delegate, public ScanTargetWidget::MouseDelegate, public Activity::Observer {
		Q_OBJECT

		void createActions();

	public:
		MainWindow(QWidget *parent = nullptr);
		~MainWindow();
		explicit MainWindow(const QString &fileName);

	protected:
		void keyPressEvent(QKeyEvent *event) override;
		void keyReleaseEvent(QKeyEvent *event) override;

		void setMouseIsCaptured(bool) override;
		void moveMouse(QPoint) override;
		void setButtonPressed(int index, bool isPressed) override;

	private:
		std::unique_ptr<Ui::MainWindow> ui;
		std::unique_ptr<Timer> timer;

		// Initial setup stuff.
		Analyser::Static::TargetList targets;
		enum class UIPhase {
			SelectingMachine, RequestingROMs, RunningMachine
		} uiPhase = UIPhase::SelectingMachine;
		QString openFileName;
		void setUIPhase(UIPhase);

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

		void changeEvent(QEvent *) override;

	private slots:
		void startMachine();
		void updateStatusBarText();

	private:
		void start_appleII();
		void start_amstradCPC();
		void start_atariST();
		void start_electron();
		void start_macintosh();
		void start_msx();
		void start_oric();
		void start_vic20();
		void start_zx80();
		void start_zx81();

		enum class KeyboardInputMode {
			Keyboard, Joystick
		} keyboardInputMode;

		QAction *insertAction = nullptr;
		void insertFile(const QString &fileName);

		void launchFile(const QString &fileName);
		void launchTarget(std::unique_ptr<Analyser::Static::Target> &&);

		void restoreSelections();
		void storeSelections();

		void init();
		void tile(const QMainWindow *previous);
		QString getFilename(const char *title);

		void closeEvent(QCloseEvent *event) override;
		static inline int mainWindowCount = 0;

		void deleteMachine();

		QMenu *displayMenu = nullptr;
		void addDisplayMenu(const std::string &machinePrefix, const std::string &compositeColour, const std::string &compositeMono, const std::string &svideo, const std::string &rgb);

		QMenu *enhancementsMenu = nullptr;
		QAction *automaticTapeControlAction = nullptr;
		void addEnhancementsMenu(const std::string &machinePrefix, bool offerQuickLoad, bool offerQuickBoot);
		void addEnhancementsItems(const std::string &machinePrefix, QMenu *menu, bool offerQuickLoad, bool offerQuickBoot, bool automatic_tape_motor_control);

		QMenu *controlsMenu = nullptr;
		QAction *stopTapeAction = nullptr;
		QAction *startTapeAction = nullptr;
		void addZX8081Menu(const std::string &machinePrefix);
		void updateTapeControls();

		void addAtari2600Menu();
		void toggleAtari2600Switch(Atari2600Switch toggleSwitch);

		void setWindowTitle();
		bool mouseIsCaptured = false;

		QMenu *helpMenu = nullptr;
		void addHelpMenu();

		QMenu *inputMenu = nullptr;

		std::optional<Inputs::Keyboard::Key> keyForEvent(QKeyEvent *);

		void register_led(const std::string &) override;
		void set_led_status(const std::string &, bool) override;

		std::recursive_mutex ledStatusesLock;
		std::map<std::string, bool> ledStatuses;

		void addActivityObserver();
};

#endif // MAINWINDOW_H
