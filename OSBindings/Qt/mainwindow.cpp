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
		affect the window, so isn't useful for this project). Therefore the emulation window
*/

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	createActions();

	// Set up the emulation timer. Bluffer's guide: the QTimer will post an
	// event to an event loop. QThread is a thread with an event loop.
	// My class, Timer, will be wired up to receive the QTimer's events.
	qTimer = std::make_unique<QTimer>(this);
	qTimer->setInterval(1);

	timerThread = std::make_unique<QThread>(this);

	timer = std::make_unique<Timer>();
	timer->moveToThread(timerThread.get());

	connect(qTimer.get(), SIGNAL(timeout()), timer.get(), SLOT(tick()));

	// Start the thread and timer.
	// TODO: not until there's actually something to display.
	timerThread->start();
	qTimer->start();

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
	std::unique_ptr<Machine::DynamicMachine> machine(Machine::MachineForTargets(targets, rom_fetcher, error));

	switch(error) {
		default:
			ui->missingROMsBox->setVisible(false);
			uiPhase = UIPhase::RunningMachine;

			// TODO: launch machine.
		break;

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
