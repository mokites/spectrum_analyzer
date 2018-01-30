
#ifndef __ALSA__H
#define __ALSA__H

#include <string>
#include <alsa/asoundlib.h>
#include "logger.h"

namespace ockl {

class Alsa {
public:
	Alsa(const std::string& deviceName, unsigned int samplingRate, const Logger& logger);

	void init();
	void start();
	void stop();

private:
	snd_pcm_t* pcmHandle;
	const std::string deviceName;
	const unsigned int samplingRate;
	const Logger& logger;
};

} // namespace

#endif
