#include "scantargetwidget.h"

#include <QApplication>
#include <QCursor>
#include <QDebug>
#include <QDesktopWidget>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QScreen>
#include <QTimer>

#include "../../ClockReceiver/TimeTypes.hpp"

ScanTargetWidget::ScanTargetWidget(QWidget *parent) : QOpenGLWidget(parent) {}
ScanTargetWidget::~ScanTargetWidget() {}

void ScanTargetWidget::initializeGL() {
	setDefaultClearColour();

	// Follow each swapped frame with an additional update.
	connect(this, &QOpenGLWidget::frameSwapped, this, &ScanTargetWidget::vsync);
}

void ScanTargetWidget::paintGL() {
	if(requestedRedrawTime) {
		const auto now = Time::nanos_now();
		vsyncPredictor.add_timer_jitter(now - requestedRedrawTime);
		requestedRedrawTime = 0;
	}

	// TODO: if Qt 5.14 can be guaranteed, just use window()->screen().
	const auto screenNumber = QApplication::desktop()->screenNumber(this);
	QScreen *const screen = QGuiApplication::screens()[screenNumber];

	const float newOutputScale = float(screen->devicePixelRatio());
	if(outputScale != newOutputScale) {
		outputScale = newOutputScale;
		resize();
	}
	vsyncPredictor.set_frame_rate(float(screen->refreshRate()));

	glClear(GL_COLOR_BUFFER_BIT);

	// Gmynastics ahoy: if a producer has been specified or previously connected then:
	//
	//	(i) if it's a new producer, generate a new scan target and pass it on;
	//	(ii) in any case, check whether the underlyiung framebuffer has changed; and
	//	(iii) draw.
	//
	// The slightly convoluted scan target forwarding arrangement works around an issue
	// with QOpenGLWidget under macOS, which I did not fully diagnose, in which creating
	// a scan target in ::initializeGL did not work (and no other arrangement really works
	// with regard to starting up).
	if(isConnected || producer) {
		if(producer) {
			isConnected = true;
			framebuffer = defaultFramebufferObject();
			scanTarget = std::make_unique<Outputs::Display::OpenGL::ScanTarget>(framebuffer);
			producer->set_scan_target(scanTarget.get());
			producer = nullptr;
		}

		// Qt reserves the right to change the framebuffer object due to window resizes or if setParent is called;
		// therefore check whether it has changed.
		const auto newFramebuffer = defaultFramebufferObject();
		if(framebuffer != newFramebuffer) {
			framebuffer = newFramebuffer;
			scanTarget->set_target_framebuffer(framebuffer);
		}

		vsyncPredictor.begin_redraw();
		scanTarget->update(scaledWidth, scaledHeight);
		scanTarget->draw(scaledWidth, scaledHeight);
		glFinish();	// Make sure all costs are properly accounted for in the vsync predictor.
		vsyncPredictor.end_redraw();
	}
}

void ScanTargetWidget::vsync() {
	if(!isConnected) return;

	vsyncPredictor.announce_vsync();

	const auto time_now = Time::nanos_now();
	requestedRedrawTime = vsyncPredictor.suggested_draw_time();
	const auto delay_time = (requestedRedrawTime - time_now) / 1'000'000;
	if(delay_time > 0) {
		QTimer::singleShot(delay_time, this, SLOT(repaint()));
	} else {
		requestedRedrawTime = 0;
		repaint();
	}
}

void ScanTargetWidget::resizeGL(int w, int h) {
	if(rawWidth != w || rawHeight != h) {
		rawWidth = w;
		rawHeight = h;
		resize();
	}
}

void ScanTargetWidget::resize() {
	const int newScaledWidth = int(float(rawWidth) * outputScale);
	const int newScaledHeight = int(float(rawHeight) * outputScale);

	if(newScaledWidth != scaledWidth || newScaledHeight != scaledHeight) {
		scaledWidth = newScaledWidth;
		scaledHeight = newScaledHeight;
		glViewport(0, 0, scaledWidth, scaledHeight);
	}
}

void ScanTargetWidget::setScanProducer(MachineTypes::ScanProducer *producer) {
	this->producer = producer;
	repaint();
}

void ScanTargetWidget::stop() {
	makeCurrent();
	scanTarget.reset();
	isConnected = false;
	setDefaultClearColour();
	vsyncPredictor.pause();
	requestedRedrawTime = 0;
	repaint();
}

void ScanTargetWidget::setDefaultClearColour() {
	// Retain the default background colour.
	const QColor backgroundColour = palette().color(QWidget::backgroundRole());
	glClearColor(backgroundColour.redF(), backgroundColour.greenF(), backgroundColour.blueF(), 1.0);
}

void ScanTargetWidget::setMouseDelegate(MouseDelegate *delegate) {
	if(!delegate && mouseIsCaptured) {
		releaseMouse();
	}
	mouseDelegate = delegate;
	setMouseTracking(delegate);
}

void ScanTargetWidget::keyPressEvent(QKeyEvent *event) {
	if(mouseIsCaptured && event->key() == Qt::Key_Escape && event->modifiers()&Qt::ControlModifier) {
		releaseMouse();

		QCursor cursor;
		cursor.setShape(Qt::ArrowCursor);
		setCursor(cursor);
	}
}

void ScanTargetWidget::releaseMouse() {
	QOpenGLWidget::releaseMouse();
	mouseIsCaptured = false;
	mouseDelegate->setMouseIsCaptured(false);
}

void ScanTargetWidget::mousePressEvent(QMouseEvent *event) {
	if(mouseDelegate) {
		if(!mouseIsCaptured) {
			mouseIsCaptured = true;
			grabMouse();

			QCursor cursor;
			cursor.setPos(mapToGlobal(QPoint(width() / 2, height() / 2)));
			cursor.setShape(Qt::BlankCursor);
			setCursor(cursor);

			mouseDelegate->setMouseIsCaptured(true);
		} else {
			setMouseButtonPressed(event->button(), true);
		}
	}
}

void ScanTargetWidget::mouseReleaseEvent(QMouseEvent *event) {
	if(mouseDelegate && !mouseIsCaptured) {
		setMouseButtonPressed(event->button(), false);
	}
}

void ScanTargetWidget::setMouseButtonPressed(Qt::MouseButton button, bool isPressed) {
	switch(button) {
		default: break;
		case Qt::LeftButton:	mouseDelegate->setButtonPressed(0, isPressed);	break;
		case Qt::RightButton:	mouseDelegate->setButtonPressed(1, isPressed);	break;
		case Qt::MiddleButton:	mouseDelegate->setButtonPressed(2, isPressed);	break;
	}
}

void ScanTargetWidget::mouseMoveEvent(QMouseEvent *event) {
	// Recentre the mouse cursor upon every move if it is currently captured.
	if(mouseDelegate && mouseIsCaptured) {
		const QPoint centre = QPoint(width() / 2, height() / 2);
		const QPoint vector = event->pos() - centre;

		mouseDelegate->moveMouse(vector);

		QCursor::setPos(mapToGlobal(centre));
	}
}

bool ScanTargetWidget::isMouseCaptured() {
	return mouseIsCaptured;
}
