// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chromeos/startup/browser_params_proxy.h"

#include "chromeos/startup/browser_init_params.h"

namespace chromeos {

// static
BrowserParamsProxy* BrowserParamsProxy::Get() {
  static base::NoDestructor<BrowserParamsProxy> browser_params_proxy;
  return browser_params_proxy.get();
}

bool BrowserParamsProxy::DisableCrosapiForTesting() const {
  return BrowserInitParams::disable_crosapi_for_testing();
}

uint32_t BrowserParamsProxy::CrosapiVersion() const {
  return BrowserInitParams::Get()->crosapi_version;
}

bool BrowserParamsProxy::DeprecatedAshMetricsEnabledHasValue() const {
  return BrowserInitParams::Get()->deprecated_ash_metrics_enabled_has_value;
}

bool BrowserParamsProxy::AshMetricsEnabled() const {
  return BrowserInitParams::Get()->ash_metrics_enabled;
}

crosapi::mojom::SessionType BrowserParamsProxy::SessionType() const {
  return BrowserInitParams::Get()->session_type;
}

crosapi::mojom::DeviceMode BrowserParamsProxy::DeviceMode() const {
  return BrowserInitParams::Get()->device_mode;
}

const absl::optional<base::flat_map<base::Token, uint32_t>>&
BrowserParamsProxy::InterfaceVersions() const {
  return BrowserInitParams::Get()->interface_versions;
}

const crosapi::mojom::DefaultPathsPtr& BrowserParamsProxy::DefaultPaths()
    const {
  return BrowserInitParams::Get()->default_paths;
}

const absl::optional<std::string>& BrowserParamsProxy::DeviceAccountGaiaId()
    const {
  return BrowserInitParams::Get()->device_account_gaia_id;
}

crosapi::mojom::MetricsReportingManaged BrowserParamsProxy::AshMetricsManaged()
    const {
  return BrowserInitParams::Get()->ash_metrics_managed;
}

crosapi::mojom::ExoImeSupport BrowserParamsProxy::ExoImeSupport() const {
  return BrowserInitParams::Get()->exo_ime_support;
}

const absl::optional<std::string>& BrowserParamsProxy::CrosUserIdHash() const {
  return BrowserInitParams::Get()->cros_user_id_hash;
}

const absl::optional<std::vector<uint8_t>>&
BrowserParamsProxy::DeviceAccountPolicy() const {
  return BrowserInitParams::Get()->device_account_policy;
}

uint64_t BrowserParamsProxy::LastPolicyFetchAttemptTimestamp() const {
  return BrowserInitParams::Get()->last_policy_fetch_attempt_timestamp;
}

const crosapi::mojom::IdleInfoPtr& BrowserParamsProxy::IdleInfo() const {
  return BrowserInitParams::Get()->idle_info;
}

crosapi::mojom::InitialBrowserAction BrowserParamsProxy::InitialBrowserAction()
    const {
  return BrowserInitParams::Get()->initial_browser_action;
}

const crosapi::mojom::AccountPtr& BrowserParamsProxy::DeviceAccount() const {
  return BrowserInitParams::Get()->device_account;
}

bool BrowserParamsProxy::WebAppsEnabled() const {
  return BrowserInitParams::Get()->web_apps_enabled;
}

bool BrowserParamsProxy::StandaloneBrowserIsPrimary() const {
  return BrowserInitParams::Get()->standalone_browser_is_primary;
}

const crosapi::mojom::NativeThemeInfoPtr& BrowserParamsProxy::NativeThemeInfo()
    const {
  return BrowserInitParams::Get()->native_theme_info;
}

const crosapi::mojom::DevicePropertiesPtr&
BrowserParamsProxy::DeviceProperties() const {
  return BrowserInitParams::Get()->device_properties;
}

crosapi::mojom::OndeviceHandwritingSupport
BrowserParamsProxy::OndeviceHandwritingSupport() const {
  return BrowserInitParams::Get()->ondevice_handwriting_support;
}

const absl::optional<std::vector<crosapi::mojom::BuildFlag>>&
BrowserParamsProxy::BuildFlags() const {
  return BrowserInitParams::Get()->build_flags;
}

crosapi::mojom::OpenUrlFrom BrowserParamsProxy::StartupUrlsFrom() const {
  return BrowserInitParams::Get()->startup_urls_from;
}

const absl::optional<std::vector<GURL>>& BrowserParamsProxy::StartupUrls()
    const {
  return BrowserInitParams::Get()->startup_urls;
}

const crosapi::mojom::DeviceSettingsPtr& BrowserParamsProxy::DeviceSettings()
    const {
  return BrowserInitParams::Get()->device_settings;
}

const absl::optional<std::string>& BrowserParamsProxy::MetricsServiceClientId()
    const {
  return BrowserInitParams::Get()->metrics_service_client_id;
}

uint64_t BrowserParamsProxy::UkmClientId() const {
  return BrowserInitParams::Get()->ukm_client_id;
}

bool BrowserParamsProxy::StandaloneBrowserIsOnlyBrowser() const {
  return BrowserInitParams::Get()->standalone_browser_is_only_browser;
}

bool BrowserParamsProxy::PublishChromeApps() const {
  return BrowserInitParams::Get()->publish_chrome_apps;
}

bool BrowserParamsProxy::PublishHostedApps() const {
  return BrowserInitParams::Get()->publish_hosted_apps;
}

crosapi::mojom::BrowserInitParams::InitialKeepAlive
BrowserParamsProxy::InitialKeepAlive() const {
  return BrowserInitParams::Get()->initial_keep_alive;
}

bool BrowserParamsProxy::IsUnfilteredBluetoothDeviceEnabled() const {
  return BrowserInitParams::Get()->is_unfiltered_bluetooth_device_enabled;
}

const absl::optional<std::vector<std::string>>&
BrowserParamsProxy::AshCapabilities() const {
  return BrowserInitParams::Get()->ash_capabilities;
}

const absl::optional<std::vector<GURL>>&
BrowserParamsProxy::AcceptedInternalAshUrls() const {
  return BrowserInitParams::Get()->accepted_internal_ash_urls;
}

bool BrowserParamsProxy::IsHoldingSpaceIncognitoProfileIntegrationEnabled()
    const {
  return BrowserInitParams::Get()
      ->is_holding_space_incognito_profile_integration_enabled;
}

bool BrowserParamsProxy::
    IsHoldingSpaceInProgressDownloadsNotificationSuppressionEnabled() const {
  return BrowserInitParams::Get()
      ->is_holding_space_in_progress_downloads_notification_suppression_enabled;
}

bool BrowserParamsProxy::IsDeviceEnterprisedManaged() const {
  return BrowserInitParams::Get()->is_device_enterprised_managed;
}

crosapi::mojom::BrowserInitParams::DeviceType BrowserParamsProxy::DeviceType()
    const {
  return BrowserInitParams::Get()->device_type;
}

bool BrowserParamsProxy::IsOndeviceSpeechSupported() const {
  return BrowserInitParams::Get()->is_ondevice_speech_supported;
}

const absl::optional<base::flat_map<policy::PolicyNamespace, base::Value>>&
BrowserParamsProxy::DeviceAccountComponentPolicy() const {
  return BrowserInitParams::Get()->device_account_component_policy;
}

const absl::optional<std::string>& BrowserParamsProxy::AshChromeVersion()
    const {
  return BrowserInitParams::Get()->ash_chrome_version;
}

bool BrowserParamsProxy::UseCupsForPrinting() const {
  return BrowserInitParams::Get()->use_cups_for_printing;
}

bool BrowserParamsProxy::UseFlossBluetooth() const {
  return BrowserInitParams::Get()->use_floss_bluetooth;
}

bool BrowserParamsProxy::IsCurrentUserDeviceOwner() const {
  return BrowserInitParams::Get()->is_current_user_device_owner;
}

bool BrowserParamsProxy::DoNotMuxExtensionAppIds() const {
  return BrowserInitParams::Get()->do_not_mux_extension_app_ids;
}

bool BrowserParamsProxy::EnableLacrosTtsSupport() const {
  return BrowserInitParams::Get()->enable_lacros_tts_support;
}

crosapi::mojom::BrowserInitParams::LacrosSelection
BrowserParamsProxy::LacrosSelection() const {
  return BrowserInitParams::Get()->lacros_selection;
}

bool BrowserParamsProxy::IsFloatWindowEnabled() const {
  return BrowserInitParams::Get()->enable_float_window;
}

bool BrowserParamsProxy::IsCloudGamingDevice() const {
  return BrowserInitParams::Get()->is_cloud_gaming_device;
}

crosapi::mojom::BrowserInitParams::GpuSandboxStartMode
BrowserParamsProxy::GpuSandboxStartMode() const {
  return BrowserInitParams::Get()->gpu_sandbox_start_mode;
}

}  // namespace chromeos
