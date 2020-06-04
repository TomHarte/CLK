#include "scantargetwidget.h"

ScanTargetWidget::ScanTargetWidget(QWidget *parent) : QOpenGLWidget(parent) {}
ScanTargetWidget::~ScanTargetWidget() {}

void ScanTargetWidget::initializeGL() {
	scanTarget = std::make_unique<Outputs::Display::OpenGL::ScanTarget>(defaultFramebufferObject());
	glClearColor(0.5, 0.5, 1.0, 1.0);
}

void ScanTargetWidget::paintGL() {
	glClear(GL_COLOR_BUFFER_BIT);
}

void ScanTargetWidget::resizeGL(int w, int h) {
    glViewport(0,0,w,h);
}

Outputs::Display::OpenGL::ScanTarget *ScanTargetWidget::getScanTarget() {
	return scanTarget.get();
}
