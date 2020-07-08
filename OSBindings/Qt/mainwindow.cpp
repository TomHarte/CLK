#include <QObject>
#include <QStandardPaths>

#include <QtWidgets>
#include <QtGlobal>

#include "mainwindow.h"
#include "settings.h"
#include "timer.h"

#include "../../Numeric/CRC.hpp"

namespace {

std::unique_ptr<std::vector<uint8_t>> fileContentsAndClose(FILE *file) {
	auto data = std::make_unique<std::vector<uint8_t>>();

	fseek(file, 0, SEEK_END);
	data->resize(std::ftell(file));
	fseek(file, 0, SEEK_SET);
	size_t read = fread(data->data(), 1, data->size(), file);
	fclose(file);

	if(read == data->size()) {
		return data;
	}

	return nullptr;
}

}

/*
	General Qt implementation notes:

	*	it seems like Qt doesn't offer a way to constrain the aspect ratio of a view by constraining
		the size of the window (i.e. you can use a custom layout to constrain a view, but that won't
		affect the window, so isn't useful for this project). Therefore the emulation window resizes freely.
*/

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {
	init();
	setUIPhase(UIPhase::SelectingMachine);
}

MainWindow::MainWindow(const QString &fileName) {
	init();
	launchFile(fileName);
}

void MainWindow::deleteMachine() {
	// Stop the timer; stopping this first ensures the machine won't attempt
	// to write to the audioOutput while it is being shut down.
	timer.reset();

	// Shut down the scan target while it still has a context for cleanup.
	ui->openGLWidget->stop();

	// Stop the audio output, and its thread.
	if(audioOutput) {
		audioThread.performAsync([this] {
			audioOutput->stop();
			audioOutput.reset();
		});
		audioThread.stop();
	}

	// Release the machine.
	machine.reset();

	// Remove any machine-specific options.
	if(displayMenu)			menuBar()->removeAction(displayMenu->menuAction());
	if(enhancementsMenu)	menuBar()->removeAction(enhancementsMenu->menuAction());
	if(controlsMenu)		menuBar()->removeAction(controlsMenu->menuAction());
}

MainWindow::~MainWindow() {
	deleteMachine();
	--mainWindowCount;

	// Store the current user selections.
	storeSelections();
}

void MainWindow::closeEvent(QCloseEvent *event) {
	// SDI behaviour, which may or may not be normal (?): if the user is closing a
	// final window, and it is anywher ebeyond the machine picker, send them back
	// to the start. i.e. assume they were closing that document, not the application.
	if(mainWindowCount == 1 && uiPhase != UIPhase::SelectingMachine) {
		setUIPhase(UIPhase::SelectingMachine);
		deleteMachine();
		event->ignore();
		return;
	}
	QMainWindow::closeEvent(event);
}

void MainWindow::init() {
	++mainWindowCount;
	qApp->installEventFilter(this);

	ui = std::make_unique<Ui::MainWindow>();
	ui->setupUi(this);
	romRequestBaseText = ui->missingROMsBox->toPlainText();

	createActions();
	restoreSelections();
}

void MainWindow::createActions() {
	// Create a file menu.
	QMenu *const fileMenu = menuBar()->addMenu(tr("&File"));

	// Add file option: 'New'
	QAction *const newAct = new QAction(tr("&New"), this);
	newAct->setShortcuts(QKeySequence::New);
	connect(newAct, &QAction::triggered, this, [this] {
		storeSelections();

		MainWindow *other = new MainWindow;
		other->tile(this);
		other->setAttribute(Qt::WA_DeleteOnClose);
		other->show();
	});
	fileMenu->addAction(newAct);

	// Add file option: 'Open...'
	QAction *const openAct = new QAction(tr("&Open..."), this);
	openAct->setShortcuts(QKeySequence::Open);
	connect(openAct, &QAction::triggered, this, [this] {
		const QString fileName = getFilename("Open...");
		if(!fileName.isEmpty()) {
			// My understanding of SDI: if a file was opened for a 'vacant' window, launch it directly there;
			// otherwise create a new window for it.
			if(machine) {
				MainWindow *const other = new MainWindow(fileName);
				other->tile(this);
				other->setAttribute(Qt::WA_DeleteOnClose);
				other->show();
			} else {
				launchFile(fileName);
			}
		}
	});
	fileMenu->addAction(openAct);

	// Add a separator and then an 'Insert...'.
	fileMenu->addSeparator();
	insertAction = new QAction(tr("&Insert..."), this);
	insertAction->setEnabled(false);
	connect(insertAction, &QAction::triggered, this, [this] {
		const QString fileName = getFilename("Insert...");
		if(!fileName.isEmpty()) {
			insertFile(fileName);
		}
	});
	fileMenu->addAction(insertAction);

	addHelpMenu();

	// Link up the start machine button.
	connect(ui->startMachineButton, &QPushButton::clicked, this, &MainWindow::startMachine);
}

void MainWindow::addHelpMenu() {
	if(helpMenu) {
		menuBar()->removeAction(helpMenu->menuAction());
	}

	// Add Help menu, with an 'About...' option.
	helpMenu = menuBar()->addMenu(tr("&Help"));
	helpMenu->addAction(tr("&About"), this, [this] {
		QMessageBox::about(this, tr("About Clock Signal"),
			tr(	"<p>Clock Signal is an emulator of various platforms.</p>"

				"<p>This emulator is offered under the MIT licence; its source code "
				"is available from <a href=\"https://github.com/tomharte/CLK\">GitHub</a>.</p>"

				"<p>This port is experimental, especially with regard to latency; "
				"please don't hesitate to provide feedback, "
				"<a href=\"mailto:thomas.harte@gmail.com\">by email</a> or via the "
				"<a href=\"https://github.com/tomharte/CLK/issues\">GitHub issue tracker</a>.</p>"
		));
	});
}

QString MainWindow::getFilename(const char *title) {
	Settings settings;

	// Use the Settings to get a default open path; write it back afterwards.
	QString fileName = QFileDialog::getOpenFileName(this, tr(title), settings.value("openPath").toString());
	if(!fileName.isEmpty()) {
		settings.setValue("openPath", QFileInfo(fileName).absoluteDir().path());
	}
	return fileName;
}

void MainWindow::insertFile(const QString &fileName) {
	if(!machine) return;

	auto mediaTarget = machine->media_target();
	if(!mediaTarget) return;

	Analyser::Static::Media media = Analyser::Static::GetMedia(fileName.toStdString());
	mediaTarget->insert_media(media);
}

void MainWindow::launchFile(const QString &fileName) {
	targets = Analyser::Static::GetTargets(fileName.toStdString());
	if(!targets.empty()) {
		openFileName = QFileInfo(fileName).fileName();
		launchMachine();
	}
}

void MainWindow::tile(const QMainWindow *previous) {
	// This entire function is essentially verbatim from the Qt SDI example.
	if (!previous)
		return;

	int topFrameWidth = previous->geometry().top() - previous->pos().y();
	if (!topFrameWidth)
		topFrameWidth = 40;

	const QPoint pos = previous->pos() + 2 * QPoint(topFrameWidth, topFrameWidth);
#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
	if (screen()->availableGeometry().contains(rect().bottomRight() + pos))
#endif
		move(pos);
}

// MARK: Machine launch.

void MainWindow::launchMachine() {
	const QStringList appDataLocations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
	missingRoms.clear();

	ROMMachine::ROMFetcher rom_fetcher = [&appDataLocations, this]
		(const std::vector<ROMMachine::ROM> &roms) -> std::vector<std::unique_ptr<std::vector<uint8_t>>> {
		std::vector<std::unique_ptr<std::vector<uint8_t>>> results;

		for(const auto &rom: roms) {
			FILE *file = nullptr;
			for(const auto &path: appDataLocations) {
				const std::string source = path.toStdString() + "/ROMImages/" + rom.machine_name + "/" + rom.file_name;
				const std::string nativeSource = QDir::toNativeSeparators(QString::fromStdString(source)).toStdString();

				file = fopen(nativeSource.c_str(), "rb");
				if(file) break;
			}

			if(file) {
				auto data = fileContentsAndClose(file);
				if(data) {
					results.push_back(std::move(data));
					continue;
				}
			}

			results.push_back(nullptr);
			missingRoms.push_back(rom);
		}
		return results;
	};
	Machine::Error error;
	machine.reset(Machine::MachineForTargets(targets, rom_fetcher, error));

	if(error != Machine::Error::None) {
		switch(error) {
			default: break;
			case Machine::Error::MissingROM: {
				setUIPhase(UIPhase::RequestingROMs);

				// Populate request text.
				QString requestText = romRequestBaseText;
				size_t index = 0;
				for(const auto rom: missingRoms) {
					requestText += "• ";
					requestText += rom.descriptive_name.c_str();

					++index;
					if(index == missingRoms.size()) {
						requestText += ".\n";
						continue;
					}
					if(index == missingRoms.size() - 1) {
						requestText += "; and\n";
						continue;
					}
					requestText += ";\n";
				}
				ui->missingROMsBox->setPlainText(requestText);
			} break;
		}
		return;
	}

	setUIPhase(UIPhase::RunningMachine);

	// Supply the scan target.
	// TODO: in the future, hypothetically, deal with non-scan producers.
	const auto scan_producer = machine->scan_producer();
	if(scan_producer) {
		ui->openGLWidget->setScanProducer(scan_producer);
	}

	// Install audio output if required.
	const auto audio_producer = machine->audio_producer();
	if(audio_producer) {
		static constexpr size_t samplesPerBuffer = 256;	// TODO: select this dynamically.
		const auto speaker = audio_producer->get_speaker();
		if(speaker) {
			const QAudioDeviceInfo &defaultDeviceInfo = QAudioDeviceInfo::defaultOutputDevice();
			if(!defaultDeviceInfo.isNull()) {
				QAudioFormat idealFormat = defaultDeviceInfo.preferredFormat();

				// Use the ideal format's sample rate, provide stereo as long as at least two channels
				// are available, and — at least for now — assume a good buffer size.
				audioIsStereo = (idealFormat.channelCount() > 1) && speaker->get_is_stereo();
				audioIs8bit = idealFormat.sampleSize() < 16;
				idealFormat.setChannelCount(1 + int(audioIsStereo));
				idealFormat.setSampleSize(audioIs8bit ? 8 : 16);

				speaker->set_output_rate(idealFormat.sampleRate(), samplesPerBuffer, audioIsStereo);
				speaker->set_delegate(this);

				audioThread.start();
				audioThread.performAsync([this, idealFormat] {
					// Create an audio output.
					audioOutput = std::make_unique<QAudioOutput>(idealFormat);

					// Start the output. The additional `audioBuffer` is meant to minimise latency,
					// believe it or not, given Qt's semantics.
					audioOutput->setBufferSize(samplesPerBuffer * sizeof(int16_t));
					audioOutput->start(&audioBuffer);
					audioBuffer.setDepth(audioOutput->bufferSize());
				});
			}
		}
	}

	// Set user-friendly default options.
	const auto machineType = targets[0]->machine;
	const std::string longMachineName = Machine::LongNameForTargetMachine(machineType);
	const auto configurable = machine->configurable_device();
	if(configurable) {
		configurable->set_options(Machine::AllOptionsByMachineName()[longMachineName]);
	}

	// If this is a timed machine, start up the timer.
	const auto timedMachine = machine->timed_machine();
	if(timedMachine) {
		timer = std::make_unique<Timer>(this);
		timer->startWithMachine(timedMachine, &machineMutex);
	}

	// If the machine can accept new media while running, enable
	// the inert action.
	if(machine->media_target()) {
		insertAction->setEnabled(true);
	}

	// Add machine-specific UI.
	const std::string settingsPrefix = Machine::ShortNameForTargetMachine(machineType);
	switch(machineType) {
		case Analyser::Machine::AmstradCPC:
			addDisplayMenu(settingsPrefix, "Television", "", "", "Monitor");
		break;

		case Analyser::Machine::AppleII:
			addDisplayMenu(settingsPrefix, "Colour", "Monochrome", "", "");
		break;

		case Analyser::Machine::Atari2600:
			addAtari2600Menu();
		break;

		case Analyser::Machine::AtariST:
			addDisplayMenu(settingsPrefix, "Television", "", "", "Monitor");
		break;

		case Analyser::Machine::ColecoVision:
			addDisplayMenu(settingsPrefix, "Composite", "", "S-Video", "");
		break;

		case Analyser::Machine::Electron:
			addDisplayMenu(settingsPrefix, "Composite", "", "S-Video", "RGB");
			addEnhancementsMenu(settingsPrefix, true, false);
		break;

		case Analyser::Machine::Macintosh:
			addEnhancementsMenu(settingsPrefix, false, true);
		break;

		case Analyser::Machine::MasterSystem:
			addDisplayMenu(settingsPrefix, "Composite", "", "S-Video", "SCART");
		break;

		case Analyser::Machine::MSX:
			addDisplayMenu(settingsPrefix, "Composite", "", "S-Video", "SCART");
			addEnhancementsMenu(settingsPrefix, true, false);
		break;

		case Analyser::Machine::Oric:
			addDisplayMenu(settingsPrefix, "Composite", "", "", "SCART");
		break;

		case Analyser::Machine::Vic20:
			addDisplayMenu(settingsPrefix, "Composite", "", "S-Video", "");
			addEnhancementsMenu(settingsPrefix, true, false);
		break;

		case Analyser::Machine::ZX8081:
			addZX8081Menu(settingsPrefix);
		break;

		default: break;
	}

	// Push the help menu after any that were just added.
	addHelpMenu();
}

void MainWindow::addDisplayMenu(const std::string &machinePrefix, const std::string &compositeColour, const std::string &compositeMono, const std::string &svideo, const std::string &rgb) {
	// Create a display menu.
	displayMenu = menuBar()->addMenu(tr("&Display"));

	QAction *compositeColourAction = nullptr;
	QAction *compositeMonochromeAction = nullptr;
	QAction *sVideoAction = nullptr;
	QAction *rgbAction = nullptr;

	// Add all requested actions.
#define Add(name, action)								\
	if(!name.empty()) {									\
		action = new QAction(tr(name.c_str()), this);	\
		action->setCheckable(true);						\
		displayMenu->addAction(action);					\
	}

	Add(compositeColour, compositeColourAction);
	Add(compositeMono, compositeMonochromeAction);
	Add(svideo, sVideoAction);
	Add(rgb, rgbAction);

#undef Add

	// Get the machine's default setting.
	auto options = machine->configurable_device()->get_options();
	auto defaultDisplay = Reflection::get<Configurable::Display>(*options, "output");

	// Check whether there's an alternative selection in the user settings. If so, apply it.
	Settings settings;
	const auto settingName = QString::fromStdString(machinePrefix + ".displayType");
	if(settings.contains(settingName)) {
		auto userSelectedDisplay = Configurable::Display(settings.value(settingName).toInt());
		if(userSelectedDisplay != defaultDisplay) {
			defaultDisplay = userSelectedDisplay;
			Reflection::set(*options, "output", int(userSelectedDisplay));
			machine->configurable_device()->set_options(options);
		}
	}

	// Add actions to the generated options.
	size_t index = 0;
	for(auto action: {compositeColourAction, compositeMonochromeAction, sVideoAction, rgbAction}) {
		constexpr Configurable::Display displaySelections[] = {
			Configurable::Display::CompositeColour,
			Configurable::Display::CompositeMonochrome,
			Configurable::Display::SVideo,
			Configurable::Display::RGB,
		};
		const Configurable::Display displaySelection = displaySelections[index];
		++index;

		if(!action) continue;

		action->setChecked(displaySelection == defaultDisplay);
		connect(action, &QAction::triggered, this, [=] {
			for(auto otherAction: {compositeColourAction, compositeMonochromeAction, sVideoAction, rgbAction}) {
				if(otherAction && otherAction != action) otherAction->setChecked(false);
			}

			Settings settings;
			settings.setValue(settingName, int(displaySelection));

			std::lock_guard lock_guard(machineMutex);
			auto options = machine->configurable_device()->get_options();
			Reflection::set(*options, "output", int(displaySelection));
			machine->configurable_device()->set_options(options);
		});
	}
}

void MainWindow::addEnhancementsMenu(const std::string &machinePrefix, bool offerQuickLoad, bool offerQuickBoot) {
	enhancementsMenu = menuBar()->addMenu(tr("&Enhancements"));
	addEnhancementsItems(machinePrefix, enhancementsMenu, offerQuickLoad, offerQuickBoot, false);
}

void MainWindow::addEnhancementsItems(const std::string &machinePrefix, QMenu *menu, bool offerQuickLoad, bool offerQuickBoot, bool offerAutomaticTapeControl) {
	auto options = machine->configurable_device()->get_options();
	Settings settings;

#define Add(offered, text, setting, action)															\
	if(offered) {																					\
		action = new QAction(tr(text), this);														\
		action->setCheckable(true);																	\
		menu->addAction(action);																	\
																									\
		const auto settingName = QString::fromStdString(machinePrefix + "." + setting);				\
		if(settings.contains(settingName)) {														\
			const bool isSelected = settings.value(settingName).toBool();							\
			Reflection::set(*options, setting, isSelected);											\
		}																							\
		action->setChecked(Reflection::get<bool>(*options, setting) ? Qt::Checked : Qt::Unchecked);	\
																									\
		connect(action, &QAction::triggered, this, [=] {											\
			std::lock_guard lock_guard(machineMutex);												\
			auto options = machine->configurable_device()->get_options();							\
			Reflection::set(*options, setting, action->isChecked());								\
			machine->configurable_device()->set_options(options);									\
																									\
			Settings settings;																		\
			settings.setValue(settingName, action->isChecked());									\
		});																							\
	}

	QAction *action;
	Add(offerQuickLoad, "Load Quickly", "quickload", action);
	Add(offerQuickBoot, "Start Quickly", "quickboot", action);

	if(offerAutomaticTapeControl) menu->addSeparator();
	Add(offerAutomaticTapeControl, "Start and Stop Tape Automatically", "automatic_tape_motor_control", automaticTapeControlAction);

#undef Add

	machine->configurable_device()->set_options(options);
}

void MainWindow::addZX8081Menu(const std::string &machinePrefix) {
	controlsMenu = menuBar()->addMenu(tr("Tape &Control"));

	// Add the quick-load option.
	addEnhancementsItems(machinePrefix, controlsMenu, true, false, true);

	// Add the start/stop items.
	startTapeAction = new QAction(tr("Start Tape"), this);
	controlsMenu->addAction(startTapeAction);
	connect(startTapeAction, &QAction::triggered, this, [=] {
		std::lock_guard lock_guard(machineMutex);
		static_cast<ZX8081::Machine *>(machine->raw_pointer())->set_tape_is_playing(true);
		updateTapeControls();
	});

	stopTapeAction = new QAction(tr("Stop Tape"), this);
	controlsMenu->addAction(stopTapeAction);
	connect(stopTapeAction, &QAction::triggered, this, [=] {
		std::lock_guard lock_guard(machineMutex);
		static_cast<ZX8081::Machine *>(machine->raw_pointer())->set_tape_is_playing(false);
		updateTapeControls();
	});

	updateTapeControls();

	connect(automaticTapeControlAction, &QAction::triggered, this, [=] {
		updateTapeControls();
	});
}

void MainWindow::updateTapeControls() {
	const bool startStopEnabled = !automaticTapeControlAction->isChecked();
	const bool isPlaying = static_cast<ZX8081::Machine *>(machine->raw_pointer())->get_tape_is_playing();

	startTapeAction->setEnabled(!isPlaying && startStopEnabled);
	stopTapeAction->setEnabled(isPlaying && startStopEnabled);
}

void MainWindow::addAtari2600Menu() {
	controlsMenu = menuBar()->addMenu(tr("&Switches"));

	QAction *const blackAndWhiteAction = new QAction(tr("Black and white"));
	blackAndWhiteAction->setCheckable(true);
	connect(blackAndWhiteAction, &QAction::triggered, this, [=] {
		std::lock_guard lock_guard(machineMutex);
		// TODO: is this switch perhaps misnamed?
		static_cast<Atari2600::Machine *>(machine->raw_pointer())->set_switch_is_enabled(Atari2600SwitchColour, blackAndWhiteAction->isChecked());
	});
	controlsMenu->addAction(blackAndWhiteAction);

	QAction *const leftDifficultyAction = new QAction(tr("Left Difficulty"));
	leftDifficultyAction->setCheckable(true);
	connect(leftDifficultyAction, &QAction::triggered, this, [=] {
		std::lock_guard lock_guard(machineMutex);
		static_cast<Atari2600::Machine *>(machine->raw_pointer())->set_switch_is_enabled(Atari2600SwitchLeftPlayerDifficulty, leftDifficultyAction->isChecked());
	});
	controlsMenu->addAction(leftDifficultyAction);

	QAction *const rightDifficultyAction = new QAction(tr("Right Difficulty"));
	rightDifficultyAction->setCheckable(true);
	connect(rightDifficultyAction, &QAction::triggered, this, [=] {
		std::lock_guard lock_guard(machineMutex);
		static_cast<Atari2600::Machine *>(machine->raw_pointer())->set_switch_is_enabled(Atari2600SwitchRightPlayerDifficulty, rightDifficultyAction->isChecked());
	});
	controlsMenu->addAction(rightDifficultyAction);

	controlsMenu->addSeparator();

	QAction *const gameSelectAction = new QAction(tr("Game Select"));
	controlsMenu->addAction(gameSelectAction);
	connect(gameSelectAction, &QAction::triggered, this, [=] {
		toggleAtari2600Switch(Atari2600SwitchSelect);
	});

	QAction *const gameResetAction = new QAction(tr("Game Reset"));
	controlsMenu->addAction(gameResetAction);
	connect(gameSelectAction, &QAction::triggered, this, [=] {
		toggleAtari2600Switch(Atari2600SwitchReset);
	});
}

void MainWindow::toggleAtari2600Switch(Atari2600Switch toggleSwitch) {
	std::lock_guard lock_guard(machineMutex);
	const auto atari2600 = static_cast<Atari2600::Machine *>(machine->raw_pointer());

	atari2600->set_switch_is_enabled(toggleSwitch, true);
	QTimer::singleShot(500, this, [atari2600, toggleSwitch] {
		atari2600->set_switch_is_enabled(toggleSwitch, false);
	});
}

void MainWindow::speaker_did_complete_samples(Outputs::Speaker::Speaker *, const std::vector<int16_t> &buffer) {
	audioBuffer.write(buffer);
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event) {
	// Always accept dragged files.
	if(event->mimeData()->hasUrls())
		event->accept();
}

void MainWindow::dropEvent(QDropEvent* event) {
	if(!event->mimeData()->hasUrls()) {
		return;
	}
	event->accept();

	switch(uiPhase) {
		case UIPhase::SelectingMachine: {
			// Treat exactly as a File -> Open... .
			const auto fileName = event->mimeData()->urls()[0].toLocalFile();
			launchFile(fileName);
		} break;

		case UIPhase::RunningMachine: {
			// Attempt to insert into the running machine.
			const auto fileName = event->mimeData()->urls()[0].toLocalFile();
			insertFile(fileName);
		} break;

		// TODO: permit multiple files dropped at once in both of the above cases.

		case UIPhase::RequestingROMs: {
			// Attempt to match up the dragged files to the requested ROM list;
			// if and when they match, copy to a writeable QStandardPaths::AppDataLocation
			// and try launchMachine() again.

			bool foundROM = false;
			const auto appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();

			for(const auto &url: event->mimeData()->urls()) {
				const char *const name = url.toLocalFile().toUtf8();
				FILE *const file = fopen(name, "rb");
				const auto contents = fileContentsAndClose(file);
				if(!contents) continue;

				CRC::CRC32 generator;
				const uint32_t crc = generator.compute_crc(*contents);

				for(const auto &rom: missingRoms) {
					if(std::find(rom.crc32s.begin(), rom.crc32s.end(), crc) != rom.crc32s.end()) {
						foundROM = true;

						// Ensure the destination folder exists.
						const std::string path = appDataLocation + "/ROMImages/" + rom.machine_name;
						const QDir dir(QString::fromStdString(path));
						if (!dir.exists())
							dir.mkpath(".");

						// Write into place.
						const std::string destination =  QDir::toNativeSeparators(QString::fromStdString(path+ "/" + rom.file_name)).toStdString();
						FILE *const target = fopen(destination.c_str(), "wb");
						fwrite(contents->data(), 1, contents->size(), target);
						fclose(target);
					}
				}
			}

			if(foundROM) launchMachine();
		} break;
	}
}

void MainWindow::setUIPhase(UIPhase phase) {
	uiPhase = phase;

	// The volume slider is never visible by default; a running machine
	// will show and hide it dynamically.
	ui->volumeSlider->setVisible(false);

	// Show or hide the missing ROMs box.
	ui->missingROMsBox->setVisible(phase == UIPhase::RequestingROMs);

	// Show or hide the various machine-picking chrome.
	ui->machineSelectionTabs->setVisible(phase == UIPhase::SelectingMachine);
	ui->startMachineButton->setVisible(phase == UIPhase::SelectingMachine);
	ui->topTipLabel->setVisible(phase == UIPhase::SelectingMachine);

	// Consider setting a window title, if it's knowable.
	setWindowTitle();

	// Set appropriate focus if necessary; e.g. this ensures that machine-picker
	// widgets aren't still selectable after a machine starts.
	if(phase != UIPhase::SelectingMachine) {
		ui->openGLWidget->setFocus();
	} else {
		ui->startMachineButton->setDefault(true);
	}

	// Indicate whether to catch mouse input.
	ui->openGLWidget->setMouseDelegate(
		(phase == UIPhase::RunningMachine && machine && machine->mouse_machine()) ? this : nullptr
	);
}

void MainWindow::setWindowTitle() {
	QString title;

	switch(uiPhase) {
		case UIPhase::SelectingMachine:		title = tr("Select a machine...");		break;
		case UIPhase::RequestingROMs:		title = tr("Provide ROMs...");			break;

		default:
			// Update the window title. TODO: clearly I need a proper functional solution for the window title.
			if(openFileName.isEmpty()) {
				const auto machineType = targets[0]->machine;
				title = QString::fromStdString(Machine::LongNameForTargetMachine(machineType));
			} else {
				title = openFileName;
			}
		break;
	}

	if(mouseIsCaptured) title += " (press control+escape to release mouse)";

	QMainWindow::setWindowTitle(title);
}

// MARK: - Event Processing

void MainWindow::keyPressEvent(QKeyEvent *event) {
	processEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *event) {
	processEvent(event);
}

// Qt is the worst.
//
// Assume your keyboard has a key labelled both . and >, as they do on US and UK keyboards. Call it the dot key.
// Perform the following:
//	1.	press dot key;
//	2.	press shift key;
//	3.	release dot key;
//	4.	release shift key.
//
// Per empirical testing, and key repeat aside, on both macOS and Ubuntu 19.04 that sequence will result in
// _three_ keypress events, but only _two_ key release events. You'll get presses for Qt::Key_Period, Qt::Key_Greater
// and Qt::Key_Shift. You'll get releases only for Qt::Key_Greater and Qt::Key_Shift.
//
// How can you detect at runtime that Key_Greater and Key_Period are the same physical key?
//
// You can't. On Ubuntu they have the same values for QKeyEvent::nativeScanCode(), which are unique to the key,
// but they have different ::nativeVirtualKey()s.
//
// On macOS they have the same ::nativeScanCode() only because on macOS [almost] all keys have the same
// ::nativeScanCode(). So that's not usable. They have the same ::nativeVirtualKey()s, but since that isn't true
// on Ubuntu, that's also not usable.
//
// So how can you track the physical keys on a keyboard via Qt?
//
// You can't. Qt is the worst. SDL doesn't have this problem, including in X11, but I'm not sure I want the extra
// dependency. I may need to reassess.

std::optional<Inputs::Keyboard::Key> MainWindow::keyForEvent(QKeyEvent *event) {
	// Workaround for X11: assume PC-esque mapping from ::nativeScanCode to symbols.
	//
	// Yucky, ugly, harcoded yuck. TODO: work out how `xmodmap -pke` seems to derive these codes at runtime.

	if(QGuiApplication::platformName() == QLatin1String("xcb")) {
#define BIND(code, key) 	case code:	return Inputs::Keyboard::Key::key;

		switch(event->nativeVirtualKey()) {
			default: qDebug() << "Unmapped" << event->nativeScanCode(); return {};

			BIND(1, Escape);
			BIND(67, F1);	BIND(68, F2);	BIND(69, F3);	BIND(70, F4);	BIND(71, F5);
			BIND(72, F6);	BIND(73, F7);	BIND(74, F8);	BIND(75, F9);	BIND(76, F10);
			BIND(95, F11);	BIND(96, F12);
			BIND(107, PrintScreen);
			BIND(78, ScrollLock);
			BIND(127, Pause);

			BIND(49, BackTick);
			BIND(10, k1);	BIND(11, k2);	BIND(12, k3);	BIND(13, k4);	BIND(14, k5);
			BIND(15, k6);	BIND(16, k7);	BIND(17, k8);	BIND(18, k9);	BIND(19, k0);
			BIND(20, Hyphen);
			BIND(21, Equals);
			BIND(22, Backspace);

			BIND(23, Tab);
			BIND(24, Q);	BIND(25, W);	BIND(26, E);	BIND(27, R);	BIND(28, T);
			BIND(29, Y);	BIND(30, U);	BIND(31, I);	BIND(32, O);	BIND(33, P);
			BIND(34, OpenSquareBracket);
			BIND(35, CloseSquareBracket);
			BIND(51, Backslash);

			BIND(66, CapsLock);
			BIND(38, A);	BIND(39, S);	BIND(40, D);	BIND(41, F);	BIND(42, G);
			BIND(43, H);	BIND(44, J);	BIND(45, K);	BIND(46, L);
			BIND(47, Semicolon);
			BIND(48, Quote);
			BIND(36, Enter);

			BIND(50, LeftShift);
			BIND(52, Z);	BIND(53, X);	BIND(54, C);	BIND(55, V);
			BIND(56, B);	BIND(57, N);	BIND(58, M);
			BIND(59, Comma);
			BIND(60, FullStop);
			BIND(61, ForwardSlash);
			BIND(62, RightShift);

			BIND(105, LeftControl);
			BIND(204, LeftOption);
			BIND(205, LeftMeta);
			BIND(65, Space);
			BIND(108, RightOption);

			BIND(113, Left);	BIND(114, Right);	BIND(111, Up);	BIND(116, Down);

			BIND(118, Insert);
			BIND(119, Delete);
			BIND(110, Home);
			BIND(115, End);

			BIND(77, NumLock);

			BIND(106, KeypadSlash);
			BIND(63, KeypadAsterisk);
			BIND(91, KeypadDelete);
			BIND(79, Keypad7);	BIND(80, Keypad8);	BIND(81, Keypad9);	BIND(86, KeypadPlus);
			BIND(83, Keypad4);	BIND(84, Keypad5);	BIND(85, Keypad6);	BIND(82, KeypadMinus);
			BIND(87, Keypad1);	BIND(88, Keypad2);	BIND(89, Keypad3);	BIND(104, KeypadEnter);
			BIND(90, Keypad0);
			BIND(129, KeypadDecimalPoint);
			BIND(125, KeypadEquals);

			BIND(146, Help);
		}

#undef BIND
	}

	// Fall back on a limited, faulty adaptation.
#define BIND2(qtKey, clkKey) case Qt::qtKey: return Inputs::Keyboard::Key::clkKey;
#define BIND(key) BIND2(Key_##key, key)

	switch(event->key()) {
		default: return {};

		BIND(Escape);
		BIND(F1);	BIND(F2);	BIND(F3);	BIND(F4);	BIND(F5);	BIND(F6);
		BIND(F7);	BIND(F8);	BIND(F9);	BIND(F10);	BIND(F11);	BIND(F12);
		BIND2(Key_Print, PrintScreen);
		BIND(ScrollLock);	BIND(Pause);

		BIND2(Key_AsciiTilde, BackTick);
		BIND2(Key_1, k1);	BIND2(Key_2, k2);	BIND2(Key_3, k3);	BIND2(Key_4, k4);	BIND2(Key_5, k5);
		BIND2(Key_6, k6);	BIND2(Key_7, k7);	BIND2(Key_8, k8);	BIND2(Key_9, k9);	BIND2(Key_0, k0);
		BIND2(Key_Minus, Hyphen);
		BIND2(Key_Plus, Equals);
		BIND(Backspace);

		BIND(Tab);	BIND(Q);	BIND(W);	BIND(E);	BIND(R);	BIND(T);	BIND(Y);
		BIND(U);	BIND(I);	BIND(O);	BIND(P);
		BIND2(Key_BraceLeft, OpenSquareBracket);
		BIND2(Key_BraceRight, CloseSquareBracket);
		BIND(Backslash);

		BIND(CapsLock);	BIND(A);	BIND(S);	BIND(D);	BIND(F);	BIND(G);
		BIND(H);		BIND(J);	BIND(K);	BIND(L);
		BIND(Semicolon);
		BIND2(Key_Apostrophe, Quote);
		BIND2(Key_QuoteDbl, Quote);
		// TODO: something to hash?
		BIND2(Key_Return, Enter);

		BIND2(Key_Shift, LeftShift);
		BIND(Z);	BIND(X);	BIND(C);	BIND(V);
		BIND(B);	BIND(N);	BIND(M);
		BIND(Comma);
		BIND2(Key_Period, FullStop);
		BIND2(Key_Slash, ForwardSlash);
		// Omitted: right shift.

		BIND2(Key_Control, LeftControl);
		BIND2(Key_Alt, LeftOption);
		BIND2(Key_Meta, LeftMeta);
		BIND(Space);
		BIND2(Key_AltGr, RightOption);

		BIND(Left);	BIND(Right);	BIND(Up);	BIND(Down);

		BIND(Insert); BIND(Home);	BIND(PageUp);	BIND(Delete);	BIND(End);	BIND(PageDown);

		BIND(NumLock);
	}

#undef BIND
#undef BIND2
}


bool MainWindow::processEvent(QKeyEvent *event) {
	if(!machine) return true;

	const auto keyboardMachine = machine->keyboard_machine();
	if(!keyboardMachine) return true;

	const auto key = keyForEvent(event);
	if(!key) return true;

	std::unique_lock lock(machineMutex);
	keyboardMachine->get_keyboard().set_key_pressed(*key, event->text().size() ? event->text()[0].toLatin1() : '\0', event->type() == QEvent::KeyPress);

	return false;
}

void MainWindow::setMouseIsCaptured(bool isCaptured) {
	mouseIsCaptured = isCaptured;
	setWindowTitle();
}

void MainWindow::moveMouse(QPoint vector) {
	std::unique_lock lock(machineMutex);
	auto mouseMachine = machine->mouse_machine();
	if(!mouseMachine) return;

	mouseMachine->get_mouse().move(vector.x(), vector.y());
}

void MainWindow::setButtonPressed(int index, bool isPressed) {
	std::unique_lock lock(machineMutex);
	auto mouseMachine = machine->mouse_machine();
	if(!mouseMachine) return;

	mouseMachine->get_mouse().set_button_pressed(index, isPressed);
}

// MARK: - New Machine Creation

#include "../../Analyser/Static/Acorn/Target.hpp"
#include "../../Analyser/Static/AmstradCPC/Target.hpp"
#include "../../Analyser/Static/AppleII/Target.hpp"
#include "../../Analyser/Static/AtariST/Target.hpp"
#include "../../Analyser/Static/Commodore/Target.hpp"
#include "../../Analyser/Static/Macintosh/Target.hpp"
#include "../../Analyser/Static/MSX/Target.hpp"
#include "../../Analyser/Static/Oric/Target.hpp"
#include "../../Analyser/Static/ZX8081/Target.hpp"

void MainWindow::startMachine() {
	const auto selectedTab = ui->machineSelectionTabs->currentWidget();

#define TEST(x)		\
	if(selectedTab == ui->x ## Tab) {	\
		start_##x();					\
		return;							\
	}

	TEST(appleII);
	TEST(amstradCPC);
	TEST(atariST);
	TEST(electron);
	TEST(macintosh);
	TEST(msx);
	TEST(oric);
	TEST(vic20);
	TEST(zx80);
	TEST(zx81);

#undef TEST
}

void MainWindow::start_appleII() {
	using Target = Analyser::Static::AppleII::Target;
	auto target = std::make_unique<Target>();

	switch(ui->appleIIModelComboBox->currentIndex()) {
		default:	target->model = Target::Model::II;			break;
		case 1:		target->model = Target::Model::IIplus;		break;
		case 2:		target->model = Target::Model::IIe;			break;
		case 3:		target->model = Target::Model::EnhancedIIe;	break;
	}

	switch(ui->appleIIDiskControllerComboBox->currentIndex()) {
		default:	target->disk_controller = Target::DiskController::SixteenSector;	break;
		case 1:		target->disk_controller = Target::DiskController::ThirteenSector;	break;
		case 2:		target->disk_controller = Target::DiskController::None;				break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_amstradCPC() {
	using Target = Analyser::Static::AmstradCPC::Target;
	auto target = std::make_unique<Target>();

	switch(ui->amstradCPCModelComboBox->currentIndex()) {
		default:	target->model = Target::Model::CPC464;	break;
		case 1:		target->model = Target::Model::CPC664;	break;
		case 2:		target->model = Target::Model::CPC6128;	break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_atariST() {
	using Target = Analyser::Static::AtariST::Target;
	auto target = std::make_unique<Target>();

	/* There are no options yet for an Atari ST. */

	launchTarget(std::move(target));
}

void MainWindow::start_electron() {
	using Target = Analyser::Static::Acorn::Target;
	auto target = std::make_unique<Target>();

	target->has_dfs = ui->electronDFSCheckBox->isChecked();
	target->has_adfs = ui->electronADFSCheckBox->isChecked();

	launchTarget(std::move(target));
}

void MainWindow::start_macintosh() {
	using Target = Analyser::Static::Macintosh::Target;
	auto target = std::make_unique<Target>();

	switch(ui->macintoshModelComboBox->currentIndex()) {
		default:	target->model = Target::Model::Mac512ke;	break;
		case 1:		target->model = Target::Model::MacPlus;		break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_msx() {
	using Target = Analyser::Static::MSX::Target;
	auto target = std::make_unique<Target>();

	switch(ui->msxRegionComboBox->currentIndex()) {
		default:	target->region = Target::Region::Europe;	break;
		case 1:		target->region = Target::Region::USA;		break;
		case 2:		target->region = Target::Region::Japan;		break;
	}

	target->has_disk_drive = ui->msxDiskDriveCheckBox->isChecked();

	launchTarget(std::move(target));
}

void MainWindow::start_oric() {
	using Target = Analyser::Static::Oric::Target;
	auto target = std::make_unique<Target>();

	switch(ui->oricModelComboBox->currentIndex()) {
		default:	target->rom = Target::ROM::BASIC10;	break;
		case 1:		target->rom = Target::ROM::BASIC11;	break;
		case 2:		target->rom = Target::ROM::Pravetz;	break;
	}

	switch(ui->oricDiskInterfaceComboBox->currentIndex()) {
		default:	target->disk_interface = Target::DiskInterface::None;		break;
		case 1:		target->disk_interface = Target::DiskInterface::Microdisc;	break;
		case 2:		target->disk_interface = Target::DiskInterface::Jasmin;		break;
		case 3:		target->disk_interface = Target::DiskInterface::Pravetz;	break;
		case 4:		target->disk_interface = Target::DiskInterface::BD500;		break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_vic20() {
	using Target = Analyser::Static::Commodore::Target;
	auto target = std::make_unique<Target>();

	switch(ui->vic20RegionComboBox->currentIndex()) {
		default:	target->region = Target::Region::European;	break;
		case 1:		target->region = Target::Region::American;	break;
		case 2:		target->region = Target::Region::Danish;	break;
		case 3:		target->region = Target::Region::Swedish;	break;
		case 4:		target->region = Target::Region::Japanese;	break;
	}

	auto memoryModel = Target::MemoryModel::Unexpanded;
	switch(ui->vic20MemorySizeComboBox->currentIndex()) {
		default:	break;
		case 1:		memoryModel = Target::MemoryModel::EightKB;		break;
		case 2:		memoryModel = Target::MemoryModel::ThirtyTwoKB;	break;
	}
	target->set_memory_model(memoryModel);

	target->has_c1540 = ui->vic20C1540CheckBox->isChecked();

	launchTarget(std::move(target));
}

void MainWindow::start_zx80() {
	using Target = Analyser::Static::ZX8081::Target;
	auto target = std::make_unique<Target>();

	switch(ui->zx80MemorySizeComboBox->currentIndex()) {
		default:	target->memory_model = Target::MemoryModel::Unexpanded;	break;
		case 1:		target->memory_model = Target::MemoryModel::SixteenKB;	break;
	}

	target->is_ZX81 = false;
	target->ZX80_uses_ZX81_ROM = ui->zx80UseZX81ROMCheckBox->isChecked();

	launchTarget(std::move(target));
}

void MainWindow::start_zx81() {
	using Target = Analyser::Static::ZX8081::Target;
	auto target = std::make_unique<Target>();

	switch(ui->zx81MemorySizeComboBox->currentIndex()) {
		default:	target->memory_model = Target::MemoryModel::Unexpanded;	break;
		case 1:		target->memory_model = Target::MemoryModel::SixteenKB;	break;
	}

	target->is_ZX81 = true;

	launchTarget(std::move(target));
}

void MainWindow::launchTarget(std::unique_ptr<Analyser::Static::Target> &&target) {
	targets.clear();
	targets.push_back(std::move(target));
	launchMachine();
}

// MARK: - UI state

// An assumption made widely below is that it's more likely I'll preserve combo box text
// than indices. This has historically been true on the Mac, as I tend to add additional
// options but the existing text is rarely affected.

#define AllSettings()													\
	/* Machine selection. */											\
	Tabs(machineSelectionTabs, "machineSelection");						\
																		\
	/* Apple II. */														\
	ComboBox(appleIIModelComboBox, "appleII.model");					\
	ComboBox(appleIIDiskControllerComboBox, "appleII.diskController");	\
																		\
	/* Amstrad CPC. */													\
	ComboBox(amstradCPCModelComboBox, "amstradcpc.model");				\
																		\
	/* Atari ST: nothing */												\
																		\
	/* Electron. */														\
	CheckBox(electronDFSCheckBox, "electron.hasDFS");					\
	CheckBox(electronADFSCheckBox, "electron.hasADFS");					\
																		\
	/* Macintosh. */													\
	ComboBox(macintoshModelComboBox, "macintosh.model");				\
																		\
	/* MSX. */															\
	ComboBox(msxRegionComboBox, "msx.region");							\
	CheckBox(msxDiskDriveCheckBox, "msx.hasDiskDrive");					\
																		\
	/* Oric. */															\
	ComboBox(oricModelComboBox, "msx.model");							\
	ComboBox(oricDiskInterfaceComboBox, "msx.diskInterface");			\
																		\
	/* Vic-20 */														\
	ComboBox(vic20RegionComboBox, "vic20.region");						\
	ComboBox(vic20MemorySizeComboBox, "vic20.memorySize");				\
	CheckBox(vic20C1540CheckBox, "vic20.has1540");						\
																		\
	/* ZX80. */															\
	ComboBox(zx80MemorySizeComboBox, "zx80.memorySize");				\
	CheckBox(zx80UseZX81ROMCheckBox, "zx80.usesZX81ROM");				\
																		\
	/* ZX81. */															\
	ComboBox(zx81MemorySizeComboBox, "zx81.memorySize");

void MainWindow::storeSelections() {
	Settings settings;
#define Tabs(name, key)		settings.setValue(key, ui->name->currentIndex())
#define CheckBox(name, key) settings.setValue(key, ui->name->isChecked())
#define ComboBox(name, key) settings.setValue(key, ui->name->currentText())

	AllSettings();

#undef Tabs
#undef CheckBox
#undef ComboBox
}

void MainWindow::restoreSelections() {
	Settings settings;

#define Tabs(name, key)		ui->name->setCurrentIndex(settings.value(key).toInt())
#define CheckBox(name, key)	ui->name->setCheckState(settings.value(key).toBool() ? Qt::Checked : Qt::Unchecked)
#define ComboBox(name, key) ui->name->setCurrentText(settings.value(key).toString())

	AllSettings();

#undef Tabs
#undef CheckBox
#undef ComboBox
}
