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
	connect(this, &QOpenGLWidget::frameSwapped, this, &ScanTargetWidget::vsync);
//	qDebug() << "share context: " << bool(context()->shareGroup());
}

void ScanTargetWidget::paintGL() {
	glClear(GL_COLOR_BUFFER_BIT);
	if(scanTarget) {
		vsync_predictor_.begin_redraw();
		scanTarget->update(width(), height());
		scanTarget->draw(width(), height());
		vsync_predictor_.end_redraw();

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

void ScanTargetWidget::vsync() {
	vsync_predictor_.announce_vsync();

	const auto time_now = Time::nanos_now();
	const auto delay_time = ((vsync_predictor_.suggested_draw_time() - time_now) / 1'000'000) - 5;	// TODO: the extra 5 is a random guess.
	if(delay_time > 0) {
		QTimer::singleShot(delay_time, this, SLOT(repaint()));
	} else {
		repaint();
	}
}

void ScanTargetWidget::resizeGL(int w, int h) {
	glViewport(0,0,w,h);
}

Outputs::Display::OpenGL::ScanTarget *ScanTargetWidget::getScanTarget() {
	makeCurrent();
	if(!scanTarget) {
		scanTarget = std::make_unique<Outputs::Display::OpenGL::ScanTarget>(defaultFramebufferObject());
	}
	return scanTarget.get();
}
