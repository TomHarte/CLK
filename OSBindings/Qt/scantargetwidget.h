#ifndef SCANTARGETWIDGET_H
#define SCANTARGETWIDGET_H

#include <QOpenGLWidget>

#include "../../Outputs/OpenGL/ScanTarget.hpp"
#include "../../Machines/ScanProducer.hpp"

#include "../../ClockReceiver/VSyncPredictor.hpp"

class ScanTargetWidget : public QOpenGLWidget
{
	public:
		ScanTargetWidget(QWidget *parent = nullptr);
		~ScanTargetWidget();

		// Sets the current scan producer; this scan producer will be
		// handed a suitable scan target as soon as one exists.
		void setScanProducer(MachineTypes::ScanProducer *);

		void stop();

	protected:
		void initializeGL() override;
		void resizeGL(int w, int h) override;
		void paintGL() override;

	private:
		// This should be created only once there's an OpenGL context. So it
		// can't be done at creation time.
		std::unique_ptr<Outputs::Display::OpenGL::ScanTarget> scanTarget;
		Time::VSyncPredictor vsyncPredictor;
		bool isConnected = false;
		GLuint framebuffer = 0;
		MachineTypes::ScanProducer *producer = nullptr;

	private slots:
		void vsync();
};

#endif // SCANTARGETWIDGET_H
