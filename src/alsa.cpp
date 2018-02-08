
#include "alsa.h"
#include "raii.h"
#include <sstream>
#include <stdexcept>

namespace ockl {

const std::chrono::milliseconds Alsa::TIMEOUT = std::chrono::milliseconds(100);

Alsa::
Alsa(const std::string& deviceName,
		unsigned int samplingRate,
		std::chrono::microseconds periodLength,
		const std::function<void ()>& error_callback,
		const std::function<void (short*, int)>& data_callback,
		const Logger& logger)
: pcmHandle(nullptr),
  deviceName(deviceName),
  samplingRate(samplingRate),
  periodLength(periodLength),
  error_callback(error_callback),
  data_callback(data_callback),
  logger(logger),
  samplingFormat(SND_PCM_FORMAT_S16_LE),
  periods(3),
  thread(nullptr),
  doShutdown(false)
{
}

void
Alsa::
init()
{
	if (pcmHandle != nullptr) {
		throw std::runtime_error("already initialized");
	}

	int result = ::snd_pcm_open(&pcmHandle, deviceName.c_str(),
			SND_PCM_STREAM_CAPTURE, 0);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to open device " << deviceName << ": "
			<< ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	try {
		initHwParams();
	} catch (const std::runtime_error& ex) {
		::snd_pcm_close(pcmHandle);
		pcmHandle = nullptr;
		throw ex;
	}
}

void
Alsa::
initHwParams()
{
	::snd_pcm_hw_params_t *params;
	int result = ::snd_pcm_hw_params_malloc(&params);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to allocate params_t: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	RAII<::snd_pcm_hw_params_t> paramsContainer(params,
			[] (auto params) { ::snd_pcm_hw_params_free(params); });

	result = ::snd_pcm_hw_params_any(pcmHandle, params);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to initialize params_t: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	result = ::snd_pcm_hw_params_set_access(pcmHandle, params,
			SND_PCM_ACCESS_RW_INTERLEAVED);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set access type: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	result = ::snd_pcm_hw_params_set_format(pcmHandle, params, samplingFormat);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set sample format: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	unsigned int actualSamplingRate = samplingRate;
	result = ::snd_pcm_hw_params_set_rate_near(pcmHandle, params,
			&actualSamplingRate, nullptr);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set sampling rate: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	if (actualSamplingRate != samplingRate) {
		LOGGER_INFO("sampling rate set to " << actualSamplingRate
			<< " (requested " << samplingRate << ")");
		samplingRate = actualSamplingRate;
	}

	result = ::snd_pcm_hw_params_set_channels(pcmHandle, params, 1);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set number of channels: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	unsigned int actualPeriodLength = periodLength.count();
	result = ::snd_pcm_hw_params_set_period_time_near(pcmHandle, params,
			&actualPeriodLength, nullptr);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set period time: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	if (std::chrono::microseconds(actualPeriodLength) != periodLength) {
		LOGGER_INFO("period length set to " << actualPeriodLength
				<< " (requested " << periodLength.count() << ")");
		periodLength = std::chrono::microseconds(actualPeriodLength);
	}

	unsigned int actualPeriods = periods;
	result = ::snd_pcm_hw_params_set_periods_near(pcmHandle, params,
			&actualPeriods, nullptr);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set number of periods: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	if (actualPeriods != periods) {
		LOGGER_INFO("number of periods set to " << actualPeriods
				<< " (requested " << periods << ")");
		periods = actualPeriods;
	}

	result = ::snd_pcm_hw_params(pcmHandle, params);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to set params_t: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}

	printInfo(params);
}

void
Alsa::
printInfo(::snd_pcm_hw_params_t *params)
{
	LOGGER_INFO("PCM handle name = " << ::snd_pcm_name(pcmHandle));
	LOGGER_INFO("PCM state = " << ::snd_pcm_state_name(snd_pcm_state(pcmHandle)));

	::snd_pcm_format_t format;
	int result = ::snd_pcm_hw_params_get_format(params, &format);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to get format description: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	LOGGER_INFO("PCM format = " << ::snd_pcm_format_name(format)
			<< " (" << ::snd_pcm_format_description(format) << ")");

	unsigned int channels;
	result = ::snd_pcm_hw_params_get_channels(params, &channels);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to get number of channels: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	LOGGER_INFO("PCM channels = " << channels);

	unsigned int rate;
	result = ::snd_pcm_hw_params_get_rate(params, &rate, nullptr);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to get sampling rate: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	LOGGER_INFO("PCM sampling rate = " << rate << " Hz");

	unsigned int periodTime;
	result = ::snd_pcm_hw_params_get_period_time(params, &periodTime, nullptr);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to get period time: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	LOGGER_INFO("PCM period time (input latency) = " << periodTime << " us");

	snd_pcm_uframes_t periodSize;
	result = ::snd_pcm_hw_params_get_period_size(params, &periodSize, nullptr);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to get period size: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	LOGGER_INFO("PCM period size = " << periodSize << " frames");

	unsigned int bufferTime;
	result = ::snd_pcm_hw_params_get_buffer_time(params, &bufferTime, nullptr);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to get buffer time: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	LOGGER_INFO("PCM buffer time = " << bufferTime << " us");

	snd_pcm_uframes_t bufferSize;
	result = ::snd_pcm_hw_params_get_buffer_size(params, &bufferSize);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to get buffer size: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	LOGGER_INFO("PCM buffer size = " << bufferSize << " frames");

	unsigned int periods;
	result = ::snd_pcm_hw_params_get_periods(params, &periods, nullptr);
	if (result < 0) {
		std::ostringstream oss;
		oss << "failed to get periods: " << ::snd_strerror(result);
		throw std::runtime_error(oss.str());
	}
	LOGGER_INFO("PCM periods = " << periods);
}

void
Alsa::
start()
{
	if (pcmHandle == nullptr) {
		throw std::runtime_error("not initialized");
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

	doShutdown = true;
	thread->join();
	delete thread;

	::snd_pcm_close(pcmHandle);
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

	short buffer[4096];

	while (!doShutdown) {
		result = ::snd_pcm_wait(pcmHandle, TIMEOUT.count());
		if (result == 0) {
			LOGGER_INFO("timeout 1");
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
			LOGGER_INFO("timeout 2");
			continue; // weird, but never mind
		} else if (numberFrames == -EPIPE) {
			LOGGER_ERROR("overrun occured - frames have been lost");
			continue;
		} else if (numberFrames < 0) {
			std::ostringstream oss;
			oss << "error while asking for available frames: " << ::snd_strerror(numberFrames);
			LOGGER_ERROR(oss.str());
			error_callback();
			break;
		}

		result = ::snd_pcm_readi(pcmHandle, buffer, numberFrames);
		if (result < 0) {
			std::ostringstream oss;
			oss << "error while reading from device: " << ::snd_strerror(result);
			LOGGER_ERROR(oss.str());
			error_callback();
			break;
		}

		data_callback(buffer, numberFrames);
	}

	LOGGER_INFO("alsa thread exiting");
}

} // namespace
