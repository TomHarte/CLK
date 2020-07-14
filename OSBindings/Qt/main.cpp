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

	QApplication a(argc, argv);
	MainWindow *const w = (argc > 1) ? new MainWindow(argv[1]) : new MainWindow();
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->show();

	return a.exec();
}
