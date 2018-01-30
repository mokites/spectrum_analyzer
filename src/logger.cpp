
#include "logger.h"
#include <iostream>
#include <iomanip>
#include <sstream>

namespace ockl {

Logger::
Logger()
: doShutdown(false)
{
	thread = new std::thread(&Logger::threadFunction, this);
}

void
Logger::
shutdown()
{
	{
		std::unique_lock<std::mutex> lock(mutex);
		doShutdown = true;
		cv.notify_all();
	}
	thread->join();
	delete thread;
}

void
Logger::
info(const std::string& msg) const
{
	log(msg, "INFO");
}

void
Logger::
error(const std::string& msg) const
{
	log(msg, "ERROR");
}

void
Logger::
log(const std::string& msg, const std::string& level) const
{
	std::unique_lock<std::mutex> lock(mutex);
	buffer.push_back(getTime() + " " + level + ": " + msg);
	cv.notify_all();
}

void
Logger::
threadFunction()
{
	while (true) {
		std::vector<std::string> buffer;
		{
			std::unique_lock<std::mutex> lock(mutex);
			if (this->buffer.empty()) {
				if (doShutdown) {
					return;
				}
				cv.wait(lock);
			}
			buffer.swap(this->buffer);
		}
		flush(buffer);
	}
}

void
Logger::
flush(const std::vector<std::string>& buffer)
{
	for (auto msg : buffer) {
		std::cout << msg << std::endl;
	}
}

std::string
Logger::
getTime() const
{
	auto now = std::chrono::system_clock::now();
	auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
	auto now_c = std::chrono::system_clock::to_time_t(now);
	std::ostringstream oss;
	oss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %T") << "." << std::setw(3) << ms.count();
	return oss.str();
}

} // namespace
