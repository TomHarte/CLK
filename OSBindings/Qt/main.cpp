#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
	// "Calling QSurfaceFormat::setDefaultFormat() before constructing the
	// QApplication instance is mandatory on some platforms ... when an
	// OpenGL core profile context is requested."
	QSurfaceFormat format;

#ifndef __APPLE__
	// This project has a fully-native Mac port; therefore the Qt version isn't
	// actually built for Apple devices in any meaningful capacity. But it's useful
	// to maintain.
	//
	// Sadly macOS is quite a hostile platform for OpenGL development at this point,
	// and has never supported OpenGL ES on the desktop. So there, and there only,
	// use full-fat desktop OpenGL.
	//
	// Using ES in most places gives this project much better compatibility with
	// Raspberry Pis, with various virtualisers, etc. Thanks to WebGL's basis in
	// OpenGL ES there just seems to be a lot more lingering support there.
	format.setVersion(3, 0);
	format.setRenderableType(QSurfaceFormat::RenderableType::OpenGLES);
#else
	format.setVersion(3, 2);
	format.setProfile(QSurfaceFormat::CoreProfile);
#endif
	format.setDepthBufferSize(0);
	format.setStencilBufferSize(0);
	format.setAlphaBufferSize(0);
	// format.setSwapBehavior(QSurfaceFormat::SingleBuffer);
	QSurfaceFormat::setDefaultFormat(format);

	QApplication a(argc, argv);
	MainWindow *const w = (argc > 1) ? new MainWindow(argv[1]) : new MainWindow();
	w->setAttribute(Qt::WA_DeleteOnClose);
	w->show();

	return a.exec();
}
