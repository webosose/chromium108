// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_WEBID_TEST_MOCK_IDP_NETWORK_REQUEST_MANAGER_H_
#define CONTENT_BROWSER_WEBID_TEST_MOCK_IDP_NETWORK_REQUEST_MANAGER_H_

#include "content/browser/webid/idp_network_request_manager.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace content {

class MockIdpNetworkRequestManager : public IdpNetworkRequestManager {
 public:
  MockIdpNetworkRequestManager();
  ~MockIdpNetworkRequestManager() override;

  MockIdpNetworkRequestManager(const MockIdpNetworkRequestManager&) = delete;
  MockIdpNetworkRequestManager& operator=(const MockIdpNetworkRequestManager&) =
      delete;

  MOCK_METHOD2(FetchManifestList, void(const GURL&, FetchManifestListCallback));
  MOCK_METHOD4(FetchManifest,
               void(const GURL&,
                    absl::optional<int>,
                    absl::optional<int>,
                    FetchManifestCallback));
  MOCK_METHOD3(FetchClientMetadata,
               void(const GURL&,
                    const std::string&,
                    FetchClientMetadataCallback));
  MOCK_METHOD3(SendAccountsRequest,
               void(const GURL&, const std::string&, AccountsRequestCallback));
  MOCK_METHOD4(SendTokenRequest,
               void(const GURL&,
                    const std::string&,
                    const std::string&,
                    TokenRequestCallback));
  MOCK_METHOD2(SendLogout, void(const GURL& logout_url, LogoutCallback));
};

}  // namespace content

#endif  // CONTENT_BROWSER_WEBID_TEST_MOCK_IDP_NETWORK_REQUEST_MANAGER_H_
