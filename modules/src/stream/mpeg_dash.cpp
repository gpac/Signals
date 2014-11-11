#include "mpeg_dash.hpp"
#include "../utils/log.hpp"
#include "../utils/tools.hpp"
#include "../out/file.hpp"
#include "../common/libav.hpp" //FIXME: for DataAVPacket
#include <fstream>

#include "ffpp.hpp" //FIXME: remove once not based on libav anymore

#define DASH_DUR_IN_MS 10000

namespace {
	class MPD {
	public:
		//FIXME: hardcoded
		static void serialize(std::ostream &os) {
			static_assert(DASH_DUR_IN_MS == 10000, "FIXME: Segment duration is fixed to 10s");
			writeLine(os, "<?xml version=\"1.0\"?>");
			writeLine(os, "<!--MPD file Generated with GPAC version 0.5.1-DEV-rev5478 on 2014-11-06T11:32:18Z-->");
			writeLine(os, "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" minBufferTime=\"PT1.500000S\" type=\"static\" mediaPresentationDuration=\"PT0H0M47.68S\" profiles=\"urn:mpeg:dash:profile:full:2011\">");
			writeLine(os, " <ProgramInformation moreInformationURL=\"http://gpac.sourceforge.net\">");
			writeLine(os, "  <Title>BatmanHD_1000kbit_mpeg_dash.mpd generated by GPAC</Title>");
			writeLine(os, " </ProgramInformation>");
			writeLine(os, "");
			writeLine(os, " <Period duration=\"PT0H0M47.68S\">");
			writeLine(os, "  <AdaptationSet segmentAlignment=\"true\">");
			writeLine(os, "   <SegmentTemplate timescale=\"90000\" media=\"$RepresentationID$.mp4_$Number$\" startNumber=\"0\" duration=\"856764\" initialization=\"$RepresentationID$.mp4\"/>");
			writeLine(os, "   <Representation id=\"0\" mimeType=\"audio/mp4\" codecs=\"mp4a.40.2\" audioSamplingRate=\"44100\" startWithSAP=\"1\" bandwidth=\"59557\">");
			writeLine(os, "    <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"2\"/>");
			writeLine(os, "   </Representation>");
			writeLine(os, "  </AdaptationSet>");
			writeLine(os, "  <AdaptationSet segmentAlignment=\"true\" maxWidth=\"1280\" maxHeight=\"720\" maxFrameRate=\"24\" par=\"16:9\">");
			writeLine(os, "   <SegmentTemplate timescale=\"90000\" media=\"$RepresentationID$.mp4_$Number$\" startNumber=\"0\" duration=\"857250\" initialization=\"$RepresentationID$.mp4\"/>");
			writeLine(os, "   <Representation id=\"1\" mimeType=\"video/mp4\" codecs=\"avc1.64001f\" width=\"1280\" height=\"720\" frameRate=\"24\" sar=\"1:1\" startWithSAP=\"0\" bandwidth=\"1230111\">");
			writeLine(os, "   </Representation>");
			writeLine(os, "  </AdaptationSet>");
			writeLine(os, " </Period>");
			writeLine(os, "</MPD>");
		}

	private:
		static void writeLine(std::ostream &os, std::string s) {
			os << s << std::endl;
		}
	};
}

namespace Modules {
namespace Stream {

MPEG_DASH::MPEG_DASH(Type type)
: type(type), workingThread(&MPEG_DASH::DASHThread, this) {
}

MPEG_DASH::~MPEG_DASH() {
	audioDataQueue.push(nullptr);
	videoDataQueue.push(nullptr);
	workingThread.join();
}

//FIXME: we would post/defer/schedule the whole module... but here we are already in our own thread
void MPEG_DASH::DASHThread() {
	uint64_t n = 0;
	auto startTime = std::chrono::steady_clock::now();
	for (;;) {
		auto a = audioDataQueue.pop();
		auto v = videoDataQueue.pop();
		if (!a || !v)
			break;

		GenerateMPD(n, a, v);

		if (type == Live) {
			auto now = std::chrono::steady_clock::now();
			auto next = startTime + std::chrono::milliseconds(DASH_DUR_IN_MS * n);
			if (next > now) {
				auto dur = next - now;
				std::this_thread::sleep_for(dur);
			} else {
				Log::msg(Log::Warning, "[MPEG_DASH] Next MPD update (%s) is in the past. Are we running too slow?", n);
			}
		}

		n++;
	}
}

void MPEG_DASH::GenerateMPD(uint64_t segNum, std::shared_ptr<Data> /*audio*/, std::shared_ptr<Data> /*video*/) {
#if 0
	//Print the segments to the appropriate threads
	std::stringstream ssa, ssv;

	const std::string audioFn = "audio.m4s";
	const std::string videoFn = "video.m4s";

	//serialize the MPD (use the GPAC code?)
	auto audioSeg = uptr(Out::File::create(audioFn));
	auto videoSeg = uptr(Out::File::create(videoFn));
#else
	std::ofstream f;
	f.open("dash.mpd", std::ios::out);
	MPD::serialize(f);
	f.close();
#endif
}

void MPEG_DASH::process(std::shared_ptr<Data> data) {
#if 0
	/* TODO:
	 * 1) no test on timestamps
	 * 2) no time to provoke the MPD generation on time
	 */
	auto inputData = safe_cast<DataAVPacket>(data);
	AVPacket *pkt = inputData->getPacket();
	switch (pkt->stream_index) {
	//FIXME: arbitrary
	case 0:
		audioDataQueue.push(data);
		break;
	case 1:
		videoDataQueue.push(data);
		break;
	default:
		Log::msg(Log::Warning, "[MPEG_DASH] undeclared data. Discarding.");
		return;
	}
#else
	assert(0);
#endif
}

void MPEG_DASH::processAudio(std::shared_ptr<Data> data) {
	audioDataQueue.push(data);
}

void MPEG_DASH::processVideo(std::shared_ptr<Data> data) {
	videoDataQueue.push(data);
}

}
}
