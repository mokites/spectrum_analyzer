
#ifndef __LOGGER_H
#define __LOGGER_H

#include <string>
#include <vector>
#include <thread>
#include <mutex>
#include <condition_variable>

namespace ockl {

class Logger {
public:
	Logger();

	void shutdown();

	void info(const std::string& msg) const;
	void error(const std::string& msg) const;

private:
	void log(const std::string& msg, const std::string& level) const;
	void threadFunction();
	void flush(const std::vector<std::string>& buffer);
	std::string getTime() const;

	bool doShutdown;
	mutable std::vector<std::string> buffer;
	std::thread* thread;
	mutable std::mutex mutex;
	mutable std::condition_variable cv;
};

} // namespace

#endif
