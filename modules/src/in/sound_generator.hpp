#pragma once

#include "../../internal/config.hpp"
#include "internal/module.hpp"
#include "internal/param.hpp"
#include <string>

namespace Modules {

namespace In {

class MODULES_EXPORT SoundGenerator : public Module {
public:
	SoundGenerator();
	bool handles(const std::string &url);
	static bool canHandle(const std::string &url);
	bool process(std::shared_ptr<Data> data);

private:
	uint64_t m_numSamples;
};

}

}