
#ifndef __ALSA__H
#define __ALSA__H

#include <string>
#include <chrono>
#include <functional>
#include <thread>
#include <atomic>

#include <alsa/asoundlib.h>

#include "utils/logger.h"
#include "utils/queue.h"
#include "defs.h"

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
public:
	Alsa(const std::string& deviceName,
			unsigned samplingRate,
			unsigned periodSize,
			Queue<SamplingType>& queue,
			const Logger& logger);
	~Alsa();

	void init();
	void start();
	void shutdown();

private:
	void initParams();
	void printInfo(::snd_pcm_hw_params_t *params);
	void threadFunction();

	::snd_pcm_t* pcmHandle;
	const std::string deviceName;

	unsigned samplingRate;
	::snd_pcm_uframes_t periodSize;
	static const snd_pcm_format_t samplingFormat = SND_PCM_FORMAT_S16_LE;

	Queue<SamplingType>& queue;

	const Logger& logger;

	std::thread* thread;
	std::atomic<bool> doShutdown;
};

} // namespace

#endif
