
#ifndef __QUEUE__H
#define __QUEUE__H

#include <chrono>
#include <deque>
#include <mutex>
#include <condition_variable>
#include <utility>

namespace ockl {

class QueueStatistics {
public:
	virtual ~QueueStatistics() {}

	virtual void getStats(unsigned& producerTimeouts,
			std::chrono::microseconds& holdTime,
			unsigned& queueLength) = 0;
};

template <typename T>
class Queue : public QueueStatistics {
public:
	/**
	 * \param elementSize   how many T's should be in one element
	 * \param elementCount  how many elements should the queue provide
	 * \param timeout       a timeout for when the producer is faster than
	 *                      the consumer or vice versa.
	 */
	Queue(unsigned elementSize,
			unsigned elementCount,
			std::chrono::milliseconds timeout)
	: elementSize(elementSize),
	  elementCount(elementCount),
	  timeout(timeout),
	  producerTimeouts(0),
	  maxHoldTime(0),
	  doShutdown(false)
	{
		for (unsigned i = 0; i < elementCount; i++) {
			pool.push_back((T*) malloc(sizeof(T) * elementSize));
		}
	}

	virtual ~Queue()
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
			cv.wait_for(lock, timeout);
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

	T* pop_front(bool nowait = false)
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (doShutdown) {
			return nullptr;
		}
		if (queue.empty()) {
			if (nowait) {
				return nullptr;
			}
			cv.wait_for(lock, timeout);
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
		auto holdTime = std::chrono::system_clock::now() - insertionTime;
		if (holdTime > maxHoldTime) {
			maxHoldTime = std::chrono::duration_cast<std::chrono::microseconds>(
					holdTime);
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
			std::chrono::microseconds& holdTime,
			unsigned& queueLength) override
	{
		std::unique_lock<std::mutex> lock(mutex);

		producerTimeouts = this->producerTimeouts;
		holdTime = this->maxHoldTime;
		queueLength = queue.size();

		this->producerTimeouts = 0;
		this->maxHoldTime = std::chrono::microseconds(0);
	}

	unsigned getElementSize()
	{
		return elementSize;
	}

private:
	unsigned elementSize;
	unsigned elementCount;

	std::chrono::milliseconds timeout;

	unsigned producerTimeouts;
	std::chrono::microseconds maxHoldTime;

	std::deque<T*> pool;
	std::deque<T*> queue;
	std::deque<std::chrono::system_clock::time_point> times;

	bool doShutdown;
	mutable std::mutex mutex;
	std::condition_variable cv;
};

} // namespace

#endif
