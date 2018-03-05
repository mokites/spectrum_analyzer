
#include <QtWidgets/QApplication>

#include "ui.h"
#include "mainwindow.h"

namespace ockl {

void
Ui::
run(Queue<double>& queue, Logger& logger, double fftResolution)
{
	int argc = 0;
	QApplication a(argc, nullptr);
	MainWindow w(queue, logger, fftResolution);
	w.show();
	a.exec();
}

}
