
#include <sstream>
#include <stdexcept>

#include "alsa.h"
#include "raii.h"

namespace ockl {

const std::chrono::milliseconds Alsa::TIMEOUT = std::chrono::milliseconds(100);

#define THROW_SND_ERROR(_msg, _errorcode)	\
	std::ostringstream _oss;				\
	_oss << _msg << ": ";					\
	_oss << ::snd_strerror(_errorcode);		\
	throw std::runtime_error(_oss.str());

Alsa::
Alsa(const std::string& deviceName,
		unsigned int samplingRate,
		std::chrono::microseconds periodTime,
		const std::function<void ()>& error_callback,
		const std::function<void (short*, int)>& data_callback,
		const Logger& logger)
: pcmHandle(nullptr),
  deviceName(deviceName),
  samplingRate(samplingRate),
  periodTime(periodTime),
  periodSize(0),
  periods(2),
  error_callback(error_callback),
  data_callback(data_callback),
  logger(logger),
  samplingFormat(SND_PCM_FORMAT_S16_LE),
  thread(nullptr),
  doShutdown(false)
{
}

Alsa::
~Alsa()
{
	shutdown();
}

void
Alsa::
init()
{
	if (pcmHandle != nullptr) {
		throw std::runtime_error("alsa already initialized");
	}

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

	RAII<::snd_pcm_hw_params_t> hwParamsContainer(hwParams,
			[] (auto params) { ::snd_pcm_hw_params_free(params); });

	result = ::snd_pcm_hw_params_any(pcmHandle, hwParams);
	if (result < 0) {
		THROW_SND_ERROR("failed to initialize hw_params_t", result);
	}

	result = ::snd_pcm_hw_params_set_access(pcmHandle, hwParams,
			SND_PCM_ACCESS_RW_INTERLEAVED);
	if (result < 0) {
		THROW_SND_ERROR("failed to set access type", result);
	}

	result = ::snd_pcm_hw_params_set_format(pcmHandle, hwParams, samplingFormat);
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
		LOGGER_INFO("sampling rate set to " << actualSamplingRate
			<< " (requested " << samplingRate << ")");
		samplingRate = actualSamplingRate;
	}

	// no stereo, only mono
	result = ::snd_pcm_hw_params_set_channels(pcmHandle, hwParams, 1);
	if (result < 0) {
		THROW_SND_ERROR("failed to set number of channels", result);
	}

	// convert period time to period size (in frames) such that
	// the size is a power of 2 (which will speedup the fft).
	::snd_pcm_uframes_t requestedPeriodSize = (unsigned long)
			((uint64_t) periodTime.count() * (uint64_t) samplingRate / 1e6);
	::snd_pcm_uframes_t roundedPeriodSize =
			roundToNearestPowerOf2(requestedPeriodSize);
	::snd_pcm_uframes_t actualPeriodSize = roundedPeriodSize;
	result = ::snd_pcm_hw_params_set_period_size_near(pcmHandle, hwParams,
			&actualPeriodSize, nullptr);
	if (result < 0) {
		THROW_SND_ERROR("failed to set period size", result);
	}
	if (requestedPeriodSize != actualPeriodSize) {
		LOGGER_INFO("period size set to " << actualPeriodSize
				<< " frames (requested " << requestedPeriodSize
				<< ", rounded " << roundedPeriodSize << ")");
	}
	periodSize = actualPeriodSize;

	// configure the buffer size inside the driver
	unsigned int actualPeriods = periods;
	result = ::snd_pcm_hw_params_set_periods_near(pcmHandle, hwParams,
			&actualPeriods, nullptr);
	if (result < 0) {
		THROW_SND_ERROR("failed to set number of periods", result);
	}
	if (actualPeriods != periods) {
		LOGGER_INFO("number of periods set to " << actualPeriods
				<< " (requested " << periods << ")");
		periods = actualPeriods;
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

	RAII<::snd_pcm_sw_params_t> swParamsContainer(swParams,
				[] (auto params) { ::snd_pcm_sw_params_free(params); });

	result = ::snd_pcm_sw_params_current(pcmHandle, swParams);
	if (result < 0) {
		THROW_SND_ERROR("failed to initialize sw_params_t", result);
	}

	// have at least one full periodSize of frames available when waking up
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
}

void
Alsa::
shutdown()
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

unsigned
Alsa::
getBufferSize() const
{
	if (pcmHandle == nullptr) {
		throw std::runtime_error("not initialized");
	}

	return periodSize;
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
		error_callback();
		return;
	}

	result = ::snd_pcm_start(pcmHandle);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to start device: " << ::snd_strerror(result);
		LOGGER_ERROR(oss.str());
		error_callback();
		return;
	}

	// Verify that the selected sampling format is compatible with our buffer
	// type. This should be a compile-time check (or some fancy dynamic type thingy)
	if (samplingFormat != SND_PCM_FORMAT_S16_LE) {
		LOGGER_ERROR("sampling format changed");
		error_callback();
		return;
	}
	short* buffer = new short[periodSize];

	while (!doShutdown) {
		result = ::snd_pcm_wait(pcmHandle, TIMEOUT.count());
		if (result == 0) {
			continue; // timeout
		} else if (result < 0) {
			std::ostringstream oss;
			oss << "error while waiting for data: " << ::snd_strerror(result);
			LOGGER_ERROR(oss.str());
			error_callback();
			break;
		}

		snd_pcm_sframes_t numberFrames = ::snd_pcm_avail_update(pcmHandle);
		if (numberFrames == 0) {
			std::ostringstream oss;
			oss << "device not ready?";
			LOGGER_ERROR(oss.str());
			error_callback();
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
			error_callback();
			break;
		} else if ((snd_pcm_uframes_t)numberFrames < periodSize) {
			continue;
		}

		result = ::snd_pcm_readi(pcmHandle, buffer, periodSize);
		if (result < 0) {
			std::ostringstream oss;
			oss << "error while reading from device: " << ::snd_strerror(result);
			LOGGER_ERROR(oss.str());
			error_callback();
			break;
		}

		data_callback(buffer, periodSize);
	}

	delete[] buffer;

	result = ::snd_pcm_drop(pcmHandle);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to drop device: " << ::snd_strerror(result);
		LOGGER_ERROR(oss.str());
	}

	LOGGER_INFO("alsa thread exiting");
}

} // namespace
