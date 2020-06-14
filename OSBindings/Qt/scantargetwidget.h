#ifndef SCANTARGETWIDGET_H
#define SCANTARGETWIDGET_H

#include <QOpenGLWidget>

#include "../../Outputs/OpenGL/ScanTarget.hpp"
#include "../../ClockReceiver/VSyncPredictor.hpp"

class ScanTargetWidget : public QOpenGLWidget
{
	public:
		ScanTargetWidget(QWidget *parent = nullptr);
		~ScanTargetWidget();

		Outputs::Display::OpenGL::ScanTarget *getScanTarget();

	protected:
		void initializeGL() override;
		void resizeGL(int w, int h) override;
		void paintGL() override;

	private:
		// This should be created only once there's an OpenGL context. So it
		// can't be done at creation time.
		std::unique_ptr<Outputs::Display::OpenGL::ScanTarget> scanTarget;
		Time::VSyncPredictor vsync_predictor_;

	private slots:
		void vsync();
};

#endif // SCANTARGETWIDGET_H
