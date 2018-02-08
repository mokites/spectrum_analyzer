
#include "logger.h"
#include "alsa.h"
#include <cstdlib>
#include <stdexcept>
#include <iostream>
#include <signal.h>
#include <atomic>

void usage(const char* arg0) {
	std::cerr << "usage: " << arg0 << " <device> <rate [Hz]> <window [us]>" << std::endl;
}

std::atomic<bool> shutdown;
std::mutex mutex;
std::condition_variable cv;

ockl::Logger logger;

void shutdown_signal(int) {
	std::unique_lock<std::mutex> lock(mutex);
	shutdown = true;
	cv.notify_all();
}

int main(int argc, char** argv) {
	if (argc < 4) {
		usage(argv[0]);
		return -1;
	}

	unsigned int samplingRate;
	std::chrono::microseconds periodTime;

	try {
		samplingRate = std::stoi(argv[2]);
		periodTime = std::chrono::microseconds(std::stoi(argv[3]));
	} catch (...) {
		std::cerr << "failed to parse arguments" << std::endl;
		usage(argv[0]);
		return -1;
	}

	::signal(SIGINT, shutdown_signal);

	ockl::Alsa alsa(
			argv[1],
			samplingRate,
			periodTime,
			[] () { shutdown_signal(0); },
			[] (short*, int size) { LOGGER_INFO("received " << size << " shorts of data"); },
			logger);

	try {
		alsa.init();
		alsa.start();
	} catch (const std::runtime_error& ex) {
		LOGGER_ERROR(ex.what());
	}

	while (true) {
		std::unique_lock<std::mutex> lock(mutex);
		if (shutdown) {
			break;
		}
		cv.wait(lock);
	}

	LOGGER_INFO("shutting down");

	alsa.shutdown();
	logger.shutdown();

	return 0;
}
