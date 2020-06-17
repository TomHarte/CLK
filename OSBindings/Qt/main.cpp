#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
	// "Calling QSurfaceFormat::setDefaultFormat() before constructing the
	// QApplication instance is mandatory on some platforms ... when an
	// OpenGL core profile context is requested."
	QSurfaceFormat format;
	format.setVersion(3, 2);
	format.setProfile(QSurfaceFormat::CoreProfile);
	format.setDepthBufferSize(0);
	format.setStencilBufferSize(0);
	QSurfaceFormat::setDefaultFormat(format);

	// TODO: something with QCommandLineParser to accept a file to launch.

	QApplication a(argc, argv);
	MainWindow w;
	w.show();
	return a.exec();
}
