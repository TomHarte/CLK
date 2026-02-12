#include "mainwindow.h"
#include "settings.h"
#include "timer.h"

#include <QtGlobal>

#include <QObject>
#include <QStandardPaths>

#include <QAudioDevice>
#include <QMediaDevices>
#include <QtWidgets>

#include <cstdio>
#include <memory>

#include "../../Numeric/CRC.hpp"
#include "../../Configurable/StandardOptions.hpp"

namespace {

std::unique_ptr<std::vector<uint8_t>> fileContentsAndClose(FILE *const file) {
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

MainWindow::MainWindow(QWidget *const parent) : QMainWindow(parent) {
	init();
	setUIPhase(UIPhase::SelectingMachine);
}

MainWindow::MainWindow(const QString &fileName) {
	init();
	if(!launchFile(fileName)) {
		setUIPhase(UIPhase::SelectingMachine);
	}
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
	if(inputMenu)			menuBar()->removeAction(inputMenu->menuAction());
	displayMenu = enhancementsMenu = controlsMenu = inputMenu = nullptr;

	// Remove the status bar, if any.
	setStatusBar(nullptr);
}

MainWindow::~MainWindow() {
	deleteMachine();
	--mainWindowCount;

	// Store the current user selections.
	storeSelections();
}

void MainWindow::closeEvent(QCloseEvent *const event) {
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

	// TEMPORARY: remove the Apple IIgs tab; this machine isn't ready yet.
	ui->machineSelectionTabs->removeTab(ui->machineSelectionTabs->indexOf(ui->appleIIgsTab));

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

QString MainWindow::getFilename(const char *const title) {
	Settings settings;

	// Use the Settings to get a default open path; write it back afterwards.
	QString fileName = QFileDialog::getOpenFileName(this, tr(title), settings.value("openPath").toString());
	if(!fileName.isEmpty()) {
		settings.setValue("openPath", QFileInfo(fileName).absoluteDir().path());
	}
	return fileName;
}

bool MainWindow::insertFile(const QString &fileName) {
	if(!machine) return false;

	auto mediaTarget = machine->media_target();
	if(!mediaTarget) return false;

	const Analyser::Static::Media media = Analyser::Static::GetMedia(fileName.toStdString());
	if(media.empty()) return false;
	return mediaTarget->insert_media(media);
}

bool MainWindow::launchFile(const QString &fileName) {
	targets = Analyser::Static::GetTargets(fileName.toStdString());
	if(!targets.empty()) {
		openFileName = QFileInfo(fileName).fileName();
		launchMachine();
		return true;
	} else {
		QMessageBox msgBox;
		msgBox.setText("Unable to open file: " + fileName);
		msgBox.exec();
		return false;
	}
}

void MainWindow::tile(const QMainWindow *const previous) {
	// This entire function is essentially verbatim from the Qt SDI example.
	if (!previous)
		return;

	int topFrameWidth = previous->geometry().top() - previous->pos().y();
	if (!topFrameWidth)
		topFrameWidth = 40;

	const QPoint pos = previous->pos() + 2 * QPoint(topFrameWidth, topFrameWidth);
	if (screen()->availableGeometry().contains(rect().bottomRight() + pos)) {
		move(pos);
	}
}

// MARK: Machine launch.

void MainWindow::launchMachine() {
	const QStringList appDataLocations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);

	ROMMachine::ROMFetcher rom_fetcher = [&appDataLocations, this]
		(const ROM::Request &roms) -> ROM::Map {
		ROM::Map results;

		for(const auto &description: roms.all_descriptions()) {
			for(const auto &file_name: description.file_names) {
				FILE *file = nullptr;
				for(const auto &path: appDataLocations) {
					const std::string source = path.toStdString() + "/ROMImages/" + description.machine_name + "/" + file_name;
					const std::string nativeSource = QDir::toNativeSeparators(QString::fromStdString(source)).toStdString();

					file = fopen(nativeSource.c_str(), "rb");
					if(file) break;
				}

				if(file) {
					auto data = fileContentsAndClose(file);
					if(data) {
						results[description.name] = *data;
						continue;
					}
				}
			}
		}

		missingRoms = roms.subtract(results);
		return results;
	};
	Machine::Error error;
	machine = Machine::MachineForTargets(targets, rom_fetcher, error);

	if(error != Machine::Error::None) {
		switch(error) {
			default: break;
			case Machine::Error::MissingROM: {
				setUIPhase(UIPhase::RequestingROMs);

				// Populate request text.
				QString requestText = romRequestBaseText;
				requestText += QString::fromWCharArray(missingRoms.description(0, L'•').c_str());
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
			QAudioDevice device(QMediaDevices::defaultAudioOutput());
			if(true) {	// TODO: how to check that audio output is available in Qt6?
				QAudioFormat idealFormat = device.preferredFormat();

				// Use the ideal format's sample rate, provide stereo as long as at least two channels
				// are available, and — at least for now — assume a good buffer size.
				audioIsStereo = (idealFormat.channelCount() > 1) && speaker->get_is_stereo();

				audioIs8bit = idealFormat.sampleFormat() == QAudioFormat::UInt8;

				idealFormat.setChannelCount(1 + int(audioIsStereo));
				idealFormat.setSampleFormat(audioIs8bit ? QAudioFormat::UInt8 : QAudioFormat::Int16);

				speaker->set_output_rate(idealFormat.sampleRate(), samplesPerBuffer, audioIsStereo);
				speaker->set_delegate(this);

				audioThread.start();
				audioThread.performAsync([&] {
					// Create an audio output.
					audioOutput = std::make_unique<QAudioSink>(device, idealFormat);

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

	// Add an 'input' menu if justified (i.e. machine has both a keyboard and joystick input, and the keyboard is exclusive).
	auto keyboardMachine = machine->keyboard_machine();
	auto joystickMachine = machine->joystick_machine();
	if(keyboardMachine && joystickMachine && keyboardMachine->get_keyboard().is_exclusive()) {
		inputMenu = menuBar()->addMenu(tr("&Input"));

		QAction *const asKeyboardAction = new QAction(tr("Use Keyboard as Keyboard"), this);
		asKeyboardAction->setCheckable(true);
		asKeyboardAction->setChecked(true);
		inputMenu->addAction(asKeyboardAction);

		QAction *const asJoystickAction = new QAction(tr("Use Keyboard as Joystick"), this);
		asJoystickAction->setCheckable(true);
		asJoystickAction->setChecked(false);
		inputMenu->addAction(asJoystickAction);

		connect(asKeyboardAction, &QAction::triggered, this, [=, this] {
			keyboardInputMode = KeyboardInputMode::Keyboard;
			asKeyboardAction->setChecked(true);
			asJoystickAction->setChecked(false);
		});

		connect(asJoystickAction, &QAction::triggered, this, [=, this] {
			keyboardInputMode = KeyboardInputMode::Joystick;
			asKeyboardAction->setChecked(false);
			asJoystickAction->setChecked(true);
		});
	}
	keyboardInputMode = keyboardMachine ? KeyboardInputMode::Keyboard : KeyboardInputMode::Joystick;

	// Add machine-specific UI.
	const std::string settingsPrefix = Machine::ShortNameForTargetMachine(machineType);
	auto configurableMachine = machine->configurable_device();
	if(configurableMachine) {
		auto options = configurableMachine->get_options();
		const auto allKeys = options->all_keys();
		const auto allDisplayValues = options->values_for(Configurable::Options::DisplayOptionName);
		const auto hasDynamicCrop = std::find(allKeys.begin(), allKeys.end(), Configurable::Options::DynamicCropOptionName) != allKeys.end();
		if(hasDynamicCrop || allDisplayValues.size() > 1) {
			const auto contains = [&](const Configurable::Display option) {
				const auto name = Reflection::Enum::to_string<Configurable::Display>(option);
				return std::find(allDisplayValues.begin(), allDisplayValues.end(), name) != allDisplayValues.end();
			};

			const bool hasCompositeColour = contains(Configurable::Display::CompositeColour);
			const bool hasCompositeMonochrome = contains(Configurable::Display::CompositeMonochrome);
			const bool hasSVideo = contains(Configurable::Display::SVideo);
			const bool hasRGB = contains(Configurable::Display::RGB);

			const bool differentiateComposite = hasCompositeColour && hasCompositeMonochrome;
			const bool hasMultipleTelevisionConnections = hasSVideo && (hasCompositeColour || hasCompositeMonochrome);
			const bool hasNonCompositeConnections = hasSVideo || hasRGB;

			const auto compositeColourName = [&]() {
				if(!hasNonCompositeConnections) {
					return "Colour";
				}
				if(hasMultipleTelevisionConnections) {
					return differentiateComposite ? "Colour Composite" : "Composite";
				} else {
					return differentiateComposite ? "Colour Television" : "Television";
				}
			};

			const auto compositeMonochromeName = [&]() {
				if(!hasNonCompositeConnections) {
					return "Monochrome";
				}
				if(hasMultipleTelevisionConnections) {
					return differentiateComposite ? "Monochrome Composite" : "Composite";
				} else {
					return differentiateComposite ? "Black and White Television" : "Television";
				}
			};

			const auto rgbName = [&]() {
				return hasMultipleTelevisionConnections ? "RGB" : "Monitor";
			};

			addDisplayMenu(
				settingsPrefix,
				hasCompositeColour ? compositeColourName() : "",
				hasCompositeMonochrome ? compositeMonochromeName() : "",
				hasSVideo ? "S-Video" : "",
				hasRGB ? rgbName() : "",
				hasDynamicCrop
			);
		}

		// The ZX80 and ZX81 have a specialised version of this.
		// It might become general later if I generalite automatic tape motor control, which I probably should.
		if(machineType != Analyser::Machine::ZX8081) {
			const auto hasQuickLoad = std::find(allKeys.begin(), allKeys.end(), Configurable::Options::QuickLoadOptionName) != allKeys.end();
			const auto hasQuickBoot = std::find(allKeys.begin(), allKeys.end(), Configurable::Options::QuickBootOptionName) != allKeys.end();
			addEnhancementsMenu(settingsPrefix, hasQuickLoad, hasQuickBoot);
		}
	}

	switch(machineType) {
		case Analyser::Machine::AppleII:
			addAppleIIMenu();
		break;

		case Analyser::Machine::Atari2600:
			addAtari2600Menu();
		break;

		case Analyser::Machine::ZX8081:
			addZX8081Menu(settingsPrefix);
		break;

		default: break;
	}

	// Push the help menu after any that were just added.
	addHelpMenu();

	// Add activity LED UI.
	addActivityObserver();
}

void MainWindow::addDisplayMenu(
	const std::string &machinePrefix,
	const std::string &compositeColour,
	const std::string &compositeMono,
	const std::string &svideo,
	const std::string &rgb,
	const bool offerDynamicCrop
) {
	// Create a display menu.
	displayMenu = menuBar()->addMenu(tr("&Display"));

	QAction *compositeColourAction = nullptr;
	QAction *compositeMonochromeAction = nullptr;
	QAction *sVideoAction = nullptr;
	QAction *rgbAction = nullptr;

	// Add all requested actions.
	const auto add = [&](const std::string &name, QAction *(&action)) {
		if(!name.empty()) {
			action = new QAction(tr(name.c_str()), this);
			action->setCheckable(true);
			displayMenu->addAction(action);
		}
	};
	add(compositeColour, compositeColourAction);
	add(compositeMono, compositeMonochromeAction);
	add(svideo, sVideoAction);
	add(rgb, rgbAction);

	// Get the machine's default setting.
	auto options = machine->configurable_device()->get_options();
	auto defaultDisplay = Reflection::get<Configurable::Display>(*options, Configurable::Options::DisplayOptionName);

	// Check whether there's an alternative selection in the user settings. If so, apply it.
	Settings settings;
	const auto settingName = QString::fromStdString(machinePrefix + ".displayType");
	if(settings.contains(settingName)) {
		auto userSelectedDisplay = Configurable::Display(settings.value(settingName).toInt());
		if(userSelectedDisplay != defaultDisplay) {
			defaultDisplay = userSelectedDisplay;
			Reflection::set(*options, Configurable::Options::DisplayOptionName, int(userSelectedDisplay));
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
		connect(action, &QAction::triggered, this, [=, this] {
			for(auto otherAction: {compositeColourAction, compositeMonochromeAction, sVideoAction, rgbAction}) {
				if(otherAction) otherAction->setChecked(otherAction == action);
			}

			Settings settings;
			settings.setValue(settingName, int(displaySelection));

			std::lock_guard lock_guard(machineMutex);
			auto options = machine->configurable_device()->get_options();
			Reflection::set(*options, Configurable::Options::DisplayOptionName, int(displaySelection));
			machine->configurable_device()->set_options(options);
		});
	}

	// Possibly add a dynamic crop selector.
	if(offerDynamicCrop) {
		displayMenu->addSeparator();

		QAction *const action = new QAction(tr("Crop Dynamically"), this);
		action->setCheckable(true);
		displayMenu->addAction(action);

		const auto dynamicCropSettingName = QString::fromStdString(machinePrefix + ".dynamicCrop");
		if(settings.contains(dynamicCropSettingName)) {
			const auto useDynamicCrop = settings.value(settingName).toBool();
			action->setChecked(useDynamicCrop);
			Reflection::set(*options, Configurable::Options::DynamicCropOptionName, useDynamicCrop);
		}
		connect(action, &QAction::toggled, this, [=, this] (const bool ticked) {
			Settings settings;
			settings.setValue(dynamicCropSettingName, ticked);

			std::lock_guard lock_guard(machineMutex);
			auto options = machine->configurable_device()->get_options();
			Reflection::set(*options, Configurable::Options::DynamicCropOptionName, ticked);
			machine->configurable_device()->set_options(options);
		});
	}
}

void MainWindow::addEnhancementsMenu(const std::string &machinePrefix, const bool offerQuickLoad, const bool offerQuickBoot) {
	if(!offerQuickLoad && !offerQuickBoot) {
		return;
	}
	enhancementsMenu = menuBar()->addMenu(tr("&Enhancements"));
	addEnhancementsItems(machinePrefix, enhancementsMenu, offerQuickLoad, offerQuickBoot, false);
}

void MainWindow::addEnhancementsItems(const std::string &machinePrefix, QMenu *const menu, const bool offerQuickLoad, const bool offerQuickBoot, const bool offerAutomaticTapeControl) {
	auto options = machine->configurable_device()->get_options();
	Settings settings;

	const auto add = [&](const bool offered, const char *text, const char *setting, QAction *(&action)) {
		if(offered) {
			action = new QAction(tr(text), this);
			action->setCheckable(true);
			menu->addAction(action);

			const auto settingName = QString::fromStdString(machinePrefix + "." + setting);
			if(settings.contains(settingName)) {
				const bool isSelected = settings.value(settingName).toBool();
				Reflection::set(*options, setting, isSelected);
			}
			action->setChecked(Reflection::get<bool>(*options, setting));

			connect(action, &QAction::triggered, this, [=, this] {
				std::lock_guard lock_guard(machineMutex);
				auto options = machine->configurable_device()->get_options();
				Reflection::set(*options, setting, action->isChecked());
				machine->configurable_device()->set_options(options);

				Settings settings;
				settings.setValue(settingName, action->isChecked());
			});
		}
	};

	QAction *action;
	add(offerQuickLoad, "Load Quickly", Configurable::Options::QuickLoadOptionName, action);
	add(offerQuickBoot, "Start Quickly", Configurable::Options::QuickBootOptionName, action);

	if(offerAutomaticTapeControl) menu->addSeparator();
	add(offerAutomaticTapeControl, "Start and Stop Tape Automatically", "automatic_tape_motor_control", automaticTapeControlAction);

	machine->configurable_device()->set_options(options);
}

void MainWindow::addZX8081Menu(const std::string &machinePrefix) {
	controlsMenu = menuBar()->addMenu(tr("Tape &Control"));

	// Add the quick-load option.
	addEnhancementsItems(machinePrefix, controlsMenu, true, false, true);

	// Add the start/stop items.
	startTapeAction = new QAction(tr("Start Tape"), this);
	controlsMenu->addAction(startTapeAction);
	connect(startTapeAction, &QAction::triggered, this, [=, this] {
		std::lock_guard lock_guard(machineMutex);
		static_cast<Sinclair::ZX8081::Machine *>(machine->raw_pointer())->set_tape_is_playing(true);
		updateTapeControls();
	});

	stopTapeAction = new QAction(tr("Stop Tape"), this);
	controlsMenu->addAction(stopTapeAction);
	connect(stopTapeAction, &QAction::triggered, this, [=, this] {
		std::lock_guard lock_guard(machineMutex);
		static_cast<Sinclair::ZX8081::Machine *>(machine->raw_pointer())->set_tape_is_playing(false);
		updateTapeControls();
	});

	updateTapeControls();

	connect(automaticTapeControlAction, &QAction::triggered, this, [=, this] {
		updateTapeControls();
	});
}

void MainWindow::updateTapeControls() {
	const bool startStopEnabled = !automaticTapeControlAction->isChecked();
	const bool isPlaying = static_cast<Sinclair::ZX8081::Machine *>(machine->raw_pointer())->get_tape_is_playing();

	startTapeAction->setEnabled(!isPlaying && startStopEnabled);
	stopTapeAction->setEnabled(isPlaying && startStopEnabled);
}

void MainWindow::addAtari2600Menu() {
	controlsMenu = menuBar()->addMenu(tr("&Switches"));

	QAction *const blackAndWhiteAction = new QAction(tr("Black and white"));
	blackAndWhiteAction->setCheckable(true);
	connect(blackAndWhiteAction, &QAction::triggered, this, [=, this] {
		std::lock_guard lock_guard(machineMutex);
		// TODO: is this switch perhaps misnamed?
		static_cast<Atari2600::Machine *>(machine->raw_pointer())->set_switch_is_enabled(Atari2600SwitchColour, blackAndWhiteAction->isChecked());
	});
	controlsMenu->addAction(blackAndWhiteAction);

	QAction *const leftDifficultyAction = new QAction(tr("Left Difficulty"));
	leftDifficultyAction->setCheckable(true);
	connect(leftDifficultyAction, &QAction::triggered, this, [=, this] {
		std::lock_guard lock_guard(machineMutex);
		static_cast<Atari2600::Machine *>(machine->raw_pointer())->set_switch_is_enabled(Atari2600SwitchLeftPlayerDifficulty, leftDifficultyAction->isChecked());
	});
	controlsMenu->addAction(leftDifficultyAction);

	QAction *const rightDifficultyAction = new QAction(tr("Right Difficulty"));
	rightDifficultyAction->setCheckable(true);
	connect(rightDifficultyAction, &QAction::triggered, this, [=, this] {
		std::lock_guard lock_guard(machineMutex);
		static_cast<Atari2600::Machine *>(machine->raw_pointer())->set_switch_is_enabled(Atari2600SwitchRightPlayerDifficulty, rightDifficultyAction->isChecked());
	});
	controlsMenu->addAction(rightDifficultyAction);

	controlsMenu->addSeparator();

	QAction *const gameSelectAction = new QAction(tr("Game Select"));
	controlsMenu->addAction(gameSelectAction);
	connect(gameSelectAction, &QAction::triggered, this, [=, this] {
		toggleAtari2600Switch(Atari2600SwitchSelect);
	});

	QAction *const gameResetAction = new QAction(tr("Game Reset"));
	controlsMenu->addAction(gameResetAction);
	connect(gameSelectAction, &QAction::triggered, this, [=, this] {
		toggleAtari2600Switch(Atari2600SwitchReset);
	});
}

void MainWindow::toggleAtari2600Switch(const Atari2600Switch toggleSwitch) {
	std::lock_guard lock_guard(machineMutex);
	const auto atari2600 = static_cast<Atari2600::Machine *>(machine->raw_pointer());

	atari2600->set_switch_is_enabled(toggleSwitch, true);
	QTimer::singleShot(500, this, [atari2600, toggleSwitch] {
		atari2600->set_switch_is_enabled(toggleSwitch, false);
	});
}

void MainWindow::addAppleIIMenu() {
	// Add an additional tick box, for square pixels.
	QAction *const squarePixelsAction = new QAction(tr("Square Pixels"));
	squarePixelsAction->setCheckable(true);
	connect(squarePixelsAction, &QAction::triggered, this, [=, this] {
		std::lock_guard lock_guard(machineMutex);

		// Apply the new setting to the machine.
		setAppleIISquarePixels(squarePixelsAction->isChecked());

		// Also store it.
		Settings settings;
		settings.setValue("appleII.squarePixels", squarePixelsAction->isChecked());
	});
	displayMenu->addAction(squarePixelsAction);

	// Establish initial selection.
	Settings settings;
	const bool useSquarePixels = settings.value("appleII.squarePixels").toBool();
	squarePixelsAction->setChecked(useSquarePixels);
	setAppleIISquarePixels(useSquarePixels);
}

void MainWindow::setAppleIISquarePixels(const bool squarePixels) {
	Configurable::Device *const configurable = machine->configurable_device();
	auto options = configurable->get_options();
	auto appleii_options = static_cast<Apple::II::Machine::Options *>(options.get());

	appleii_options->use_square_pixels = squarePixels;
	configurable->set_options(options);
}

void MainWindow::speaker_did_complete_samples(Outputs::Speaker::Speaker &, const std::vector<int16_t> &buffer) {
	audioBuffer.write(buffer);
}

void MainWindow::dragEnterEvent(QDragEnterEvent *const event) {
	// Always accept dragged files.
	if(event->mimeData()->hasUrls())
		event->accept();
}

void MainWindow::dropEvent(QDropEvent *const event) {
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
			if(!insertFile(fileName)) {
				deleteMachine();
				launchFile(fileName);
			}
		} break;

		// TODO: permit multiple files dropped at once in both of the above cases.

		case UIPhase::RequestingROMs: {
			// Attempt to match up the dragged files to the requested ROM list;
			// if and when they match, copy to a writeable QStandardPaths::AppDataLocation
			// and try launchMachine() again.

			bool foundROM = false;
			const auto appDataLocation = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation).toStdString();

			QString unusedRoms;
			for(const auto &url: event->mimeData()->urls()) {
				const std::string name = url.toLocalFile().toStdString();
				FILE *const file = fopen(name.c_str(), "rb");
				if(!file) continue;
				const auto contents = fileContentsAndClose(file);
				if(!contents) continue;

				const uint32_t crc = CRC::CRC32::crc_of(*contents);

				std::optional<ROM::Description> target_rom = ROM::Description::from_crc(crc);
				if(target_rom) {
					// Ensure the destination folder exists.
					const std::string path = appDataLocation + "/ROMImages/" + target_rom->machine_name;
					const QDir dir(QString::fromStdString(path));
					if (!dir.exists())
						dir.mkpath(".");

					// Write into place.
					const std::string destination = QDir::toNativeSeparators(QString::fromStdString(path+ "/" + target_rom->file_names[0])).toStdString();
					FILE *const target = fopen(destination.c_str(), "wb");
					fwrite(contents->data(), 1, contents->size(), target);
					fclose(target);

					// Note that at least one meaningful ROM was supplied.
					foundROM = true;
				} else {
					if(!unusedRoms.isEmpty()) unusedRoms += ", ";
					unusedRoms += url.fileName();
				}
			}

			if(!unusedRoms.isEmpty()) {
				QMessageBox msgBox;
				msgBox.setText("Couldn't identify ROMs: " + unusedRoms);
				msgBox.exec();
			}
			if(foundROM) launchMachine();
		} break;
	}
}

void MainWindow::setUIPhase(const UIPhase phase) {
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

	if(mouseIsCaptured) title += " (press control+escape or F8+F12 to release mouse)";

	QMainWindow::setWindowTitle(title);
}

// MARK: - Event Processing

void MainWindow::changeEvent(QEvent *const event) {
	// Clear current key state upon any window activation change.
	if(machine && event->type() == QEvent::ActivationChange) {
		const auto keyboardMachine = machine->keyboard_machine();
		if(keyboardMachine) {
			keyboardMachine->clear_all_keys();
			return;
		}
	}

	event->ignore();
}

void MainWindow::keyPressEvent(QKeyEvent *const event) {
	processEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent *const event) {
	processEvent(event);
}

bool MainWindow::processEvent(QKeyEvent *const event) {
	if(!machine) return true;

	const auto key = keyMapper.keyForEvent(event);
	if(!key) return true;

	const bool isPressed = event->type() == QEvent::KeyPress;
	std::unique_lock lock(machineMutex);

	switch(keyboardInputMode) {
		case KeyboardInputMode::Keyboard: {
			const auto keyboardMachine = machine->keyboard_machine();
			if(!keyboardMachine) return true;

			auto &keyboard = keyboardMachine->get_keyboard();
			const auto text = event->text();
			keyboard.set_key_pressed(*key, event->text().size() ? text[0].toLatin1() : '\0', isPressed, event->isAutoRepeat());
			if(keyboard.is_exclusive() || keyboard.observed_keys().find(*key) != keyboard.observed_keys().end()) {
				return false;
			}
		}
		[[fallthrough]];

		case KeyboardInputMode::Joystick: {
			const auto joystickMachine = machine->joystick_machine();
			if(!joystickMachine) return true;

			const auto &joysticks = joystickMachine->get_joysticks();
			if(!joysticks.empty()) {
				using Key = Inputs::Keyboard::Key;
				switch(*key) {
					case Key::Left:		joysticks[0]->set_input(Inputs::Joystick::Input::Left, isPressed);		break;
					case Key::Right:	joysticks[0]->set_input(Inputs::Joystick::Input::Right, isPressed);		break;
					case Key::Up:		joysticks[0]->set_input(Inputs::Joystick::Input::Up, isPressed);		break;
					case Key::Down:		joysticks[0]->set_input(Inputs::Joystick::Input::Down, isPressed);		break;
					case Key::Space:	joysticks[0]->set_input(Inputs::Joystick::Input::Fire, isPressed);		break;
					case Key::A:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 0), isPressed);	break;
					case Key::S:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 1), isPressed);	break;
					case Key::D:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 2), isPressed);	break;
					case Key::F:		joysticks[0]->set_input(Inputs::Joystick::Input(Inputs::Joystick::Input::Fire, 3), isPressed);	break;
					default:
						if(event->text().size()) {
							const auto text = event->text();
							joysticks[0]->set_input(Inputs::Joystick::Input(text[0].toLatin1()), isPressed);
						} else {
							joysticks[0]->set_input(Inputs::Joystick::Input::Fire, isPressed);
						}
					break;
				}
			}
		} break;
	}

	return false;
}

void MainWindow::setMouseIsCaptured(const bool isCaptured) {
	mouseIsCaptured = isCaptured;
	setWindowTitle();
}

void MainWindow::moveMouse(const QPoint vector) {
	std::unique_lock lock(machineMutex);
	auto mouseMachine = machine->mouse_machine();
	if(!mouseMachine) return;

	mouseMachine->get_mouse().move(vector.x(), vector.y());
}

void MainWindow::setButtonPressed(const int index, const bool isPressed) {
	std::unique_lock lock(machineMutex);
	auto mouseMachine = machine->mouse_machine();
	if(!mouseMachine) return;

	mouseMachine->get_mouse().set_button_pressed(index, isPressed);
}

// MARK: - New Machine Creation

#include "../../Analyser/Static/Acorn/Target.hpp"
#include "../../Analyser/Static/Amiga/Target.hpp"
#include "../../Analyser/Static/AmstradCPC/Target.hpp"
#include "../../Analyser/Static/AppleII/Target.hpp"
#include "../../Analyser/Static/AppleIIgs/Target.hpp"
#include "../../Analyser/Static/AtariST/Target.hpp"
#include "../../Analyser/Static/Commodore/Target.hpp"
#include "../../Analyser/Static/Enterprise/Target.hpp"
#include "../../Analyser/Static/Macintosh/Target.hpp"
#include "../../Analyser/Static/MSX/Target.hpp"
#include "../../Analyser/Static/Oric/Target.hpp"
#include "../../Analyser/Static/PCCompatible/Target.hpp"
#include "../../Analyser/Static/ZX8081/Target.hpp"
#include "../../Analyser/Static/ZXSpectrum/Target.hpp"

void MainWindow::startMachine() {
	const auto selectedTabName = ui->machineSelectionTabs->currentWidget()->objectName().chopped(3);
	const auto starter = QString("start_") + selectedTabName;
	QMetaObject::invokeMethod(this, starter.toStdString().c_str());
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

	target->has_mockingboard = ui->appleIIMockingboardCheckBox->isChecked();

	launchTarget(std::move(target));
}

void MainWindow::start_amiga() {
	using Target = Analyser::Static::Amiga::Target;
	auto target = std::make_unique<Target>();

	switch(ui->amigaChipRAMComboBox->currentIndex()) {
		default:	target->chip_ram = Target::ChipRAM::FiveHundredAndTwelveKilobytes;	break;
		case 1:		target->chip_ram = Target::ChipRAM::OneMegabyte;					break;
		case 2:		target->chip_ram = Target::ChipRAM::TwoMegabytes;					break;
	}

	switch(ui->amigaFastRAMComboBox->currentIndex()) {
		default:	target->fast_ram = Target::FastRAM::None;			break;
		case 1:		target->fast_ram = Target::FastRAM::OneMegabyte;	break;
		case 2:		target->fast_ram = Target::FastRAM::TwoMegabytes;	break;
		case 3:		target->fast_ram = Target::FastRAM::FourMegabytes;	break;
		case 4:		target->fast_ram = Target::FastRAM::EightMegabytes;	break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_appleIIgs() {
	using Target = Analyser::Static::AppleIIgs::Target;
	auto target = std::make_unique<Target>();

	switch(ui->appleIIgsModelComboBox->currentIndex()) {
		default:	target->model = Target::Model::ROM00;	break;
		case 1:		target->model = Target::Model::ROM01;	break;
		case 2:		target->model = Target::Model::ROM03;	break;
	}

	switch(ui->appleIIgsMemorySizeComboBox->currentIndex()) {
		default:	target->memory_model = Target::MemoryModel::TwoHundredAndFiftySixKB;	break;
		case 1:		target->memory_model = Target::MemoryModel::OneMB;						break;
		case 2:		target->memory_model = Target::MemoryModel::EightMB;					break;
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

void MainWindow::start_archimedes() {
	using Target = Analyser::Static::Acorn::ArchimedesTarget;
	auto target = std::make_unique<Target>();
	launchTarget(std::move(target));
}

void MainWindow::start_atariST() {
	using Target = Analyser::Static::AtariST::Target;
	auto target = std::make_unique<Target>();

	switch(ui->atariSTRAMComboBox->currentIndex()) {
		default:	target->memory_size = Target::MemorySize::FiveHundredAndTwelveKilobytes;	break;
		case 1:		target->memory_size = Target::MemorySize::OneMegabyte;						break;
		case 2:		target->memory_size = Target::MemorySize::FourMegabytes;					break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_bbc() {
	using Target = Analyser::Static::Acorn::BBCMicroTarget;
	auto target = std::make_unique<Target>();

	target->has_1770dfs = ui->bbcMicroDFSCheckBox->isChecked();
	target->has_adfs = ui->bbcMicroADFSCheckBox->isChecked();
	target->has_beebsid = ui->bbcMicroBeebSIDCheckBox->isChecked();
	target->has_sideways_ram = ui->bbcMicroSidewaysRAMCheckBox->isChecked();

	switch(ui->bbcMicroSecondProcessorComboBox->currentIndex()) {
		default:	target->tube_processor = Target::TubeProcessor::None;		break;
		case 1:		target->tube_processor = Target::TubeProcessor::WDC65C02;	break;
		case 2:		target->tube_processor = Target::TubeProcessor::Z80;		break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_electron() {
	using Target = Analyser::Static::Acorn::ElectronTarget;
	auto target = std::make_unique<Target>();

	target->has_dfs = ui->electronDFSCheckBox->isChecked();
	target->has_pres_adfs = ui->electronADFSCheckBox->isChecked();
	target->has_ap6_rom = ui->electronAP6CheckBox->isChecked();
	target->has_sideways_ram = ui->electronSidewaysRAMCheckBox->isChecked();

	launchTarget(std::move(target));
}

void MainWindow::start_enterprise() {
	using Target = Analyser::Static::Enterprise::Target;
	auto target = std::make_unique<Target>();

	switch(ui->enterpriseModelComboBox->currentIndex()) {
		default:	target->model = Target::Model::Enterprise64;	break;
		case 1:		target->model = Target::Model::Enterprise128;	break;
		case 2:		target->model = Target::Model::Enterprise256;	break;
	}

	switch(ui->enterpriseSpeedComboBox->currentIndex()) {
		default:	target->speed = Target::Speed::FourMHz;	break;
		case 1:		target->speed = Target::Speed::SixMHz;	break;
	}

	switch(ui->enterpriseEXOSComboBox->currentIndex()) {
		default:	target->exos_version = Target::EXOSVersion::v10;	break;
		case 1:		target->exos_version = Target::EXOSVersion::v20;	break;
		case 2:		target->exos_version = Target::EXOSVersion::v21;	break;
		case 3:		target->exos_version = Target::EXOSVersion::v23;	break;
	}

	switch(ui->enterpriseBASICComboBox->currentIndex()) {
		default:	target->basic_version = Target::BASICVersion::None;	break;
		case 1:		target->basic_version = Target::BASICVersion::v10;	break;
		case 2:		target->basic_version = Target::BASICVersion::v11;	break;
		case 3:		target->basic_version = Target::BASICVersion::v21;	break;
	}

	switch(ui->enterpriseDOSComboBox->currentIndex()) {
		default:	target->dos = Target::DOS::None;	break;
		case 1:		target->dos = Target::DOS::EXDOS;	break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_macintosh() {
	using Target = Analyser::Static::Macintosh::Target;
	auto target = std::make_unique<Target>();

	switch(ui->macintoshModelComboBox->currentIndex()) {
		default:	target->model = Target::Model::Mac128k;		break;
		case 1:		target->model = Target::Model::Mac512k;		break;
		case 2:		target->model = Target::Model::Mac512ke;	break;
		case 3:		target->model = Target::Model::MacPlus;		break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_msx() {
	using Target = Analyser::Static::MSX::Target;
	auto target = std::make_unique<Target>();

	switch(ui->msxModelComboBox->currentIndex()) {
		default:	target->model = Target::Model::MSX1;		break;
		case 1:		target->model = Target::Model::MSX2;		break;
	}
	switch(ui->msxRegionComboBox->currentIndex()) {
		default:	target->region = Target::Region::Europe;	break;
		case 1:		target->region = Target::Region::USA;		break;
		case 2:		target->region = Target::Region::Japan;		break;
	}

	target->has_disk_drive = ui->msxDiskDriveCheckBox->isChecked();
	target->has_msx_music = ui->msxMSXMUSICCheckBox->isChecked();

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

void MainWindow::start_pc() {
	using Target = Analyser::Static::PCCompatible::Target;
	auto target = std::make_unique<Target>();

	switch(ui->pcSpeedComboBox->currentIndex()) {
			default:	target->model = Analyser::Static::PCCompatible::Model::XT;		break;
			case 1:		target->model = Analyser::Static::PCCompatible::Model::TurboXT;	break;
	}

	switch(ui->pcVideoAdaptorComboBox->currentIndex()) {
			default:	target->adaptor = Target::VideoAdaptor::MDA;		break;
			case 1:		target->adaptor = Target::VideoAdaptor::CGA;		break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_spectrum() {
	using Target = Analyser::Static::ZXSpectrum::Target;
	auto target = std::make_unique<Target>();

	switch(ui->spectrumModelComboBox->currentIndex()) {
		default:	target->model = Target::Model::SixteenK;		break;
		case 1:		target->model = Target::Model::FortyEightK;		break;
		case 2:		target->model = Target::Model::OneTwoEightK;	break;
		case 3:		target->model = Target::Model::Plus2;			break;
		case 4:		target->model = Target::Model::Plus2a;			break;
		case 5:		target->model = Target::Model::Plus3;			break;
	}

	launchTarget(std::move(target));
}

void MainWindow::start_plus4() {
	using Target = Analyser::Static::Commodore::Plus4Target;
	auto target = std::make_unique<Target>();
	target->has_c1541 = ui->plus4C1541CheckBox->isChecked();
	launchTarget(std::move(target));
}

void MainWindow::start_vic20() {
	using Target = Analyser::Static::Commodore::Vic20Target;
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

template <typename ApplierT>
void MainWindow::processAllSettings() {
	ApplierT applier;

	/* Machine selection. */
	applier(ui->machineSelectionTabs, "machineSelection");

	/* Amiga. */
	applier(ui->amigaChipRAMComboBox, "amiga.chipRAM");
	applier(ui->amigaFastRAMComboBox, "amiga.fastRAM");

	/* Apple II. */
	applier(ui->appleIIModelComboBox, "appleII.model");
	applier(ui->appleIIDiskControllerComboBox, "appleII.diskController");

	/* Apple IIgs. */
	applier(ui->appleIIgsModelComboBox, "appleIIgs.model");
	applier(ui->appleIIgsMemorySizeComboBox, "appleIIgs.memorySize");

	/* Amstrad CPC. */
	applier(ui->amstradCPCModelComboBox, "amstradcpc.model");

	/* Atari ST. */
	applier(ui->atariSTRAMComboBox, "atarist.memorySize");

	/* BBC Micro. */
	applier(ui->bbcMicroDFSCheckBox, "bbc.hasDFS");
	applier(ui->bbcMicroADFSCheckBox, "bbc.hasADFS");
	applier(ui->bbcMicroBeebSIDCheckBox, "bbc.hasBeebSID");
	applier(ui->bbcMicroSidewaysRAMCheckBox, "bbc.fillSidewaysRAM");
	applier(ui->bbcMicroSecondProcessorComboBox, "bbc.secondProcessor");

	/* Electron. */
	applier(ui->electronDFSCheckBox, "electron.hasDFS");
	applier(ui->electronADFSCheckBox, "electron.hasADFS");
	applier(ui->electronAP6CheckBox, "electron.hasAP6");
	applier(ui->electronSidewaysRAMCheckBox, "electron.fillSidewaysRAM");

	/* Enterprise. */
	applier(ui->enterpriseModelComboBox, "enterprise.model");
	applier(ui->enterpriseSpeedComboBox, "enterprise.speed");
	applier(ui->enterpriseEXOSComboBox, "enterprise.exos");
	applier(ui->enterpriseBASICComboBox, "enterprise.basic");
	applier(ui->enterpriseDOSComboBox, "enterprise.dos");

	/* Macintosh. */
	applier(ui->macintoshModelComboBox, "macintosh.model");

	/* MSX. */
	applier(ui->msxModelComboBox, "msx.model");
	applier(ui->msxRegionComboBox, "msx.region");
	applier(ui->msxDiskDriveCheckBox, "msx.hasDiskDrive");
	applier(ui->msxMSXMUSICCheckBox, "msx.hasMSXMUSIC");

	/* Oric. */
	applier(ui->oricModelComboBox, "msx.model");
	applier(ui->oricDiskInterfaceComboBox, "msx.diskInterface");

	/* Plus 4. */
	applier(ui->plus4C1541CheckBox, "plus4.hasC1541");

	/* PC Compatible. */
	applier(ui->pcSpeedComboBox, "pc.speed");
	applier(ui->pcVideoAdaptorComboBox, "pc.videoAdaptor");

	/* Vic-20 */
	applier(ui->vic20RegionComboBox, "vic20.region");
	applier(ui->vic20MemorySizeComboBox, "vic20.memorySize");
	applier(ui->vic20C1540CheckBox, "vic20.has1540");

	/* ZX80. */
	applier(ui->zx80MemorySizeComboBox, "zx80.memorySize");
	applier(ui->zx80UseZX81ROMCheckBox, "zx80.usesZX81ROM");

	/* ZX81. */
	applier(ui->zx81MemorySizeComboBox, "zx81.memorySize");

	/* ZX Spectrum. */
	applier(ui->spectrumModelComboBox, "spectrum.model");
}

void MainWindow::storeSelections() {
	struct Storer {
		Settings settings;

		void operator()(QCheckBox *const checkBox, const char *key) {
			settings.setValue(key, checkBox->isChecked());
		}
		void operator()(QComboBox *const comboBox, const char *key) {
			settings.setValue(key, comboBox->currentText());
		}
		void operator()(QTabWidget *const tabs, const char *key) {
			settings.setValue(key, tabs->currentIndex());
		}
	};
	processAllSettings<Storer>();
}

void MainWindow::restoreSelections() {
	struct Retriever {
		Settings settings;

		void operator()(QCheckBox *const checkBox, const char *key) {
			checkBox->setCheckState(settings.value(key).toBool() ? Qt::Checked : Qt::Unchecked);
		}
		void operator()(QComboBox *const comboBox, const char *key) {
			comboBox->setCurrentText(settings.value(key).toString());
		}
		void operator()(QTabWidget *const tabs, const char *key) {
			tabs->setCurrentIndex(settings.value(key).toInt());
		}
	};
	processAllSettings<Retriever>();
}

// MARK: - Activity observation

void MainWindow::addActivityObserver() {
	ledStatuses.clear();
	auto activitySource = machine->activity_source();
	if(!activitySource) return;

	setStatusBar(new QStatusBar());
	activitySource->set_activity_observer(this);
}

void MainWindow::register_led(const std::string &name, uint8_t) {
	std::lock_guard guard(ledStatusesLock);
	ledStatuses[name] = false;
	QMetaObject::invokeMethod(this, "updateStatusBarText");
}

void MainWindow::set_led_status(const std::string &name, const bool isLit) {
	std::lock_guard guard(ledStatusesLock);
	ledStatuses[name] = isLit;
	QMetaObject::invokeMethod(this, "updateStatusBarText");
}

void MainWindow::updateStatusBarText() {
	QString fullText;
	std::lock_guard guard(ledStatusesLock);
	for(const auto &pair: ledStatuses) {
		if(!fullText.isEmpty()) fullText += " | ";
		fullText += QString::fromStdString(pair.first);
		fullText += " ";
		fullText += pair.second ? "■" : "□";
	}
	statusBar()->showMessage(fullText);
}
