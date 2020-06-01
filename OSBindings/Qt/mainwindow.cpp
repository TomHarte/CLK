#include <QtWidgets>
#include <QObject>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "timer.h"

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
		qDebug() << "Should open" << fileName;
	}
}

MainWindow::~MainWindow() {
	// Stop the timer by asking its QThread to exit and
	// waiting for it to do so.
	timerThread->exit();
	while(timerThread->isRunning());
}

