
#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(ockl::Queue<double>& queue, ockl::Logger& logger, double fftResolution)
: QMainWindow(nullptr),
  ui(new Ui::MainWindow),
  queue(queue),
  logger(logger),
  dataLength(queue.getElementSize()),
  x(dataLength),
  y(dataLength)
{
	ui->setupUi(this);
	setGeometry(400, 250, 542, 390);

	timerId = startTimer(10);

	ui->customPlot->addGraph();

	ui->customPlot->xAxis->setLabel("Hz");
	ui->customPlot->yAxis->setLabel(""); // TODO: should be something like dB
	ui->customPlot->xAxis->setRange(0, fftResolution * dataLength);
	ui->customPlot->yAxis->setRange(0, 10000.0);

	setWindowTitle("Spectrum Analyzer");
	statusBar()->clearMessage();

	for (unsigned i = 0; i < dataLength; i++) {
		x[i] = fftResolution * i;
	}
}

MainWindow::~MainWindow()
{
	killTimer(timerId);
	delete ui;
}

void
MainWindow::
timerEvent(QTimerEvent*)
{
	double* data = queue.pop_front(true);
	if (data == nullptr) {
		return;
    }

	qCopy(data, data + dataLength, y.begin());
	queue.release(data);

	ui->customPlot->graph(0)->setData(x, y);
	ui->customPlot->replot();
}
