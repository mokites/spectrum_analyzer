
#ifndef __UI__H
#define __UI__H

#include "../utils/queue.h"
#include "../utils/logger.h"

namespace ockl {

class Ui {
public:
	void run(Queue<double>& queue, Logger& logger, double fftResolution);
};

}

#endif
