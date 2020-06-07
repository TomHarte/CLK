#include <QtWidgets>
#include <QObject>
#include <QStandardPaths>

#include "mainwindow.h"
#include "timer.h"

#include "../../Numeric/CRC.hpp"

namespace {

struct AudioEvent: public QEvent {
	AudioEvent() : QEvent(QEvent::Type::User) {}
	std::vector<int16_t> audio;
};

}

/*
	General Qt implementation notes:

	*	it seems like Qt doesn't offer a way to constrain the aspect ratio of a view by constraining
		the size of the window (i.e. you can use a custom layout to constrain a view, but that won't
		affect the window, so isn't useful for this project). Therefore the emulation window
*/

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	createActions();
	qApp->installEventFilter(this);

	// Set up the emulation timer. Bluffer's guide: the QTimer will post an
	// event to an event loop. QThread is a thread with an event loop.
	// My class, Timer, will be wired up to receive the QTimer's events.
	qTimer = std::make_unique<QTimer>(this);
	qTimer->setInterval(1);

	timerThread = std::make_unique<QThread>(this);

	timer = std::make_unique<Timer>();
	timer->moveToThread(timerThread.get());

	connect(qTimer.get(), SIGNAL(timeout()), timer.get(), SLOT(tick()));

	// Hide the missing ROMs box unless or until it's needed; grab the text it
	// began with as a prefix for future mutation.
	ui->missingROMsBox->setVisible(false);
	romRequestBaseText = ui->missingROMsBox->toPlainText();
}

void MainWindow::createActions() {
	// Create a file menu.
	QMenu *fileMenu = menuBar()->addMenu(tr("&File"));

//	QAction *newAct = new QAction(tr("&New"), this);
//	newAct->setShortcuts(QKeySequence::New);
//	newAct->setStatusTip(tr("Create a new file"));
//	connect(newAct, &QAction::triggered, this, &MainWindow::newFile);
//	fileMenu->addAction(newAct);

	// Add file option: 'Open..."
	QAction *openAct = new QAction(tr("&Open..."), this);
	openAct->setShortcuts(QKeySequence::Open);
	openAct->setStatusTip(tr("Open an existing file"));
	connect(openAct, &QAction::triggered, this, &MainWindow::open);
	fileMenu->addAction(openAct);

}

void MainWindow::open() {
	QString fileName = QFileDialog::getOpenFileName(this);
	if(!fileName.isEmpty()) {
		targets = Analyser::Static::GetTargets(fileName.toStdString());
		if(targets.empty()) {
			qDebug() << "Not media:" << fileName;
		} else {
			qDebug() << "Got media:" << fileName;
			launchMachine();
		}
	}
}

MainWindow::~MainWindow() {
	// Stop the timer by asking its QThread to exit and
	// waiting for it to do so.
	timerThread->exit();
	while(timerThread->isRunning());
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

				qDebug() << "Taking a run at " << nativeSource.c_str();

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
			ui->missingROMsBox->setVisible(false);
			uiPhase = UIPhase::RunningMachine;

			// Install user-friendly options. TODO: plus user overrides.
//			const auto configurable = machine->configurable_device();
//			if(configurable) {
//				configurable->set_options(configurable->get_options());
//			}

			// Supply the scan target.
			// TODO: in the future, hypothetically, deal with non-scan producers.
			const auto scan_producer = machine->scan_producer();
			if(scan_producer) {
				scan_producer->set_scan_target(ui->openGLWidget->getScanTarget());
			}

			// Install audio output if required.
			const auto audio_producer = machine->audio_producer();
			if(audio_producer) {
				const auto speaker = audio_producer->get_speaker();
				if(speaker) {
					const QAudioDeviceInfo &defaultDeviceInfo = QAudioDeviceInfo::defaultOutputDevice();
					QAudioFormat idealFormat = defaultDeviceInfo.preferredFormat();

					// Use the ideal format's sample rate, provide stereo as long as at least two channels
					// are available, and — at least for now — assume 512 samples/buffer is a good size.
					audioIsStereo = (idealFormat.channelCount() > 1) && speaker->get_is_stereo();
					audioIs8bit = idealFormat.sampleSize() < 16;
					const int samplesPerBuffer = 65536;
					speaker->set_output_rate(idealFormat.sampleRate(), samplesPerBuffer, audioIsStereo);

					// Adjust format appropriately, and create an audio output.
					idealFormat.setChannelCount(1 + int(audioIsStereo));
					idealFormat.setSampleSize(audioIs8bit ? 8 : 16);
					audioOutput = std::make_unique<QAudioOutput>(idealFormat, this);
					audioOutput->setBufferSize(samplesPerBuffer * (audioIsStereo ? 2 : 1) * (audioIs8bit ? 1 : 2));

					qDebug() << idealFormat;

					// Start the output.
					speaker->set_delegate(this);
					audioIODevice = audioOutput->start();
				}
			}

			// If this is a timed machine, start up the timer.
			const auto timedMachine = machine->timed_machine();
			if(timedMachine) {
				timer->setMachine(timedMachine, &machineMutex);
				timerThread->start();
				qTimer->start();
			}

		} break;

		case Machine::Error::MissingROM: {
			ui->missingROMsBox->setVisible(true);
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
	// Forward this buffrer to the QThread that QAudioOutput lives on.
	AudioEvent *event = new AudioEvent;
	event->audio = buffer;
	QApplication::instance()->postEvent(this, event);
//	const auto bytesWritten = audioIODevice->write(reinterpret_cast<const char *>(buffer.data()), qint64(buffer.size()));
//	qDebug() << bytesWritten << "; " << audioOutput->state();
//	(void)bytesWritten;
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
			qDebug() << "Should start machine";
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

		case QEvent::User: {
			const auto audioEvent = dynamic_cast<AudioEvent *>(event);
			if(audioEvent) {
				audioIODevice->write(reinterpret_cast<const char *>(audioEvent->audio.data()), qint64(audioEvent->audio.size()));
			}
		} break;

		default:
		break;
	}

	return QObject::eventFilter(obj, event);
}

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

	return true;
}
