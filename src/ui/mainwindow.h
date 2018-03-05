
#ifndef __MAINWINDOW__H
#define __MAINWINDOW__H

#include <QtWidgets/QMainWindow>
#include <qcustomplot.h>

#include "../utils/queue.h"
#include "../utils/logger.h"

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow {
	Q_OBJECT
public:
	explicit MainWindow(ockl::Queue<double>& queue, ockl::Logger& logger, double fftResolution);
	~MainWindow();

private:
	void timerEvent(QTimerEvent*);

	Ui::MainWindow *ui;
	int timerId;

	ockl::Queue<double>& queue;
	ockl::Logger& logger;
	unsigned dataLength;

	QVector<double> x;
	QVector<double> y;
};

#endif
