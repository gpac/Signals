#pragma once

#include "data.hpp"
#include "pin.hpp"
#include <vector>
#include <string>


namespace Modules {

//TODO: separate sync and async?
class MODULES_EXPORT IModule {
public:
	virtual bool process(std::shared_ptr<Data> data) = 0;
	virtual bool handles(const std::string &url) = 0;
	virtual void destroy() = 0; /* required for async, otherwise we still have callback/futures on an object being destroyed */
};

//specialization
class MODULES_EXPORT Module : public IModule {
public:
	virtual ~Module() {
	}

	Module() = default;
	Module(Module const&) = delete;
	Module const& operator=(Module const&) = delete;

	virtual bool process(std::shared_ptr<Data> data) = 0;
	virtual bool handles(const std::string &url) = 0;

	/**
	 * Must be called before the destructor.
	 */
	virtual void destroy() {
		for (auto &signal : signals) {
			signal->destroy();
		}
	}

	std::vector<std::unique_ptr<Pin<>>> signals; //TODO: evaluate how public this needs to be
};

class MODULES_EXPORT ModuleSync : public IModule {
public:
	virtual bool process(std::shared_ptr<Data> data) = 0;
	virtual bool handles(const std::string &url) = 0;

	/**
	* Must be called before the destructor.
	*/
	virtual void destroy() {
		for (auto &signal : signals) {
			signal->destroy();
		}
	}

	std::vector<PinSync*> signals; //TODO: evaluate how public this needs to be
};

}
