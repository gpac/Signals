#pragma once

#include "internal/core/module.hpp"
#include "src/common/libav.hpp"

struct AVFormatContext;

using namespace Modules;

namespace Demux {

class LibavDemux : public Module {
public:
	static LibavDemux* create(const std::string &url);
	~LibavDemux();
	void process(std::shared_ptr<const Data> data) override;

private:
	LibavDemux(struct AVFormatContext *formatCtx);

	struct AVFormatContext *m_formatCtx;
	std::vector<PinDataDefault<DataAVPacket>*> outputs;
};

}
