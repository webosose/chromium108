// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <iostream>
#include <sstream>
#include <string>

#include "base/command_line.h"
#include "base/files/file.h"
#include "base/files/file_util.h"
#include "base/files/memory_mapped_file.h"
#include "base/hash/md5.h"
#include "base/logging.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/stringprintf.h"
// AV1 stateless decoding not supported upstream yet
#if BUILDFLAG(IS_CHROMEOS)
#include "media/gpu/v4l2/test/av1_decoder.h"
#endif
#include "media/gpu/v4l2/test/h264_decoder.h"
#include "media/gpu/v4l2/test/video_decoder.h"
#include "media/gpu/v4l2/test/vp9_decoder.h"

// AV1 stateless decoding not supported upstream yet
#if BUILDFLAG(IS_CHROMEOS)
using media::v4l2_test::Av1Decoder;
#endif
using media::v4l2_test::H264Decoder;
using media::v4l2_test::VideoDecoder;
using media::v4l2_test::Vp9Decoder;

namespace {

constexpr char kUsageMsg[] =
    "usage: v4l2_stateless_decoder\n"
    "           --video=<video path>\n"
    "           [--frames=<number of frames to decode>]\n"
    "           [--v=<log verbosity>]\n"
    "           [--output_path_prefix=<output files path prefix>]\n"
    "           [--md5=<md_log_path>]\n"
    "           [--visible]\n"
    "           [--help]\n";

constexpr char kHelpMsg[] =
    "This binary decodes the IVF video in <video> path with specified \n"
    "video <profile> via thinly wrapped v4l2 calls.\n"
    "Supported codecs: VP9 (profile 0), and AV1 (profile 0)\n"
    "\nThe following arguments are supported:\n"
    "    --video=<path>\n"
    "        Required. Path to IVF-formatted video to decode.\n"
    "    --frames=<int>\n"
    "        Optional. Number of frames to decode, defaults to all.\n"
    "        Override with a positive integer to decode at most that many.\n"
    "    --output_path_prefix=<path>\n"
    "        Optional. Prefix to the filepaths where raw YUV frames will be\n"
    "        written. For example, setting <path> to \"test/test_\" would \n"
    "        result in output files of the form \"test/test_000000.yuv\",\n"
    "       \"test/test_000001.yuv\", etc.\n"
    "    --md5=<path>\n"
    "        Optional. If specified, computes md5 checksum. If specified\n"
    "        with argument, prints the md5 checksum of each decoded (and\n"
    "        visible, if --visible is specified) frame in I420 format\n"
    "        to specified file. Verbose level 2 will display md5 checksums.\n"
    "    --visible\n"
    "        Optional. If specified, computes md5 hash values only for\n"
    "        visible frames.\n"
    "    --help\n"
    "        Display this help message and exit.\n";

}  // namespace

// Computes the md5 of given I420 data |yuv_plane| and prints the md5 to stdout.
// This functionality is needed for tast tests.
void ComputeAndPrintMD5hash(const std::vector<char>& yuv_plane, const base::FilePath md5_log_location) {
  base::MD5Digest md5_digest;
  base::MD5Sum(yuv_plane.data(), yuv_plane.size(), &md5_digest);
  std::string md5_digest_b16 = MD5DigestToBase16(md5_digest);

  if (!md5_log_location.empty()) {
    if (!PathExists(md5_log_location))
      WriteFile(md5_log_location, md5_digest_b16 + "\n");
    else
      AppendToFile(md5_log_location, md5_digest_b16 + "\n");
  }
  VLOG(2) << md5_digest_b16;
}

// Creates the appropriate decoder for |stream|, which points to IVF data.
// Returns nullptr on failure.
std::unique_ptr<VideoDecoder> CreateVideoDecoder(
    const base::MemoryMappedFile& stream) {
  CHECK(stream.IsValid());

  std::unique_ptr<VideoDecoder> decoder;

// AV1 stateless decoding not supported upstream yet
#if BUILDFLAG(IS_CHROMEOS)
  decoder = Av1Decoder::Create(stream);
#endif

  if (!decoder)
    decoder = Vp9Decoder::Create(stream);

  if (!decoder)
    decoder = H264Decoder::Create(stream);

  return decoder;
}

int main(int argc, char** argv) {
  base::CommandLine::Init(argc, argv);

  logging::LoggingSettings settings;
  settings.logging_dest =
      logging::LOG_TO_SYSTEM_DEBUG_LOG | logging::LOG_TO_STDERR;
  logging::InitLogging(settings);

  const base::CommandLine* cmd = base::CommandLine::ForCurrentProcess();

  if (cmd->HasSwitch("help")) {
    std::cout << kUsageMsg << "\n" << kHelpMsg;
    return EXIT_SUCCESS;
  }

  const bool has_output_file = cmd->HasSwitch("output_path_prefix");
  const std::string output_file_prefix =
      cmd->GetSwitchValueASCII("output_path_prefix");

  const base::FilePath video_path = cmd->GetSwitchValuePath("video");
  if (video_path.empty()) {
    LOG(ERROR) << "No input video path provided to decode.\n" << kUsageMsg;
    return EXIT_FAILURE;
  }

  const std::string frames = cmd->GetSwitchValueASCII("frames");
  int n_frames;
  if (frames.empty()) {
    n_frames = 0;
  } else if (!base::StringToInt(frames, &n_frames) || n_frames <= 0) {
    LOG(ERROR) << "Number of frames to decode must be positive integer, got "
               << frames;
    return EXIT_FAILURE;
  }

  const base::FilePath md5_log_location = cmd->GetSwitchValuePath("md5");

  // Deletes md5 log file if it already exists.
  if (PathExists(md5_log_location)) {
    DeleteFile(md5_log_location);
  }

  // Set up video stream.
  base::MemoryMappedFile stream;
  if (!stream.Initialize(video_path)) {
    LOG(ERROR) << "Couldn't open file: " << video_path;
    return EXIT_FAILURE;
  }

  const std::unique_ptr<VideoDecoder> dec = CreateVideoDecoder(stream);
  if (!dec) {
    LOG(ERROR) << "Failed to create decoder for file: " << video_path;
    return EXIT_FAILURE;
  }

  dec->Initialize();

  for (int i = 0; i < n_frames || n_frames == 0; i++) {
    LOG(INFO) << "Frame " << i << "...";

    std::vector<char> y_plane;
    std::vector<char> u_plane;
    std::vector<char> v_plane;
    gfx::Size size;
    const VideoDecoder::Result res =
        dec->DecodeNextFrame(y_plane, u_plane, v_plane, size, i);
    if (res == VideoDecoder::kEOStream) {
      LOG(INFO) << "End of stream.";
      break;
    } else if (res == VideoDecoder::kError) {
      LOG(ERROR) << "Unable to decode next frame.";
      return EXIT_FAILURE;
    }

    if (cmd->HasSwitch("visible") && !dec->LastDecodedFrameVisible())
      continue;

    std::vector<char> yuv_plane(y_plane);
    yuv_plane.insert(yuv_plane.end(), u_plane.begin(), u_plane.end());
    yuv_plane.insert(yuv_plane.end(), v_plane.begin(), v_plane.end());

    if (cmd->HasSwitch("md5"))
      ComputeAndPrintMD5hash(yuv_plane, md5_log_location);

    if (!has_output_file)
      continue;

    base::FilePath filename(
        base::StringPrintf("%s%.6d.yuv", output_file_prefix.c_str(), i));
    base::File output_file(
        filename, base::File::FLAG_CREATE_ALWAYS | base::File::FLAG_WRITE);
    output_file.WriteAtCurrentPos(y_plane.data(), size.GetArea());
    output_file.WriteAtCurrentPos(u_plane.data(), size.GetArea() / 4);
    output_file.WriteAtCurrentPos(v_plane.data(), size.GetArea() / 4);
  }

  return EXIT_SUCCESS;
}
