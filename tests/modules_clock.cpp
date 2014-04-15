#include "tests.hpp"
#include "modules.hpp"
#include "../utils/tools.hpp"
#include <memory>

using namespace Tests;
using namespace Modules;

namespace {

unittest("Basic clock") {
	for (int i = 0; i < 10; ++i) {
		auto const now = g_DefaultClock->now();
		std::cout << "Time: " << now << std::endl;

		auto const duration = std::chrono::milliseconds(20);
		std::this_thread::sleep_for(duration);
	}
}

}
