
#ifndef __QUEUE__H
#define __QUEUE__H

#include <chrono>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <utility>

#include "defs.h"

namespace ockl {

template <typename T>
class Queue {
public:
	Queue(unsigned elementSize,
			unsigned elementCount)
	: elementSize(elementSize),
	  elementCount(elementCount),
	  producerTimeouts(0),
	  maxCycleTime(0),
	  doShutdown(false)
	{
		for (unsigned i = 0; i < elementCount; i++) {
			pool.push_back((T*) malloc(sizeof(T) * elementSize));
		}
	}

	~Queue()
	{
		std::unique_lock<std::mutex> lock(mutex);
		doShutdown = true;
		while (pool.size() + queue.size() != elementCount) {
			cv.wait(lock);
		}
		for (unsigned i = 0; i < pool.size(); i++) {
			free(pool[i]);
		}
		for (unsigned i = 0; i < queue.size(); i++) {
			free(queue[i]);
		}
	}

	void shutdown()
	{
		std::unique_lock<std::mutex> lock(mutex);
		doShutdown = true;
	}

	T* allocate()
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (doShutdown) {
			return nullptr;
		}
		if (pool.empty()) {
			cv.wait_for(lock, Timeout);
		}
		if (doShutdown) {
			return nullptr;
		}
		if (pool.empty()) {
			producerTimeouts++;
			return nullptr;
		}
		T* element = pool.front();
		pool.pop_front();
		return element;
	}

	void push_back(T* data)
	{
		if (data == nullptr) {
			throw new std::runtime_error("push_back(nullptr)");
		}
		std::unique_lock<std::mutex> lock(mutex);
		queue.push_back(data);
		times.push_back(std::chrono::system_clock::now());
		cv.notify_all();
	}

	T* pop_front()
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (doShutdown) {
			return nullptr;
		}
		if (queue.empty()) {
			cv.wait_for(lock, Timeout);
		}
		if (doShutdown) {
			return nullptr;
		}
		if (queue.empty()) {
			return nullptr;
		}
		T* element = queue.front();
		queue.pop_front();
		auto insertionTime = times.front();
		times.pop_front();
		auto cycleTime = std::chrono::system_clock::now() - insertionTime;
		if (cycleTime > maxCycleTime) {
			maxCycleTime = std::chrono::duration_cast<std::chrono::microseconds>(
					cycleTime);
		}
		return element;
	}

	void release(T* data)
	{
		if (data == nullptr) {
			throw new std::runtime_error("release(nullptr)");
		}
		std::unique_lock<std::mutex> lock(mutex);
		pool.push_back(data);
		cv.notify_all();
	}

	void getStats(unsigned& producerTimeouts,
			std::chrono::microseconds& maxCycleTime,
			unsigned& queueLength) {
		std::unique_lock<std::mutex> lock(mutex);

		producerTimeouts = this->producerTimeouts;
		maxCycleTime = this->maxCycleTime;
		queueLength = queue.size();

		this->producerTimeouts = 0;
		this->maxCycleTime = std::chrono::microseconds(0);
	}

private:
	unsigned elementSize;
	unsigned elementCount;

	unsigned producerTimeouts;
	std::chrono::microseconds maxCycleTime;

	std::deque<T*> pool;
	std::deque<T*> queue;
	std::deque<std::chrono::system_clock::time_point> times;

	bool doShutdown;
	mutable std::mutex mutex;
	std::condition_variable cv;
};

} // namespace

#endif
