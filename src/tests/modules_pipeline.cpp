#include "tests.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/out/null.hpp"
#include "lib_modules/utils/pipeline.hpp"


using namespace Tests;
using namespace Modules;
using namespace Pipelines;

namespace {

unittest("Pipeline: empty") {
	{
		Pipeline p;
	}
	{
		Pipeline p;
		p.start();
	}
	{
		Pipeline p;
		p.waitForCompletion();
	}
	{
		Pipeline p;
		p.start();
		p.waitForCompletion();
	}
}

unittest("Pipeline: interrupted") {
	Pipeline p;
	auto demux = p.addModule(new Demux::LibavDemux("data/beepbop.mp4"));
	ASSERT(demux->getNumOutputs() > 1);
	auto null = p.addModule(new Out::Null);
	p.connect(demux, 0, null, 0);
	p.start();
	auto f = [&]() {
		p.exitSync();
	};
	std::thread tf(f);
	p.waitForCompletion();
	tf.join();
}

unittest("Pipeline: connect while running") {
	Pipeline p;
	auto demux = p.addModule(new Demux::LibavDemux("data/beepbop.mp4"));
	ASSERT(demux->getNumOutputs() > 1);
	auto null1 = p.addModule(new Out::Null);
	auto null2 = p.addModule(new Out::Null);
	p.connect(demux, 0, null1, 0);
	p.start();
	auto f = [&]() {
		p.connect(demux, 0, null2, 0);
	};
	std::thread tf(f);
	p.waitForCompletion();
	tf.join();
}

unittest("Pipeline: connect one input (out of 2) to one output") {
	Pipeline p;
	auto demux = p.addModule(new Demux::LibavDemux("data/beepbop.mp4"));
	ASSERT(demux->getNumOutputs() > 1);
	auto null = p.addModule(new Out::Null);
	p.connect(demux, 0, null, 0);
	p.start();
	p.waitForCompletion();
}

unittest("Pipeline: connect inputs to outputs") {
	bool thrown = false;
	try {
		Pipeline p;
		auto demux = p.addModule(new Demux::LibavDemux("data/beepbop.mp4"));
		auto null = p.addModule(new Out::Null());
		for (int i = 0; i < (int)demux->getNumOutputs(); ++i) {
			p.connect(null, i, demux, i);
		}
		p.start();
		p.waitForCompletion();
	}
	catch (std::runtime_error const& /*e*/) {
		thrown = true;
	}
	ASSERT(thrown);
}

#ifdef ENABLE_FAILING_TESTS
unittest("Pipeline: connect incompatible i/o") {
	bool thrown = false;
	try {
		thrown = true; //TODO
	} catch (std::runtime_error const& /*e*/) {
		thrown = true;
	}
	ASSERT(thrown);
}
#endif

}