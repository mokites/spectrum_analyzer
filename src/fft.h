
#ifndef __FFT__H
#define __FFT__H

#include <functional>
#include <thread>
#include <atomic>

#include <rfftw.h>

#include "utils/logger.h"
#include "utils/queue.h"
#include "defs.h"

namespace ockl {

class Fft {
public:
	Fft(unsigned fftSize,
			Queue<SamplingType>& inQueue,
			Queue<double>& outQueue,
			const Logger& logger);
	~Fft();

	void init();
	void start();
	void shutdown();

private:
	void threadFunction();

	unsigned long fftSize;

	Queue<SamplingType>& inQueue;
	Queue<double>& outQueue;

	const Logger& logger;

	std::thread* thread;
	std::atomic<bool> doShutdown;
	::rfftw_plan plan;
};

} // namespace

#endif
