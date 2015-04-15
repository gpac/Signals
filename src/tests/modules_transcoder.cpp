#include "tests.hpp"
#include "lib_modules/modules.hpp"
#include "lib_media/decode/jpegturbo_decode.hpp"
#include "lib_media/decode/libav_decode.hpp"
#include "lib_media/demux/libav_demux.hpp"
#include "lib_media/encode/jpegturbo_encode.hpp"
#include "lib_media/encode/libav_encode.hpp"
#include "lib_media/in/file.hpp"
#include "lib_media/mux/libav_mux.hpp"
#include "lib_media/mux/gpac_mux_mp4.hpp"
#include "lib_media/out/file.hpp"
#include "lib_media/out/null.hpp"
#include "lib_media/transform/video_convert.hpp"
#include "lib_utils/tools.hpp"


using namespace Tests;
using namespace Modules;

namespace {

unittest("transcoder: video simple (libav mux)") {
	auto demux = uptr(new Demux::LibavDemux("data/BatmanHD_1000kbit_mpeg_0_20_frag_1000.mp4"));
	auto null = uptr(new Out::Null);

	//find video signal from demux
	size_t videoIndex = std::numeric_limits<size_t>::max();
	for (size_t i = 0; i < demux->getNumPin(); ++i) {
		auto props = demux->getPin(i)->getProps();
		auto decoderProps = safe_cast<PropsPkt>(props);
		if (decoderProps->getStreamType() == VIDEO_PKT) {
			videoIndex = i;
		} else {
			ConnectPinToModule(demux->getPin(i), null);
		}
	}
	ASSERT(videoIndex != std::numeric_limits<size_t>::max());

	//create the video decoder
	auto props = demux->getPin(videoIndex)->getProps();
	auto decoderProps = safe_cast<PropsDecoder>(props);

	auto decode = uptr(new Decode::LibavDecode(*decoderProps));
	auto encode = uptr(new Encode::LibavEncode(Encode::LibavEncode::Video));
	auto mux = uptr(new Mux::LibavMux("output_video_libav"));

	ConnectPinToModule(demux->getPin(videoIndex), decode);
	ConnectPinToModule(decode->getPin(0), encode);
	ConnectPinToModule(encode->getPin(0), mux);
	encode->sendOutputPinsInfo();

	demux->process(nullptr);
}

unittest("transcoder: video simple (gpac mux)") {
	auto demux = uptr(new Demux::LibavDemux("data/BatmanHD_1000kbit_mpeg_0_20_frag_1000.mp4"));

	//create stub output (for unused demuxer's outputs)
	auto null = uptr(new Out::Null);

	//find video signal from demux
	size_t videoIndex = std::numeric_limits<size_t>::max();
	for (size_t i = 0; i < demux->getNumPin(); ++i) {
		auto props = demux->getPin(i)->getProps();
		auto decoderProps = safe_cast<PropsPkt>(props);
		if (decoderProps->getStreamType() == VIDEO_PKT) {
			videoIndex = i;
		} else {
			ConnectPinToModule(demux->getPin(i), null);
		}
	}
	ASSERT(videoIndex != std::numeric_limits<size_t>::max());

	//create the video decoder
	auto props = demux->getPin(videoIndex)->getProps();
	auto decoderProps = safe_cast<PropsDecoder>(props);

	auto decode = uptr(new Decode::LibavDecode(*decoderProps));
	auto encode = uptr(new Encode::LibavEncode(Encode::LibavEncode::Video));
	auto mux = uptr(new Mux::GPACMuxMP4("output_video_gpac"));

	ConnectPinToModule(demux->getPin(videoIndex), decode);
	ConnectPinToModule(decode->getPin(0), encode);
	ConnectPinToModule(encode->getPin(0), mux);
	encode->sendOutputPinsInfo();

	demux->process(nullptr);
}

unittest("transcoder: jpg to jpg") {
	const std::string filename("data/sample.jpg");
	auto decoder = uptr(new Decode::JPEGTurboDecode());
	{
		auto preReader = uptr(new In::File(filename));
		ConnectPinToModule(preReader->getPin(0), decoder);
		preReader->process(nullptr);
	}
	auto props = decoder->getPin(0)->getProps();
	ASSERT(props != nullptr);
	auto decoderProps = safe_cast<PropsDecoderImage>(props);

	auto reader = uptr(new In::File(filename));
	auto dstRes = decoderProps->getResolution();
	auto encoder = uptr(new Encode::JPEGTurboEncode(dstRes));
	auto writer = uptr(new Out::File("data/test.jpg"));

	ConnectPinToModule(reader->getPin(0), decoder);
	ConnectPinToModule(decoder->getPin(0), encoder);
	ConnectPinToModule(encoder->getPin(0), writer);

	reader->process(nullptr);
}

unittest("transcoder: jpg to resized jpg") {
	const std::string filename("data/sample.jpg");
	auto decoder = uptr(new Decode::JPEGTurboDecode());
	{
		auto preReader = uptr(new In::File(filename));
		ConnectPinToModule(preReader->getPin(0), decoder);
		preReader->process(nullptr);
	}
	auto props = decoder->getPin(0)->getProps();
	ASSERT(props != nullptr);
	auto decoderProps = safe_cast<PropsDecoderImage>(props);

	auto reader = uptr(new In::File(filename));
	ASSERT(decoderProps->getPixelFormat() == RGB24);
	auto dstRes = decoderProps->getResolution() / 2;
	auto dstFormat = PictureFormat(dstRes, decoderProps->getPixelFormat());
	auto converter = uptr(new Transform::VideoConvert(dstFormat));
	auto encoder = uptr(new Encode::JPEGTurboEncode(dstRes));
	auto writer = uptr(new Out::File("data/test.jpg"));

	ConnectPinToModule(reader->getPin(0), decoder);
	ConnectPinToModule(decoder->getPin(0), converter);
	ConnectPinToModule(converter->getPin(0), encoder);
	ConnectPinToModule(encoder->getPin(0), writer);

	reader->process(nullptr);
}

unittest("transcoder: h264/mp4 to jpg") {
	auto demux = uptr(new Demux::LibavDemux("data/BatmanHD_1000kbit_mpeg_0_20_frag_1000.mp4"));

	auto props = demux->getPin(0)->getProps();
	auto decoderProps = safe_cast<PropsDecoderImage>(props);
	auto decoder = uptr(new Decode::LibavDecode(*decoderProps));

	auto dstRes = decoderProps->getResolution();
	auto encoder = uptr(new Encode::JPEGTurboEncode(dstRes));
	auto writer = uptr(new Out::File("data/test.jpg"));

	ASSERT(decoderProps->getPixelFormat() == YUV420P);
	auto dstFormat = PictureFormat(dstRes, RGB24);
	auto converter = uptr(new Transform::VideoConvert(dstFormat));

	ConnectPinToModule(demux->getPin(0), decoder);
	ConnectPinToModule(decoder->getPin(0), converter);
	ConnectPinToModule(converter->getPin(0), encoder);
	ConnectPinToModule(encoder->getPin(0), writer);

	demux->process(nullptr);
}

unittest("transcoder: jpg to h264/mp4 (gpac)") {
	const std::string filename("data/sample.jpg");
	auto decoder = uptr(new Decode::JPEGTurboDecode());
	{
		auto preReader = uptr(new In::File(filename));
		ConnectPinToModule(preReader->getPin(0), decoder);
		//FIXME: to retrieve the props, we now need to decode (need to have a memory module keeping the data while inspecting)
		preReader->process(nullptr);
	}
	auto props = decoder->getPin(0)->getProps();
	ASSERT(props != nullptr);
	auto decoderProps = safe_cast<PropsDecoderImage>(props);
	auto srcRes = decoderProps->getResolution();

	auto reader = uptr(new In::File(filename));
	ASSERT(decoderProps->getPixelFormat() == RGB24);
	auto dstFormat = PictureFormat(srcRes, decoderProps->getPixelFormat());
	auto converter = uptr(new Transform::VideoConvert(dstFormat));

	auto encoder = uptr(new Encode::LibavEncode(Encode::LibavEncode::Video));
	auto mux = uptr(new Mux::GPACMuxMP4("data/test"));

	ConnectPinToModule(reader->getPin(0), decoder);
	ConnectPinToModule(decoder->getPin(0), converter);
	ConnectPinToModule(converter->getPin(0), encoder);
	ConnectPinToModule(encoder->getPin(0), mux);
	encoder->sendOutputPinsInfo();

	reader->process(nullptr);
}

}
