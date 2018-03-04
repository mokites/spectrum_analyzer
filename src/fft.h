
#ifndef __FFT__H
#define __FFT__H

#include <functional>
#include <thread>
#include <atomic>

#include <rfftw.h>

#include "logger.h"
#include "queue.h"
#include "defs.h"

namespace ockl {

class Fft {
public:
	Fft(unsigned fftSize,
			Queue<SamplingType>& queue,
			const std::function<void ()>& error_callback,
			const Logger& logger);
	~Fft();

	void init();
	void start();
	void shutdown();

private:
	void threadFunction();

	unsigned long fftSize;

	Queue<SamplingType>& queue;

	const std::function<void ()> error_callback;

	const Logger& logger;

	std::thread* thread;
	std::atomic<bool> doShutdown;
	::rfftw_plan plan;
};

} // namespace

#endif
