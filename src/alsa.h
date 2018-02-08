
#ifndef __ALSA__H
#define __ALSA__H

#include <string>
#include <alsa/asoundlib.h>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>
#include "logger.h"

namespace ockl {

class Alsa {
private:
	static const std::chrono::milliseconds TIMEOUT;

public:
	Alsa(const std::string& deviceName,
			unsigned int samplingRate,
			std::chrono::microseconds periodLength,
			const std::function<void ()>& error_callback,
			const std::function<void (short*, int)>& data_callback,
			const Logger& logger);

	void init();
	void start();
	void shutdown();

private:
	void initHwParams();
	void printInfo(::snd_pcm_hw_params_t *params);
	void threadFunction();

	::snd_pcm_t* pcmHandle;
	const std::string deviceName;
	unsigned int samplingRate;
	std::chrono::microseconds periodLength;
	const std::function<void ()>& error_callback;
	const std::function<void (short*, int)>& data_callback;
	const Logger& logger;
	const snd_pcm_format_t samplingFormat;
	unsigned int periods;
	std::thread* thread;
	std::atomic<bool> doShutdown;
};

} // namespace

#endif
