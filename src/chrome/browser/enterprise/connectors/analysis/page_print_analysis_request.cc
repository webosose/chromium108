// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/enterprise/connectors/analysis/page_print_analysis_request.h"

#include "base/memory/read_only_shared_memory_region.h"
#include "chrome/browser/enterprise/connectors/common.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/binary_upload_service.h"
#include "chrome/browser/safe_browsing/cloud_content_scanning/deep_scanning_utils.h"

namespace enterprise_connectors {

namespace {

constexpr size_t kMaxPageSize = 50 * 1024 * 1024;

}  // namespace

PagePrintAnalysisRequest::PagePrintAnalysisRequest(
    const AnalysisSettings& analysis_settings,
    base::ReadOnlySharedMemoryRegion page,
    safe_browsing::BinaryUploadService::ContentAnalysisCallback callback)
    : safe_browsing::BinaryUploadService::Request(
          std::move(callback),
          analysis_settings.cloud_or_local_settings),
      page_(std::move(page)) {
  safe_browsing::IncrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_PRINTS);
  safe_browsing::IncrementCrashKey(
      safe_browsing::ScanningCrashKey::TOTAL_PRINTS);
}

PagePrintAnalysisRequest::~PagePrintAnalysisRequest() {
  safe_browsing::DecrementCrashKey(
      safe_browsing::ScanningCrashKey::PENDING_PRINTS);
}

void PagePrintAnalysisRequest::GetRequestData(DataCallback callback) {
  safe_browsing::BinaryUploadService::Request::Data data;
  data.size = page_.GetSize();

  if (data.size >= kMaxPageSize) {
    std::move(callback).Run(
        safe_browsing::BinaryUploadService::Result::FILE_TOO_LARGE,
        std::move(data));
    return;
  }

  data.page = std::move(page_);
  std::move(callback).Run(safe_browsing::BinaryUploadService::Result::SUCCESS,
                          std::move(data));
}

}  // namespace enterprise_connectors
