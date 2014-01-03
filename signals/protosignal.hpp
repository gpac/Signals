#pragma once

#include "caller.hpp"
#include "connection.hpp"

#include <atomic>
#include <cassert>
#include <functional>
#include <unordered_map>
#include <vector>


template<typename, typename> class ProtoSignal;

template<typename Result, typename Callback, typename... Args>
class ProtoSignal<Result, Callback(Args...)> : private CallerAsync<Result, Callback(Args...)> {
protected:
	typedef std::function<Callback(Args...)> Callback;

private:
	typedef typename Result::ResultValue ResultValue;
	typedef typename Callback::result_type ResultType;
	typedef typename Connection<Callback> ConnectionType;
	typedef typename std::unordered_map<size_t, ConnectionType*> ConnectionManager;

public:
	size_t connect(const Callback &cb) {
		size_t connectionId = uid++;
		callbacks[connectionId] = new ConnectionType(cb, connectionId);
		return connectionId;
	}

	bool disconnect(size_t connectionId) {
		if (callbacks[connectionId] != nullptr) {
			delete callbacks[connectionId];
			callbacks[connectionId] = nullptr;
			return true;
		} else {
			return false;
		}
	}

	size_t emit(Args... args) {
		result.clear();
		futures.clear();
		for each (auto cb in callbacks) {
			futures.push_back(call(result, cb.second->callback, args...));
		}
		return callbacks.size();
	}

	ResultValue results() {
		for (size_t i = 0; i < futures.size(); i++) {
			std::future<ResultType> f = std::move(futures[i]);
			result.set(f.get());
		}
		return result.get();
	}

protected:
	ProtoSignal(const Callback &cb) : uid(0) {
		if (cb != NULL) {
			size_t connectionId = uid++;
			callbacks[connectionId] = new ConnectionType(cb, connectionId);
		}
	}

	~ProtoSignal() {
		//delete still connected callbacks
		for each(auto cb in callbacks) {
			auto id = cb.first;
			if (callbacks[id] != nullptr) {
				bool res = disconnect(id);
				assert(res);
			}
		}
	}

private:
	ProtoSignal(const ProtoSignal&) = delete;
	ProtoSignal& operator= (const ProtoSignal&) = delete;

	ConnectionManager callbacks; //TODO: make an interface for the ConnectionManager to remove code from this class
	std::vector<std::future<ResultType>> futures;
	std::atomic<size_t> uid;
	Result result;
};
