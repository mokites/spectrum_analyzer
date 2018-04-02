
#include <sstream>
#include <stdexcept>
#include <memory>

#include "alsa.h"

namespace ockl {

#define THROW_SND_ERROR(_msg, _errorcode)	\
	std::ostringstream _oss;				\
	_oss << _msg << ": ";					\
	_oss << ::snd_strerror(_errorcode);		\
	throw std::runtime_error(_oss.str());

Alsa::
Alsa(const std::string& deviceName,
		unsigned samplingRate,
		unsigned periodSize,
		Queue<SamplingType>& queue,
		const Logger& logger)
: pcmHandle(nullptr),
  deviceName(deviceName),
  samplingRate(samplingRate),
  periodSize(periodSize),
  queue(queue),
  logger(logger),
  thread(nullptr),
  doShutdown(false)
{
}

Alsa::
~Alsa()
{
	if (pcmHandle == nullptr) {
		return;
	}

	if (thread != nullptr) {
		doShutdown = true;
		thread->join();
		delete thread;
		thread = nullptr;
	}

	::snd_pcm_close(pcmHandle);
	pcmHandle = nullptr;
}

void
Alsa::
init()
{
	if (pcmHandle != nullptr) {
		throw std::runtime_error("alsa already initialized");
	}

	// This should have been solved a bit more elegantly, but there is a
	// dependency between the type we are pushing into the queue and the
	// sampling format of the alsa configuration.
	static_assert(sizeof(SamplingType) == 2, "");
	static_assert(samplingFormat == SND_PCM_FORMAT_S16_LE, "");

	int result = ::snd_pcm_open(&pcmHandle, deviceName.c_str(),
			SND_PCM_STREAM_CAPTURE, 0);
	if (result < 0) {
		THROW_SND_ERROR("failed to open device", result);
	}

	try {
		initParams();
	} catch (const std::runtime_error& ex) {
		::snd_pcm_close(pcmHandle);
		pcmHandle = nullptr;
		throw ex;
	}
}

void
Alsa::
initParams()
{
	::snd_pcm_hw_params_t *hwParams;
	int result = ::snd_pcm_hw_params_malloc(&hwParams);
	if (result < 0) {
		THROW_SND_ERROR("failed to allocate hw_params_t", result);
	}

	std::unique_ptr<::snd_pcm_hw_params_t, decltype(&::snd_pcm_hw_params_free)>
		hwParamsContainer(hwParams, &::snd_pcm_hw_params_free);

	result = ::snd_pcm_hw_params_any(pcmHandle, hwParams);
	if (result < 0) {
		THROW_SND_ERROR("failed to initialize hw_params_t", result);
	}

	result = ::snd_pcm_hw_params_set_access(pcmHandle, hwParams,
			SND_PCM_ACCESS_RW_INTERLEAVED);
	if (result < 0) {
		THROW_SND_ERROR("failed to set access type", result);
	}

	result = ::snd_pcm_hw_params_set_format(pcmHandle, hwParams,
			samplingFormat);
	if (result < 0) {
		THROW_SND_ERROR("failed to set sample format", result);
	}

	unsigned int actualSamplingRate = samplingRate;
	result = ::snd_pcm_hw_params_set_rate_near(pcmHandle, hwParams,
			&actualSamplingRate, nullptr);
	if (result < 0) {
		THROW_SND_ERROR("failed to set sampling rate", result);
	}
	if (actualSamplingRate != samplingRate) {
		std::ostringstream oss;
		oss << "sampling rate rejected by alsa driver: set to "
			<< actualSamplingRate << " (requested " << samplingRate << ")";
		throw std::runtime_error(oss.str());
	}

	// no stereo, only mono
	result = ::snd_pcm_hw_params_set_channels(pcmHandle, hwParams, 1);
	if (result < 0) {
		THROW_SND_ERROR("failed to set number of channels", result);
	}

	::snd_pcm_uframes_t actualPeriodSize = periodSize;
	result = ::snd_pcm_hw_params_set_period_size_near(pcmHandle, hwParams,
			&actualPeriodSize, nullptr);
	if (result < 0) {
		THROW_SND_ERROR("failed to set period size", result);
	}
	if (actualPeriodSize != periodSize) {
		// The soundcard not supporting this specific period size (probably
		// because some hw buffer is not large enough or whatever) should not
		// cause such drastic failure. Instead we should have some kind of
		// buffering here in this class.
		std::ostringstream oss;
		oss << "period size rejected by alsa driver: set to "
			<< actualPeriodSize << " (requested " << periodSize << ")";
		throw std::runtime_error(oss.str());
	}

	result = ::snd_pcm_hw_params(pcmHandle, hwParams);
	if (result < 0) {
		THROW_SND_ERROR("failed to set hw_params_t", result);
	}

	printInfo(hwParams);

	::snd_pcm_sw_params_t* swParams;
	result = ::snd_pcm_sw_params_malloc(&swParams);
	if (result < 0) {
		THROW_SND_ERROR("failed to allocate sw_params_t", result);
	}

	std::unique_ptr<::snd_pcm_sw_params_t, decltype(&::snd_pcm_sw_params_free)>
		swParamsContainer(swParams, &::snd_pcm_sw_params_free);

	result = ::snd_pcm_sw_params_current(pcmHandle, swParams);
	if (result < 0) {
		THROW_SND_ERROR("failed to initialize sw_params_t", result);
	}

	// Have at least one full periodSize of frames available when waking up.
    result = ::snd_pcm_sw_params_set_avail_min(pcmHandle, swParams, periodSize);
    if (result < 0) {
    	THROW_SND_ERROR("failed to set available min", result);
    }

    result = ::snd_pcm_sw_params(pcmHandle, swParams);
    if (result < 0) {
    	THROW_SND_ERROR("failed to set sw_params_t", result);
    }
}

void
Alsa::
printInfo(::snd_pcm_hw_params_t* params)
{
	::snd_pcm_format_t format;
	int result = ::snd_pcm_hw_params_get_format(params, &format);
	if (result < 0) {
		THROW_SND_ERROR("failed to get format description", result);
	}
	LOGGER_DEBUG("PCM format = " << ::snd_pcm_format_name(format)
			<< " (" << ::snd_pcm_format_description(format) << ")");

	unsigned int channels;
	result = ::snd_pcm_hw_params_get_channels(params, &channels);
	if (result < 0) {
		THROW_SND_ERROR("failed to get number of channels", result);
	}
	LOGGER_DEBUG("PCM channels = " << channels);

	unsigned int rate;
	result = ::snd_pcm_hw_params_get_rate(params, &rate, nullptr);
	if (result < 0) {
		THROW_SND_ERROR("failed to get sampling rate", result);
	}
	LOGGER_DEBUG("PCM sampling rate = " << rate << " Hz");

	unsigned int periodTime;
	result = ::snd_pcm_hw_params_get_period_time(params, &periodTime, nullptr);
	if (result < 0) {
		THROW_SND_ERROR("failed to get period time", result);
	}
	LOGGER_DEBUG("PCM period time (input latency) = " << periodTime << " us");

	snd_pcm_uframes_t periodSize;
	result = ::snd_pcm_hw_params_get_period_size(params, &periodSize, nullptr);
	if (result < 0) {
		THROW_SND_ERROR("failed to get period size", result);
	}
	LOGGER_DEBUG("PCM period size = " << periodSize << " frames");

	unsigned int bufferTime;
	result = ::snd_pcm_hw_params_get_buffer_time(params, &bufferTime, nullptr);
	if (result < 0) {
		THROW_SND_ERROR("failed to get buffer time", result);
	}
	LOGGER_DEBUG("PCM buffer time = " << bufferTime << " us");

	snd_pcm_uframes_t bufferSize;
	result = ::snd_pcm_hw_params_get_buffer_size(params, &bufferSize);
	if (result < 0) {
		THROW_SND_ERROR("failed to get buffer size", result);
	}
	LOGGER_DEBUG("PCM buffer size = " << bufferSize << " frames");

	unsigned int periods;
	result = ::snd_pcm_hw_params_get_periods(params, &periods, nullptr);
	if (result < 0) {
		THROW_SND_ERROR("failed to get periods", result);
	}
	LOGGER_DEBUG("PCM periods = " << periods);
}

void
Alsa::
start()
{
	if (pcmHandle == nullptr) {
		throw std::runtime_error("alsa not initialized");
	}

	thread = new std::thread(&Alsa::threadFunction, this);
	pthread_setname_np(thread->native_handle(), "alsa");
}

void
Alsa::
shutdown()
{
	doShutdown = true;
}

void
Alsa::
threadFunction()
{
	int result = ::snd_pcm_prepare(pcmHandle);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to prepare device: " << ::snd_strerror(result);
		LOGGER_ERROR(oss.str());
		return;
	}

	result = ::snd_pcm_start(pcmHandle);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to start device: " << ::snd_strerror(result);
		LOGGER_ERROR(oss.str());
		return;
	}

	while (!doShutdown) {
		result = ::snd_pcm_wait(pcmHandle, Timeout.count());
		if (result == 0) {
			continue; // timeout
		} else if (result < 0) {
			std::ostringstream oss;
			oss << "error while waiting for data: " << ::snd_strerror(result);
			LOGGER_ERROR(oss.str());
			break;
		}

		snd_pcm_sframes_t numberFrames = ::snd_pcm_avail_update(pcmHandle);
		if (numberFrames == 0) {
			std::ostringstream oss;
			oss << "device not ready?";
			LOGGER_ERROR(oss.str());
			break;
		} else if (numberFrames == -EPIPE) {
			LOGGER_ERROR("overrun occured - frames have been lost");
			// this path has never been tested - do we need to call
			// snd_pcm_prepare again?
			continue;
		} else if (numberFrames < 0) {
			std::ostringstream oss;
			oss << "error while asking for available frames: " << ::snd_strerror(numberFrames);
			LOGGER_ERROR(oss.str());
			break;
		} else if ((snd_pcm_uframes_t)numberFrames < periodSize) {
			continue;
		}

		SamplingType* buffer = queue.allocate();
		if (buffer == nullptr) {
			continue;
		}

		result = ::snd_pcm_readi(pcmHandle, buffer, periodSize);
		if (result < 0) {
			queue.release(buffer);
			std::ostringstream oss;
			oss << "error while reading from device: " << ::snd_strerror(result);
			LOGGER_ERROR(oss.str());
			break;
		}

		queue.push_back(buffer);
	}

	result = ::snd_pcm_drop(pcmHandle);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to drop device: " << ::snd_strerror(result);
		LOGGER_ERROR(oss.str());
	}

	LOGGER_INFO("alsa thread exiting");
}

} // namespace
