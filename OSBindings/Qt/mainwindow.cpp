#include <QtWidgets>
#include <QObject>

#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "timer.h"

Timer *t;

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow)
{
	ui->setupUi(this);
	createActions();

	// Start the emulation timer. TODO: not now.
	timer = std::make_unique<QTimer>(this);
	QThread *thread = new QThread(this);
	timer->setInterval(1);
	t = new Timer;
	t->moveToThread(thread);
	connect(timer.get(), SIGNAL(timeout()), t, SLOT(tick()));
	connect(thread, SIGNAL(finished()), t, SLOT(deleteLater()));
	thread->start();
	timer->start();
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
//	if(!fileName.isEmpty())
//		loadFile(fileName);
}

MainWindow::~MainWindow() {
	// TODO: stop thread somehow?
}

