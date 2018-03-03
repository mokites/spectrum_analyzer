
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <signal.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "logger.h"
#include "raii.h"
#include "alsa.h"
#include "fft.h"
#include "queue.h"

void usage(const char* arg0)
{
	std::cerr << "usage: " << arg0
			<< " <pcm device> <sampling rate [Hz]> <input length [ms]>"
			<< std::endl;
}

bool shutdown;
std::mutex mutex;
std::condition_variable cv;
ockl::Logger logger;
const unsigned QueueLength = 10;

void shutdown_signal(int)
{
	std::unique_lock<std::mutex> lock(mutex);
	shutdown = true;
	cv.notify_all();
}

unsigned
roundToNearestPowerOf2(unsigned value)
{
	uint64_t counter = 2;
	while (counter < value) {
		counter *= 2; // no danger of overflow, unsigned is at most 32 bit
	}
	if (counter > std::numeric_limits<unsigned>::max()) {
		std::ostringstream oss;
		oss << "out of range when computing nearest power of 2 for value " << value;
		throw std::runtime_error(oss.str());
	}
	// return counter or counter/2, depending on what is nearer
	return (counter - value < value - counter / 2 ? counter : counter / 2);
}

int main(int argc, char** argv)
{
	if (argc < 4) {
		usage(argv[0]);
		return -1;
	}

	unsigned samplingRate;
	std::chrono::microseconds inputLength;

	try {
		samplingRate = std::stoi(argv[2]);
		inputLength = std::chrono::microseconds(std::stoi(argv[3]) * 1000);
	} catch (...) {
		std::cerr << "failed to parse arguments" << std::endl;
		usage(argv[0]);
		return -1;
	}

	// Convert input length [us] (period time in alsa speak) to sample
	// count [frames] (period size in alsa speak, aka fft length) and round
	// it up/down such that it is a power of 2 (which makes the fft faster).
	unsigned sampleCount = roundToNearestPowerOf2((unsigned)
			((uint64_t) inputLength.count() * (uint64_t) samplingRate / 1e6));

	LOGGER_INFO("sample count: " << sampleCount);
	LOGGER_INFO("fft resolution: " << (double) samplingRate / (double) sampleCount);

	ockl::Queue<short> fftQueue(sampleCount, QueueLength,
			std::chrono::milliseconds(10));

	ockl::Alsa alsa(
			argv[1],
			samplingRate,
			sampleCount,
			fftQueue,
			[] () { shutdown_signal(0); },
			[] (short*, int size) { LOGGER_INFO("received " << size << " frames"); },
			logger);

	ockl::Fft fft(sampleCount,
			fftQueue,
			[] () { shutdown_signal(0); },
			[] () { },
			logger);

	try {
		alsa.init();
		fft.init();
	} catch (const std::runtime_error& ex) {
		LOGGER_ERROR("init failed: " << ex.what());
		logger.shutdown();
		return -2;
	}

	try {
		alsa.start();
		fft.start();
	} catch (const std::runtime_error& ex) {
		LOGGER_ERROR("start failed: " << ex.what());
		logger.shutdown();
		return -3;
	}

	::signal(SIGINT, shutdown_signal);

	std::unique_lock<std::mutex> lock(mutex);
	while (!shutdown) {
		cv.wait(lock);
	}

	LOGGER_INFO("shutting down");

	fft.shutdown();
	alsa.shutdown();

	return 0;
}
