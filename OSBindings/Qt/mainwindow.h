#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <memory>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow {
		Q_OBJECT

		void createActions();

	public:
		MainWindow(QWidget *parent = nullptr);
		~MainWindow();

	private:
		std::unique_ptr<Ui::MainWindow> ui;
		std::unique_ptr<QTimer> timer;

	private slots:
		void open();
};

#endif // MAINWINDOW_H
