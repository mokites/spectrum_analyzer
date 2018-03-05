
#include <iostream>
#include <iomanip>

#include "logger.h"

namespace ockl {

Logger::
Logger()
: doShutdown(false)
{
	thread = new std::thread(&Logger::threadFunction, this);
	pthread_setname_np(thread->native_handle(), "logger");
}

Logger::
~Logger()
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
debug(const std::string& msg) const
{
	log(msg, "DEBUG");
}

void
Logger::
info(const std::string& msg) const
{
	log(msg, "INFO");
}

void
Logger::
warning(const std::string& msg) const
{
	log(msg, "WARNING");
}

void
Logger::
error(const std::string& msg) const
{
	log(msg, "ERROR");
}

void
Logger::
log(const std::string& text, const std::string& level) const
{
	auto now = std::chrono::system_clock::now();
	Message message = Message{text, level, now, 1};
	std::unique_lock<std::mutex> lock(mutex);
	if (buffer.size() > 0 && message == buffer.back()) {
		buffer.back().recurrence++;
	} else {
		buffer.emplace_back(message);
	}
	cv.notify_all();
}

void
Logger::
threadFunction()
{
	while (true) {
		std::vector<Message> buffer;
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
flush(const std::vector<Message>& buffer)
{
	for (auto msg : buffer) {
		auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
				msg.time.time_since_epoch()) % 1000;
		auto now_c = std::chrono::system_clock::to_time_t(msg.time);
		std::ostringstream oss;
		oss << std::put_time(std::localtime(&now_c), "%Y-%m-%d %T")
			<< "." << std::setw(3) << ms.count()
			<< ": " << msg.level << ": " << msg.text;
		if (msg.recurrence > 1) {
			oss << " (message repeated " << msg.recurrence << " times)";
		}
		std::cout << oss.str() << std::endl;
	}
}

} // namespace
