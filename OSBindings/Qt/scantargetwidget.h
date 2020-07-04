#ifndef SCANTARGETWIDGET_H
#define SCANTARGETWIDGET_H

#include <QOpenGLWidget>

#include "../../Outputs/OpenGL/ScanTarget.hpp"
#include "../../Machines/ScanProducer.hpp"

#include "../../ClockReceiver/VSyncPredictor.hpp"

class ScanTargetWidget : public QOpenGLWidget {
	public:
		ScanTargetWidget(QWidget *parent = nullptr);
		~ScanTargetWidget();

		/// Sets the current scan producer; this scan producer will be
		/// handed a suitable scan target as soon as one exists.
		void setScanProducer(MachineTypes::ScanProducer *);

		/// Destructs the current scan target
		void stop();

		struct MouseDelegate {
			virtual void setMouseIsCaptured(bool) = 0;
			virtual void moveMouse(QPoint) = 0;
			virtual void setButtonPressed(int index, bool isPressed) = 0;
		};
		/// If a delegate is assigned then this widget will respond to clicks by capturing
		/// the mouse, unless and until either ::stop() is called or ctrl+escape is pressed.
		/// Mouse events can be tracked by the main window while the mouse is captured.
		void setMouseDelegate(MouseDelegate *);

		/// @returns @c true if the mouse is currently captured; @c false otherwise.
		bool isMouseCaptured();

	protected:
		void initializeGL() override;
		void resizeGL(int w, int h) override;
		void paintGL() override;

		void mousePressEvent(QMouseEvent *) override;
		void mouseReleaseEvent(QMouseEvent *) override;
		void mouseMoveEvent(QMouseEvent *) override;
		void keyPressEvent(QKeyEvent *) override;

		void releaseMouse();
		void setMouseButtonPressed(Qt::MouseButton, bool);

	private:
		// This should be created only once there's an OpenGL context. So it
		// can't be done at creation time.
		std::unique_ptr<Outputs::Display::OpenGL::ScanTarget> scanTarget;
		Time::VSyncPredictor vsyncPredictor;
		bool isConnected = false;
		GLuint framebuffer = 0;
		MachineTypes::ScanProducer *producer = nullptr;

		Time::Nanos requestedRedrawTime = 0;

		void setDefaultClearColour();

		int rawWidth = 0, rawHeight = 0;
		int scaledWidth = 0, scaledHeight = 0;
		float outputScale = 1.0f;
		void resize();

		MouseDelegate *mouseDelegate = nullptr;
		bool mouseIsCaptured = false;

	private slots:
		void vsync();
};

#endif // SCANTARGETWIDGET_H
