
#ifndef __FFT__H
#define __FFT__H

#include <functional>
#include <thread>
#include <atomic>

#include <rfftw.h>

#include "logger.h"

namespace ockl {

class Fft {
public:
	Fft(unsigned fftSize,
			const std::function<void ()>& error_callback,
			const std::function<void ()>& data_callback,
			const Logger& logger);
	~Fft();

	void init();
	void start();
	void shutdown();

private:
	void threadFunction();

	unsigned long fftSize;
	const std::function<void ()> error_callback;
	const std::function<void ()> data_callback;
	const Logger& logger;
	std::thread* thread;
	std::atomic<bool> doShutdown;
	::rfftw_plan plan;
};

} // namespace

#endif
