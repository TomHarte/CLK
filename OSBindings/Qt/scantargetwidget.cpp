#include "scantargetwidget.h"

#include <QDebug>
#include <QOpenGLContext>
#include <QTimer>

#include "../../ClockReceiver/TimeTypes.hpp"

ScanTargetWidget::ScanTargetWidget(QWidget *parent) : QOpenGLWidget(parent) {}
ScanTargetWidget::~ScanTargetWidget() {}

void ScanTargetWidget::initializeGL() {
	// Retain the default background colour.
	const QColor backgroundColour = palette().color(QWidget::backgroundRole());
	glClearColor(backgroundColour.redF(), backgroundColour.greenF(), backgroundColour.blueF(), 1.0);

	// Follow each swapped frame with an additional update.
	connect(this, &QOpenGLWidget::frameSwapped, this, &ScanTargetWidget::vsync);
}

void ScanTargetWidget::paintGL() {
	glClear(GL_COLOR_BUFFER_BIT);
	if(isConnected) {
		// Qt reserves the right to change the framebuffer object due to window resizes or if setParent is called;
		// therefore check whether it has changed.
		const auto newFramebuffer = defaultFramebufferObject();
		if(framebuffer != newFramebuffer) {
			framebuffer = newFramebuffer;
			scanTarget->set_target_framebuffer(framebuffer);
		}

		vsyncPredictor.begin_redraw();
		scanTarget->update(width(), height());
		scanTarget->draw(width(), height());
		vsyncPredictor.end_redraw();
	}
}

void ScanTargetWidget::vsync() {
	vsyncPredictor.announce_vsync();

	const auto time_now = Time::nanos_now();
	const auto delay_time = ((vsyncPredictor.suggested_draw_time() - time_now) / 1'000'000) - 5;	// TODO: the extra 5 is a random guess.
	if(delay_time > 0) {
		QTimer::singleShot(delay_time, this, SLOT(repaint()));
	} else {
		repaint();
	}
}

void ScanTargetWidget::resizeGL(int w, int h) {
	glViewport(0, 0, w, h);
}

Outputs::Display::OpenGL::ScanTarget *ScanTargetWidget::getScanTarget() {
	makeCurrent();
	if(!scanTarget) {
		isConnected = true;
		framebuffer = defaultFramebufferObject();
		scanTarget = std::make_unique<Outputs::Display::OpenGL::ScanTarget>(framebuffer);
	}
	return scanTarget.get();
}
