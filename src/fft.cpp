
#include "fft.h"

namespace ockl {

Fft::
Fft(unsigned fftSize,
		Queue<SamplingType>& inQueue,
		Queue<double>& outQueue,
		const std::function<void ()>& error_callback,
		const Logger& logger)
: fftSize(fftSize),
  inQueue(inQueue),
  outQueue(outQueue),
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
	pthread_setname_np(thread->native_handle(), "fft");
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
	fftw_real* in = new fftw_real[fftSize];
	fftw_real* out = new fftw_real[fftSize];

	while (!doShutdown) {
		double* spectrum = outQueue.allocate();
		if (spectrum == nullptr) {
			continue;
		}

		SamplingType* inBuffer = inQueue.pop_front();
		if (inBuffer == nullptr) {
			outQueue.release(spectrum);
			continue;
		}

		std::copy(inBuffer, inBuffer + fftSize, in);
		inQueue.release(inBuffer);

		rfftw_one(plan, in, out);

		spectrum[0] = out[0] * out[0];
		for (unsigned i = 0; i < (fftSize + 1) / 2; i++) {
			spectrum[i] = out[i] * out[i] + out[fftSize - i] * out[fftSize - i];
		}
		if (fftSize % 2 == 0) {
			spectrum[fftSize / 2] = out[fftSize / 2] * out[fftSize / 2];
		}

		//outQueue.push_back(spectrum);
		outQueue.release(spectrum);
	}

	delete[] in;
	delete[] out;
}

} // namespace
