#include <QtWidgets>
#include <QObject>
#include <QStandardPaths>

#include "mainwindow.h"
#include "timer.h"

#include "../../Numeric/CRC.hpp"

/*
	General Qt implementation notes:

	*	it seems like Qt doesn't offer a way to constrain the aspect ratio of a view by constraining
		the size of the window (i.e. you can use a custom layout to constrain a view, but that won't
		affect the window, so isn't useful for this project). Therefore the emulation window resizes freely.
*/

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	createActions();
	qApp->installEventFilter(this);

	timer = std::make_unique<Timer>(this);

	setVisibleWidgetSet(WidgetSet::MachinePicker);
	romRequestBaseText = ui->missingROMsBox->toPlainText();
}

void MainWindow::createActions() {
	// Create a file menu.
	QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

	// Add file option: 'New...'
	QAction *newAct = new QAction(tr("&New"), this);
	newAct->setShortcuts(QKeySequence::New);
	newAct->setStatusTip(tr("Create a new file"));
	connect(newAct, &QAction::triggered, this, &MainWindow::newFile);
	fileMenu->addAction(newAct);

	// Add file option: 'Open...'
	QAction *openAct = new QAction(tr("&Open..."), this);
	openAct->setShortcuts(QKeySequence::Open);
	openAct->setStatusTip(tr("Open an existing file"));
	connect(openAct, &QAction::triggered, this, &MainWindow::open);
	fileMenu->addAction(openAct);

	// Add Help menu, with an 'About...' option.
	QMenu *helpMenu = menuBar()->addMenu(tr("&Help"));
	QAction *aboutAct = helpMenu->addAction(tr("&About"), this, &MainWindow::about);
	aboutAct->setStatusTip(tr("Show the application's About box"));

	// Link up the start machine button.
	connect(ui->startMachineButton, &QPushButton::clicked, this, &MainWindow::startMachine);
}

void MainWindow::open() {
	QString fileName = QFileDialog::getOpenFileName(this);
	if(!fileName.isEmpty()) {
		targets = Analyser::Static::GetTargets(fileName.toStdString());
		if(!targets.empty()) {
			launchMachine();
		}
	}
}

void MainWindow::newFile() {
}

void MainWindow::about() {
	QMessageBox::about(this, tr("About Clock Signal"),
		tr(	"<p>Clock Signal is an emulator of various platforms by "
			"<a href=\"mailto:thomas.harte@gmail.com\">Thomas Harte</a>.</p>"

			"<p>This emulator is offered under the MIT licence; its source code "
			"is available from <a href=\"https://github.com/tomharte/CLK\">GitHub</a>.</p>"

			"<p>This port is experimental, especially with regard to latency; "
			"please don't hesitate to provide feedback.</p>"
	));
}

MainWindow::~MainWindow() {
	// Stop the timer; stopping this first ensures the machine won't attempt
	// to write to the audioOutput while it is being shut down.
	timer.reset();

	// Stop the audio output, and its thread.
	if(audioOutput) {
		audioThread.performAsync([this] {
			audioOutput->stop();
		});
		audioThread.stop();
	}
}

// MARK: Machine launch.

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

	switch(error) {
		default: {
			// TODO: correct assumptions herein that this is the first machine to be
			// assigned to this window.
			setVisibleWidgetSet(WidgetSet::RunningMachine);
			uiPhase = UIPhase::RunningMachine;

			// Supply the scan target.
			// TODO: in the future, hypothetically, deal with non-scan producers.
			const auto scan_producer = machine->scan_producer();
			if(scan_producer) {
				scan_producer->set_scan_target(ui->openGLWidget->getScanTarget());
			}

			// Install audio output if required.
			const auto audio_producer = machine->audio_producer();
			if(audio_producer) {
				static constexpr size_t samplesPerBuffer = 256;	// TODO: select this dynamically.
				const auto speaker = audio_producer->get_speaker();
				if(speaker) {
					const QAudioDeviceInfo &defaultDeviceInfo = QAudioDeviceInfo::defaultOutputDevice();
					QAudioFormat idealFormat = defaultDeviceInfo.preferredFormat();

					// Use the ideal format's sample rate, provide stereo as long as at least two channels
					// are available, and — at least for now — assume a good buffer size.
					audioIsStereo = (idealFormat.channelCount() > 1) && speaker->get_is_stereo();
					audioIs8bit = idealFormat.sampleSize() < 16;
					idealFormat.setChannelCount(1 + int(audioIsStereo));
					idealFormat.setSampleSize(audioIs8bit ? 8 : 16);

					speaker->set_output_rate(idealFormat.sampleRate(), samplesPerBuffer, audioIsStereo);
					speaker->set_delegate(this);

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

			// If this is a timed machine, start up the timer.
			const auto timedMachine = machine->timed_machine();
			if(timedMachine) {
				timer->startWithMachine(timedMachine, &machineMutex);
			}

		} break;

		case Machine::Error::MissingROM: {
			setVisibleWidgetSet(WidgetSet::ROMRequester);
			uiPhase = UIPhase::RequestingROMs;

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
		case UIPhase::NoFileSelected:
			// Treat exactly as a File -> Open... .
		break;

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
						QDir dir(QString::fromStdString(path));
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

		case UIPhase::RunningMachine:
			// Attempt to insert into the running machine.
		break;
	}
}

// MARK: Input capture.

bool MainWindow::eventFilter(QObject *obj, QEvent *event) {
	switch(event->type()) {
		case QEvent::KeyPress:
		case QEvent::KeyRelease: {
			const auto keyEvent = static_cast<QKeyEvent *>(event);
			if(!processEvent(keyEvent)) {
				return false;
			}
		} break;

		default:
		break;
	}

	return QObject::eventFilter(obj, event);
}

void MainWindow::setVisibleWidgetSet(WidgetSet set) {
	// The volume slider is never visible by default; a running machine
	// will show and hide it dynamically.
	ui->volumeSlider->setVisible(false);

	// Show or hide the missing ROMs box.
	ui->missingROMsBox->setVisible(set == WidgetSet::ROMRequester);

	// Show or hide the various machine-picking chrome.
	ui->machineSelectionTabs->setVisible(set == WidgetSet::MachinePicker);
	ui->startMachineButton->setVisible(set == WidgetSet::MachinePicker);
	ui->topTipLabel->setVisible(set == WidgetSet::MachinePicker);

	// Set appropriate focus if necessary; e.g. this ensures that machine-picker
	// widgets aren't still selectable after a machine starts.
	if(set != WidgetSet::MachinePicker) {
		ui->openGLWidget->setFocus();
	}
}

// MARK: - Event Processing

bool MainWindow::processEvent(QKeyEvent *event) {
	if(!machine) return true;

	// First version: support keyboard input only.
	const auto keyboardMachine = machine->keyboard_machine();
	if(!keyboardMachine) return true;

#define BIND2(qtKey, clkKey) case Qt::qtKey: key = Inputs::Keyboard::Key::clkKey; break;
#define BIND(key) BIND2(Key_##key, key)

	Inputs::Keyboard::Key key;
	switch(event->key()) {
		default: return true;

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

	std::unique_lock lock(machineMutex);
	keyboardMachine->get_keyboard().set_key_pressed(key, event->text()[0].toLatin1(), event->type() == QEvent::KeyPress);

	return false;
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
		qDebug() << #x;					\
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
