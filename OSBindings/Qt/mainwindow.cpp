#include <QtWidgets>
#include <QObject>
#include <QStandardPaths>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "timer.h"

#include "../../Analyser/Static/StaticAnalyser.hpp"
#include "../../Machines/Utility/MachineForTarget.hpp"

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
		const auto targets = Analyser::Static::GetTargets(fileName.toStdString());
		if(targets.empty()) {
			qDebug() << "Not media:" << fileName;
		} else {
			qDebug() << "Got media:" << fileName;

			const QStringList appDataLocations = QStandardPaths::standardLocations(QStandardPaths::AppDataLocation);
			ROMMachine::ROMFetcher rom_fetcher = [&appDataLocations]
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
						auto data = std::make_unique<std::vector<uint8_t>>();

						fseek(file, 0, SEEK_END);
						data->resize(std::ftell(file));
						fseek(file, 0, SEEK_SET);
						size_t read = fread(data->data(), 1, data->size(), file);
						fclose(file);

						if(read == data->size()) {
							results.push_back(std::move(data));
						} else {
							results.push_back(nullptr);
						}
					} else {
						results.push_back(nullptr);
					}
				}
				return results;
			};
			Machine::Error error;
			std::unique_ptr<Machine::DynamicMachine> machine(Machine::MachineForTargets(targets, rom_fetcher, error));
		}
	}
}

MainWindow::~MainWindow() {
	// Stop the timer by asking its QThread to exit and
	// waiting for it to do so.
	timerThread->exit();
	while(timerThread->isRunning());
}

