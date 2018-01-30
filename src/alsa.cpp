
#include "alsa.h"
#include <sstream>
#include <stdexcept>

namespace ockl {

Alsa::
Alsa(const std::string& deviceName, unsigned int samplingRate, const Logger& logger)
: pcmHandle(nullptr),
  deviceName(deviceName),
  samplingRate(samplingRate),
  logger(logger)
{
}

void
Alsa::
init()
{
	if (pcmHandle != nullptr) {
		throw std::runtime_error("already initialized");
	}

	int result;

	result = ::snd_pcm_open(&pcmHandle, deviceName.c_str(), SND_PCM_STREAM_CAPTURE, 0);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to open device " << deviceName << ": " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	snd_pcm_hw_params_t *params;
	result = ::snd_pcm_hw_params_malloc(&params);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to allocate params_t: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	result = ::snd_pcm_hw_params_any(pcmHandle, params);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to initialize params_t: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	result = ::snd_pcm_hw_params_set_access(pcmHandle, params, SND_PCM_ACCESS_RW_INTERLEAVED);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set access type: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	result = ::snd_pcm_hw_params_set_format(pcmHandle, params, SND_PCM_FORMAT_S16_LE);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set sample format: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	unsigned int actualSamplingRate = samplingRate;
	result = ::snd_pcm_hw_params_set_rate_near(pcmHandle, params, &actualSamplingRate, nullptr);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set sampling rate: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	if (actualSamplingRate != samplingRate) {
		std::ostringstream oss;
		oss << "sampling rate set to " << actualSamplingRate << " (requested " << samplingRate << ")";
		logger.info(oss.str());
	}

	result = ::snd_pcm_hw_params_set_channels(pcmHandle, params, 1);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set number of channels: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}



}

void
Alsa::
start()
{

}

void
Alsa::
stop()
{

}

} // namespace
