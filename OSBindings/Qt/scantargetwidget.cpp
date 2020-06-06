#include "scantargetwidget.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QTimer>

#include "../../ClockReceiver/TimeTypes.hpp"

ScanTargetWidget::ScanTargetWidget(QWidget *parent) : QOpenGLWidget(parent) {}
ScanTargetWidget::~ScanTargetWidget() {}

void ScanTargetWidget::initializeGL() {
	glClearColor(0.5, 0.5, 1.0, 1.0);

	// Follow each swapped frame with an additional update.
	connect(this, SIGNAL(frameSwapped()), this, SLOT(update()));
//	qDebug() << "share context: " << bool(context()->shareGroup());
}

void ScanTargetWidget::paintGL() {
	glClear(GL_COLOR_BUFFER_BIT);
	if(scanTarget) {
		scanTarget->update(width(), height());
		scanTarget->draw(width(), height());

//		static int64_t start = 0;
//		static int frames = 0;
//		if(!start) start = Time::nanos_now();
//		else {
//			++frames;
//			const int64_t now = Time::nanos_now();
//			qDebug() << double(frames) * 1e9 / double(now - start);
//		}
	}
}

void ScanTargetWidget::resizeGL(int w, int h) {
    glViewport(0,0,w,h);
}

Outputs::Display::OpenGL::ScanTarget *ScanTargetWidget::getScanTarget() {
	makeCurrent();
	if(!scanTarget) {
		scanTarget = std::make_unique<Outputs::Display::OpenGL::ScanTarget>(defaultFramebufferObject());
		QTimer::singleShot(500, this, SLOT(update()));	// TODO: obviously this is nonsense.
	}
	return scanTarget.get();
}
