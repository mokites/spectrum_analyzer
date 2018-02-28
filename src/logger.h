
#ifndef __LOGGER_H
#define __LOGGER_H

#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace ockl {

#define _LOG(level, stream)				\
		do {							\
			std::ostringstream oss;		\
			oss << stream;				\
			logger.level(oss.str());	\
		} while (false)

#define LOGGER_DEBUG(stream) _LOG(debug, stream)
#define LOGGER_INFO(stream) _LOG(info, stream)
#define LOGGER_WARNING(stream) _LOG(warning, stream)
#define LOGGER_ERROR(stream) _LOG(error, stream)

class Logger {
public:
	Logger();

	void shutdown();

	void debug(const std::string& msg) const;
	void info(const std::string& msg) const;
	void warning(const std::string& msg) const;
	void error(const std::string& msg) const;

private:
	void log(const std::string& msg, const std::string& level) const;
	void threadFunction();

	bool doShutdown;

	struct Message {
		std::string text;
		std::string level;
		std::chrono::system_clock::time_point time;
		unsigned recurrence;

		bool operator ==(const Message &other) const {
			return text == other.text && level == other.level;
		}
	};

	void flush(const std::vector<Message>& buffer);

	mutable std::vector<Message> buffer;
	std::thread* thread;
	mutable std::mutex mutex;
	mutable std::condition_variable cv;
};

} // namespace

#endif
