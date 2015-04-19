#include "mpeg_dash.hpp"
#include "lib_modules/core/clock.hpp"
#include "lib_utils/log.hpp"
#include "lib_utils/tools.hpp"
#include "../out/file.hpp"
#include "../common/libav.hpp"
#include <fstream>

extern "C" {
#include <gpac/isomedia.h>
}


namespace {
std::string getUTC() {
	auto utc = gf_net_get_utc();
	time_t t = utc / 1000;
	auto msecs = (u32)(utc - t * 1000);
	auto AST = *gmtime(&t);
	const size_t maxLen = 25;
	char sAST[maxLen];
	snprintf(sAST, maxLen, "%.4d-%02d-%02dT%02d:%02d:%02d.%03dZ", 1900 + AST.tm_year, AST.tm_mon + 1, AST.tm_mday, AST.tm_hour, AST.tm_min, AST.tm_sec, msecs);
	return sAST;
}
}


namespace Modules {
namespace Stream {

struct MPD {
	MPD(bool isLive, uint64_t segDurationInMs)
	: isLive(isLive), segDurationInMs(segDurationInMs) {
		if (segDurationInMs == 0)
			throw std::runtime_error("[MPEG-DASH] Segment duration too small. Please check your settings.");
	}

	void serialize(std::ostream &os) {
		writeLine(os, "<?xml version=\"1.0\"?>");
		writeLine(os, "<!--MPD file Generated with Signals - Copyright (C) gpac-licensing.com 2014 -->");
		auto const mediaPresentationDuration = "PT0H0M47.68S"; //TODO
		if (isLive) {
			writeLine(os, "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" minBufferTime=\"PT1.500000S\" type=\"dynamic\" mediaPresentationDuration=\"%s\" profiles=\"urn:mpeg:dash:profile:full:2011\" minimumUpdatePeriod=\"PT%sS\">", mediaPresentationDuration, convertToTimescale(segDurationInMs, 1000, 1));
		} else {
			auto availabilityStartTime = getUTC();
			writeLine(os, "<MPD xmlns=\"urn:mpeg:dash:schema:mpd:2011\" minBufferTime=\"PT1.500000S\" type=\"static\" availabilityStartTime=\"%s\" profiles=\"urn:mpeg:dash:profile:full:2011\">", availabilityStartTime);
		}
		writeLine(os, " <ProgramInformation moreInformationURL=\"http://gpac.io\">");
		writeLine(os, "  <Title>BatmanHD_1000kbit_mpeg_dash.mpd generated by GPAC</Title>");
		writeLine(os, " </ProgramInformation>");
		writeLine(os, "");
		auto const periodDuration = mediaPresentationDuration;
		writeLine(os, " <Period duration=\"%s\">", periodDuration);

		//Audio AS
		writeLine(os, "  <AdaptationSet segmentAlignment=\"true\">");
		auto audioTimescale = 90000;
		writeLine(os, "   <SegmentTemplate timescale=\"%s\" media=\"$RepresentationID$.mp4_$Number$\" startNumber=\"0\" duration=\"%s\" initialization=\"$RepresentationID$.mp4\"/>", audioTimescale, convertToTimescale(segDurationInMs, 1000, audioTimescale));
		/*ret = gf_media_get_rfc_6381_codec_name(video_output_file->isof, track, video_output_file->video_data_conf->codec6381, GF_FALSE);
		if (ret != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_DASH, ("%s: gf_isom_finalize_for_fragment\n", gf_error_to_string(ret)));
		return -1;
		}*/
		auto const audioCodec = "mp4a.40.2";
		auto const audioRate = 44100;
		auto const audioBandwidth = 59557;
		writeLine(os, "   <Representation id=\"0\" mimeType=\"audio/mp4\" codecs=\"%s\" audioSamplingRate=\"%s\" startWithSAP=\"1\" bandwidth=\"%s\">", audioCodec, audioRate, audioBandwidth);
		writeLine(os, "    <AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"2\"/>");
		writeLine(os, "   </Representation>");
		writeLine(os, "  </AdaptationSet>");

		//Video AS
		writeLine(os, "  <AdaptationSet segmentAlignment=\"true\" maxWidth=\"1280\" maxHeight=\"720\" maxFrameRate=\"24\" par=\"16:9\">");
		auto videoTimescale = audioTimescale;
		writeLine(os, "   <SegmentTemplate timescale=\"%s\" media=\"$RepresentationID$.mp4_$Number$\" startNumber=\"0\" duration=\"%s\" initialization=\"$RepresentationID$.mp4\"/>", videoTimescale, convertToTimescale(segDurationInMs, 1000, videoTimescale));
		auto const videoCodec = "avc1.64001f";
		auto const videoWidth = 1280;
		auto const videoHeight = 720;
		auto const videoRate = 24;
		auto const videoSAR = "1:1";
		auto const videoBandwidth = 1230111;
		writeLine(os, "   <Representation id=\"1\" mimeType=\"video/mp4\" codecs=\"%s\" width=\"%s\" height=\"%s\" frameRate=\"%s\" sar=\"%s\" startWithSAP=\"0\" bandwidth=\"%s\">", videoCodec, videoWidth, videoHeight, videoRate, videoSAR, videoBandwidth);
		writeLine(os, "   </Representation>");
		writeLine(os, "  </AdaptationSet>");

		writeLine(os, " </Period>");
		writeLine(os, "</MPD>");
	}

	template<typename... Arguments>
	static void writeLine(std::ostream &os, const std::string& fmt, Arguments... args) {
		os << format(fmt, args...) << std::endl;
		os.flush();
	}

	bool isLive;
	uint64_t segDurationInMs;
};

MPEG_DASH::MPEG_DASH(Type type, uint64_t segDurationInMs)
: workingThread(&MPEG_DASH::DASHThread, this), type(type), mpd(new MPD(type == Live, segDurationInMs)) {
	auto input = addInput(new Input<DataAVPacket>(this));
}

void MPEG_DASH::endOfStream() {
	if (workingThread.joinable()) {
		audioDataQueue.push(nullptr);
		videoDataQueue.push(nullptr);
		workingThread.join();
	}
}

MPEG_DASH::~MPEG_DASH() {
	audioDataQueue.clear();
	videoDataQueue.clear();
	endOfStream();
}

//needed because of the use of system time for live - otherwise awake on data as for any multi-input module
//TODO: add clock to the scheduler
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
			auto next = startTime + std::chrono::milliseconds(mpd->segDurationInMs * n);
			if (next > now) {
				auto dur = next - now;
				Log::msg(Log::Info, "[MPEG_DASH] Going to sleep for %s ms.", std::chrono::duration_cast<std::chrono::milliseconds>(dur).count());
				std::this_thread::sleep_for(dur);
			} else {
				Log::msg(Log::Warning, "[MPEG_DASH] Next MPD update (%s) is in the past. Are we running too slow?", n);
			}
		}

		n++;
	}
}

void MPEG_DASH::GenerateMPD(uint64_t segNum, Data /*audio*/, Data /*video*/) {
#if 0
	//Print the segments to the appropriate threads
	std::stringstream ssa, ssv;

	const std::string audioFn = "audio.m4s";
	const std::string videoFn = "video.m4s";

	//serialize the MPD (use the GPAC code?)
	auto audioSeg = uptr(new Out::File(audioFn));
	auto videoSeg = uptr(new Out::File(videoFn));
#else
	std::ofstream f;
	f.open("dash.mpd", std::ios::out);
	mpd->serialize(f);
	f.close();
#endif
}

void MPEG_DASH::process(Data data) {
	/* TODO:
	 * 1) no test on timestamps
	 * 2) no time to provoke the MPD generation on time
	 */
	//FIXME: Romain: reimplement with multiple inputs
	switch (data->getMetadata()->getStreamType()) {
	case AUDIO_PKT:
		audioDataQueue.push(data);
		break;
	case VIDEO_PKT:
		videoDataQueue.push(data);
		break;
	default:
		assert(0);
		Log::msg(Log::Warning, "[MPEG_DASH] undeclared data. Discarding.");
		return;
	}
}

void MPEG_DASH::flush() {
	numDataQueueNotify--;
	if ((type == Live) && (numDataQueueNotify == 0))
		endOfStream();
}

}
}
