#pragma once

#include "../utils/threadpool.hpp"
#include <functional>
#include <future>


template<typename> class CallerSync;
template<typename> class CallerLazy;
template<typename> class CallerAsync;
template<typename> class CallerAuto;
template<typename> class CallerThread;
template<typename> class CallerThreadPool;

//synchronous calls
template<typename Callback, typename... Args>
class CallerSync<Callback(Args...)> {
public:
	std::shared_future<Callback> operator() (const std::function<Callback(Args...)> &callback, Args... args) {
		std::packaged_task<Callback(Args...)> task(callback);
		const std::shared_future<Callback> &f = task.get_future();
		task(args...);
		return f;
	}
};
template<typename... Args>
class CallerSync<void(Args...)> {
public:
	std::shared_future<int> operator() (const std::function<void(Args...)> &callback, Args... args) {
		auto closure = [=](Args... args)->int { callback(args...); return 0; };
		std::packaged_task<int(Args...)> task(closure);
		const std::shared_future<int> &f = task.get_future();
		task(args...);
		return f;
	}
};

//synchronous lazy calls
template<typename Callback, typename... Args>
class CallerLazy<Callback(Args...)> {
public:
	std::shared_future<Callback> operator() (const std::function<Callback(Args...)> &callback, Args... args) {
		return std::async(std::launch::deferred, callback, args...);
	}
};
template<typename... Args>
class CallerLazy<void(Args...)> {
public:
	std::shared_future<int> operator() (const std::function<void(Args...)> &callback, Args... args) {
		auto closure = [=](Args... args)->int { callback(args...); return 0; };
		return std::async(std::launch::deferred, closure, args...);
	}
};

//asynchronous calls with std::launch::async (spawns a thread)
template<typename Callback, typename... Args>
class CallerAsync<Callback(Args...)> {
public:
	std::shared_future<Callback> operator() (const std::function<Callback(Args...)> &callback, Args... args) {
		return std::async(std::launch::async, callback, args...);
	}
};
template<typename... Args>
class CallerAsync<void(Args...)> {
public:
	std::shared_future<int> operator() (const std::function<void(Args...)> &callback, Args... args) {
		auto closure = [=](Args... args)->int { callback(args...); return 0; };
		return std::async(std::launch::async, closure, args...);
	}
};

//asynchronous or synchronous calls at the runtime convenience
template<typename Callback, typename... Args>
class CallerAuto<Callback(Args...)> {
public:
	std::shared_future<Callback> operator() (const std::function<Callback(Args...)> &callback, Args... args) {
		return std::async(std::launch::async | std::launch::deferred, callback, args...);
	}
};
template<typename... Args>
class CallerAuto<void(Args...)> {
public:
	std::shared_future<int> operator() (const std::function<void(Args...)> &callback, Args... args) {
		auto closure = [=](Args... args)->int { callback(args...); return 0; };
		return std::async(std::launch::async | std::launch::deferred, closure, args...);
	}
};

//tasks occur in a thread
template<typename Callback, typename... Args>
class CallerThread<Callback(Args...)> {
public:
	CallerThread() : threadPool(1) {
	}

	std::shared_future<Callback> operator() (const std::function<Callback(Args...)> &callback, Args... args) {
		return threadPool.submit(callback, args...);
	}

private:
	Tests::ThreadPool threadPool;
};
template<typename... Args>
class CallerThread<void(Args...)> {
public:
	CallerThread() : threadPool(1) {
	}

	std::shared_future<int> operator() (const std::function<void(Args...)> &callback, Args... args) {
		std::function<int(Args...)> closure = [=](Args... args)->int { callback(args...); return 0; };
		return threadPool.submit(closure, args...);
	}

private:
	Tests::ThreadPool threadPool;
};

//tasks occur in the pool
template<typename Callback, typename... Args>
class CallerThreadPool<Callback(Args...)> {
public:
	std::shared_future<Callback> operator() (const std::function<Callback(Args...)> &callback, Args... args) {
		return threadPool.submit(callback, args...);
	}

private:
	Tests::ThreadPool threadPool;
};
template< typename... Args>
class CallerThreadPool<void(Args...)> {
public:
	std::shared_future<int> operator() (const std::function<void(Args...)> &callback, Args... args) {
		std::function<int(Args...)> closure = [=](Args... args)->int { callback(args...); return 0; };
		return threadPool.submit(closure, args...);
	}

private:
	Tests::ThreadPool threadPool;
};
