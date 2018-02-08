
#ifndef __LOGGER_H
#define __LOGGER_H

#include <string>
#include <vector>
#include <sstream>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace ockl {

#define LOGGER_INFO(STREAM)      \
	do {                         \
		std::ostringstream oss;  \
		oss << STREAM;           \
		logger.info(oss.str());  \
	} while (false);

#define LOGGER_ERROR(STREAM)     \
	do {                         \
		std::ostringstream oss;  \
		oss << STREAM;           \
		logger.error(oss.str()); \
	} while (false);

class Logger {
public:
	Logger();

	void shutdown();

	void info(const std::string& msg) const;
	void error(const std::string& msg) const;

private:
	void log(const std::string& msg, const std::string& level) const;
	void threadFunction();

	bool doShutdown;

	struct Message {
		std::string text;
		std::string level;
		std::chrono::system_clock::time_point time;
	};

	void flush(const std::vector<Message>& buffer);

	mutable std::vector<Message> buffer;
	std::thread* thread;
	mutable std::mutex mutex;
	mutable std::condition_variable cv;
};

} // namespace

#endif
