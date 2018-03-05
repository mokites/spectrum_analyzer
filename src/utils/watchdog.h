
#ifndef __WATCHDOG__H
#define __WATCHDOG__H

#include <chrono>
#include <set>
#include <thread>
#include <mutex>
#include <condition_variable>
#include <utility>

#include "queue.h"
#include "logger.h"

namespace ockl {

class Watchdog {
public:
	/**
	 * \param interval     frequency of the watch dog
	 * \param maxHoldTime  threshold for the hold time inside the queue
	 */
	Watchdog(std::chrono::milliseconds interval,
			std::chrono::milliseconds maxHoldTime,
			const Logger& logger)
	: interval(interval),
	  maxHoldTime(maxHoldTime),
	  doShutdown(false),
	  logger(logger)
	{
		thread = new std::thread(&Watchdog::threadFunction, this);
		pthread_setname_np(thread->native_handle(), "watchdog");
	}

	~Watchdog()
	{
		shutdown();
		thread->join();
		delete thread;
	}

	void shutdown()
	{
		std::unique_lock<std::mutex> lock(mutex);
		doShutdown = true;
		cv.notify_all();
	}

	void addQueue(QueueStatistics* queue, const std::string& consumerName)
	{
		std::unique_lock<std::mutex> lock(mutex);
		queues.insert(std::make_pair(queue, consumerName));
	}

private:
	void threadFunction()
	{
		while (true) {
			std::set<std::pair<QueueStatistics*, std::string>> queues;
			{
				std::unique_lock<std::mutex> lock(mutex);
				if (doShutdown) {
					break;
				}
				cv.wait_for(lock, interval);
				if (doShutdown) {
					break;
				}
				queues.insert(this->queues.begin(), this->queues.end());
			}
			for (auto element : queues) {
				statCheck(element.first, element.second);
			}
		}
	}

	void statCheck(QueueStatistics* queue, const std::string& consumerName)
	{
		unsigned timeouts;
		std::chrono::microseconds holdTime;
		unsigned length;
		queue->getStats(timeouts, holdTime, length);
		if (timeouts > 0 || holdTime > maxHoldTime) {
			auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
					holdTime);
			LOGGER_WARNING(consumerName << " too slow: timeouts " << timeouts
					<< ", cycle time "
					<< ms.count() << " [ms], length " << length);
		}
	}

	std::chrono::milliseconds interval;
	std::chrono::milliseconds maxHoldTime;
	std::set<std::pair<QueueStatistics*, std::string>> queues;

	std::thread* thread;
	bool doShutdown;
	std::mutex mutex;
	std::condition_variable cv;

	const Logger& logger;
};

}

#endif
