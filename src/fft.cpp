
#include "fft.h"

namespace ockl {

Fft::
Fft(unsigned fftSize,
		Queue<SamplingType>& queue,
		const std::function<void ()>& error_callback,
		const Logger& logger)
: fftSize(fftSize),
  queue(queue),
  error_callback(error_callback),
  logger(logger),
  thread(nullptr),
  doShutdown(false),
  plan(nullptr)
{
}

Fft::
~Fft()
{
	if (plan == nullptr) {
		return;
	}

	if (thread != nullptr) {
		doShutdown = true;
		thread->join();
		delete thread;
		thread = nullptr;
	}

	::rfftw_destroy_plan(plan);
	plan = nullptr;
}

void
Fft::
init()
{
	if (plan != nullptr) {
		throw std::runtime_error("fft already initialized");
	}

	if ((fftSize & (fftSize - 1)) != 0) {
		LOGGER_WARNING("period size not a power of 2 - fft will be slow");
	}

	plan = ::rfftw_create_plan(fftSize, FFTW_FORWARD, FFTW_ESTIMATE);
}

void
Fft::
start()
{
	if (plan == nullptr) {
		throw std::runtime_error("alsa not initialized");
	}

	thread = new std::thread(&Fft::threadFunction, this);
}

void
Fft::
shutdown()
{
	doShutdown = true;
}

void
Fft::
threadFunction()
{
	while (!doShutdown) {
		SamplingType* buffer = queue.pop_front();
		if (buffer != nullptr) {
			LOGGER_INFO("received data");
			queue.release(buffer);
		}
	}
}

} // namespace
