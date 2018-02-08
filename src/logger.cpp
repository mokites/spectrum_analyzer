
#include "logger.h"
#include <iostream>
#include <iomanip>

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
log(const std::string& text, const std::string& level) const
{
	auto now = std::chrono::system_clock::now();
	std::unique_lock<std::mutex> lock(mutex);
	buffer.emplace_back(Message{text, level, now});
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
		std::cout << oss.str() << std::endl;
	}
}

} // namespace
