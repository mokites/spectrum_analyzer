
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <signal.h>
#include <atomic>
#include <chrono>
#include <mutex>
#include <condition_variable>

#include "utils/logger.h"
#include "utils/queue.h"
#include "utils/watchdog.h"
#include "alsa.h"
#include "fft.h"
#include "defs.h"
#include "ui/ui.h"

void usage(const char* arg0)
{
	std::cerr << "usage: " << arg0
			<< " <pcm device> <sampling rate [Hz]> <input length [ms]>"
			<< std::endl;
}

const unsigned QueueLength = 10;

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
	// return counter or counter divided by 2, depending on what is nearer
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

	ockl::Logger logger;

	// Convert input length [us] (period time in alsa speak) to sample
	// count [frames] (period size in alsa speak, aka fft length) and round
	// it up/down such that it is a power of 2 (which makes the fft faster).
	unsigned sampleCount = roundToNearestPowerOf2((unsigned)
			((uint64_t) inputLength.count() * (uint64_t) samplingRate / 1e6));
	double fftResolution = (double) samplingRate / (double) sampleCount;
	unsigned fftBinCount = sampleCount / 2 + 1;

	LOGGER_INFO("sample count: " << sampleCount << " [frames]");
	LOGGER_INFO("input length: " <<
			(double) sampleCount / (double) samplingRate * 1000 << " [ms]");
	LOGGER_INFO("fft resolution: " << fftResolution << " [Hz/bin]");

	ockl::Queue<ockl::SamplingType> fftQueue(sampleCount, QueueLength,
			ockl::Timeout);

	ockl::Queue<double> uiQueue(fftBinCount, QueueLength,
			ockl::Timeout);

	ockl::Watchdog watchdog(std::chrono::seconds(10),
			std::chrono::duration_cast<std::chrono::milliseconds>(inputLength),
			logger);
	watchdog.addQueue(&fftQueue, "fft");
	watchdog.addQueue(&uiQueue, "ui");

	ockl::Alsa alsa(
			argv[1],
			samplingRate,
			sampleCount,
			fftQueue,
			logger);

	ockl::Fft fft(sampleCount,
			fftQueue,
			uiQueue,
			logger);

	try {
		alsa.init();
		fft.init();
	} catch (const std::runtime_error& ex) {
		LOGGER_ERROR("init failed: " << ex.what());
		return -2;
	}

	try {
		alsa.start();
		fft.start();
	} catch (const std::runtime_error& ex) {
		LOGGER_ERROR("start failed: " << ex.what());
		return -3;
	}

	ockl::Ui ui;
	ui.run(uiQueue, logger, fftResolution);

	LOGGER_INFO("shutting down");

	watchdog.shutdown();
	fftQueue.shutdown();
	uiQueue.shutdown();
	fft.shutdown();
	alsa.shutdown();

	return 0;
}
