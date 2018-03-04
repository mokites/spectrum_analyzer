
#ifndef __QUEUE__H
#define __QUEUE__H

#include <chrono>
#include <deque>
#include <mutex>
#include <condition_variable>

namespace ockl {

template <typename T>
class Queue {
public:
	Queue(unsigned elementSize,
			unsigned elementCount,
			std::chrono::milliseconds timeout)
	: elementSize(elementSize),
	  elementCount(elementCount),
	  timeout(timeout),
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
			cv.wait_for(lock, timeout);
		}
		if (doShutdown || pool.empty()) {
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
		cv.notify_all();
	}

	T* pop_front()
	{
		std::unique_lock<std::mutex> lock(mutex);
		if (doShutdown) {
			return nullptr;
		}
		if (queue.empty()) {
			cv.wait_for(lock, timeout);
		}
		if (doShutdown || queue.empty()) {
			return nullptr;
		}
		T* element = queue.front();
		queue.pop_front();
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

private:
	unsigned elementSize;
	unsigned elementCount;
	std::chrono::milliseconds timeout;

	std::deque<T*> pool;
	std::deque<T*> queue;

	bool doShutdown;
	std::mutex mutex;
	std::condition_variable cv;
};

} // namespace

#endif
