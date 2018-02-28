
#include "fft.h"

namespace ockl {

Fft::
Fft(const std::function<void ()>& error_callback,
		const std::function<void ()>& data_callback,
		const Logger& logger)
: N(0),
  error_callback(error_callback),
  data_callback(data_callback),
  logger(logger),
  thread(nullptr),
  doShutdown(false),
  plan(nullptr)
{
}

Fft::
~Fft()
{
	shutdown();
}

void
Fft::
init(unsigned long N)
{
	if (plan != nullptr) {
		throw std::runtime_error("fft already initialized");
	}

	this->N = N;
	if ((N & (N - 1)) != 0) {
		LOGGER_WARNING("period size not a power of 2");
	}

	plan = ::rfftw_create_plan(N, FFTW_FORWARD, FFTW_ESTIMATE);
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
threadFunction()
{
}

} // namespace
