
#ifndef __ALSA__H
#define __ALSA__H

#include <string>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>

#include <alsa/asoundlib.h>

#include "logger.h"

namespace ockl {

/**
 * Notes about how ALSA works:
 *  sampling size = sampling format = 2 bytes (S16_LE = 16 bit little endian)
 *  frame size = {sampling size} * #channels
 *  period size = {frame size} * {#frames between hardware interrupts}
 *  period time = time between hardware interrupts
 *  buffer size = #periods * {period size}, this is the buffer inside the driver
 *  input latency = {#frames in period} * {length of frame}, length of frame is {sampling rate}^{-1}
 */

class Alsa {
private:
	static const std::chrono::milliseconds TIMEOUT;

public:
	Alsa(const std::string& deviceName,
			unsigned int samplingRate,
			std::chrono::microseconds periodTime,
			const std::function<void ()>& error_callback,
			const std::function<void (short*, int)>& data_callback,
			const Logger& logger);
	~Alsa();

	void init();
	void start();
	void shutdown();

	unsigned getBufferSize() const;

private:
	void initParams();
	void printInfo(::snd_pcm_hw_params_t *params);
	void threadFunction();

	::snd_pcm_t* pcmHandle;
	const std::string deviceName;
	unsigned int samplingRate;
	std::chrono::microseconds periodTime;
	::snd_pcm_uframes_t periodSize;
	unsigned int periods;
	const std::function<void ()> error_callback;
	const std::function<void (short*, int)> data_callback;
	const Logger& logger;
	const snd_pcm_format_t samplingFormat;
	std::thread* thread;
	std::atomic<bool> doShutdown;
};

} // namespace

#endif
