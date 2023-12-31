# Critical User Journeys for the dPWA Product

This file lists all critical user journeys that are required to have full test coverage of the dPWA product.

Existing documentation lives [here](/docs/webapps/integration-testing-framework.md).

TODO(dmurph): Move more documentation here. https://crbug.com/1314822

[[TOC]]

## How this file is parsed

The tables are parsed in this file as critical user journeys. Lines are considered a CUJ if:
- The first non-whitespace character is a `|`
- Splitting the line using the `|` character as a delimiter, the first item (stripping whitespace):
  - Does not start with `#`
  - Is not `---`
  - Is not empty

## App Identity Updating tests
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_by_user_windowed(Standalone) |  launch(Standalone) | check_app_title(Standalone, StandaloneOriginal) |
| WMLC | install_or_shortcut_by_user_windowed(Standalone) | manifest_update_title(Standalone, StandaloneUpdated) | accept_app_id_update_dialog | await_manifest_update | launch(Standalone) | check_app_title(Standalone, StandaloneUpdated) |
| WMLC | install_or_shortcut_by_user_windowed(Standalone) | manifest_update_title(Standalone, StandaloneUpdated) | deny_app_update_dialog | check_app_not_in_list | check_platform_shortcut_not_exists |
| WMLC | install_or_shortcut_by_user_windowed | manifest_update_icon | await_manifest_update | check_app_in_list_icon_correct |
| WMLC | install_or_shortcut_by_user_windowed | manifest_update_icon | await_manifest_update | check_platform_shortcut_and_icon |
| WMLC | install_policy_app(Standalone, NoShortcut, WindowOptions::All, WebApp) | manifest_update_title(Standalone, StandaloneUpdated) | check_update_dialog_not_shown  | await_manifest_update | launch(Standalone) | check_app_title(Standalone, StandaloneUpdated) |

## Run on OS Login
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WML | install_or_shortcut | apply_run_on_os_login_policy_blocked | check_user_cannot_set_run_on_os_login |
| WML | install_or_shortcut | enable_run_on_os_login | apply_run_on_os_login_policy_blocked | check_run_on_os_login_disabled |
| WML | install_or_shortcut | apply_run_on_os_login_policy_run_windowed | check_run_on_os_login_enabled |
| WML | install_or_shortcut | apply_run_on_os_login_policy_run_windowed | check_user_cannot_set_run_on_os_login |
| WML | install_or_shortcut | enable_run_on_os_login | check_run_on_os_login_enabled |
| WML | install_or_shortcut | enable_run_on_os_login | disable_run_on_os_login | check_run_on_os_login_disabled |
| WML | install_or_shortcut | apply_run_on_os_login_policy_run_windowed | remove_run_on_os_login_policy | check_run_on_os_login_disabled |
| WML | install_or_shortcut | enable_run_on_os_login | apply_run_on_os_login_policy_blocked | remove_run_on_os_login_policy | check_run_on_os_login_enabled |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients(Client2) | switch_profile_clients(Client1) | sync_turn_off | uninstall_by_user | switch_profile_clients(Client2) | apply_run_on_os_login_policy_run_windowed | check_run_on_os_login_disabled |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients(Client2) | switch_profile_clients(Client1) | sync_turn_off | uninstall_by_user | switch_profile_clients(Client2) | apply_run_on_os_login_policy_run_windowed | check_run_on_os_login_disabled | install_locally | check_run_on_os_login_enabled |

## Badging
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_by_user_windowed | set_app_badge | clear_app_badge | check_app_badge_empty |
| WMLC | install_or_shortcut_by_user_windowed | set_app_badge | check_app_badge_has_value |
| WMLC | navigate_browser(Standalone) | set_app_badge | check_platform_shortcut_not_exists |
| # Toolbar |
| WMLC | install_or_shortcut_windowed | navigate_pwa(Standalone, MinimalUi) | close_custom_toolbar | check_app_navigation_is_start_url |
| WMLC | install_or_shortcut_by_user_windowed | navigate_pwa(Standalone, MinimalUi) | check_custom_toolbar |
| # Initial state sanity checks |
| WMLC | navigate_browser(Standalone) | check_app_not_in_list |
| WMLC | navigate_browser(Standalone) | check_platform_shortcut_not_exists |
| WMLC | navigate_browser(NotPromotable) | check_app_not_in_list |
| WMLC | navigate_browser(NotPromotable) | check_platform_shortcut_not_exists |

# Installation
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut(Standalone) | check_app_title(Standalone, StandaloneUpdated) |
| WMLC | install_omnibox_icon(Screenshots) |
| WMLC | install_or_shortcut_by_user_windowed | check_window_created |
| WMLC | install_no_shortcut | check_platform_shortcut_not_exists |
| WMLC | install_no_shortcut(NotPromotable) | check_platform_shortcut_not_exists(NotPromotable) |
| WMLC | install_or_shortcut_tabbed | check_app_in_list_tabbed |
| WMLC | install_or_shortcut_tabbed | navigate_browser(Standalone) | check_create_shortcut_shown |
| WMLC | install_or_shortcut_tabbed | navigate_browser(Standalone) | check_install_icon_shown |
| WMLC | install_or_shortcut_tabbed | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_tabbed(NotPromotable) | check_app_in_list_tabbed(NotPromotable) |
| WMLC | install_or_shortcut_tabbed(NotPromotable) | navigate_browser(NotPromotable) | check_create_shortcut_shown |
| WMLC | install_or_shortcut_tabbed(NotPromotable) | navigate_browser(NotPromotable) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_tabbed(NotPromotable) | navigate_browser(NotPromotable) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_windowed | check_app_in_list_windowed |
| WMLC | install_or_shortcut_windowed | navigate_browser(Standalone) | check_create_shortcut_not_shown |
| WMLC | install_or_shortcut_windowed | navigate_browser(Standalone) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_windowed | navigate_browser(Standalone) | check_launch_icon_shown |
| WMLC | install_or_shortcut_windowed(MinimalUi) | navigate_browser(MinimalUi) | check_launch_icon_shown |
| WMLC | install_or_shortcut_windowed(NotPromotable) | check_app_in_list_windowed(NotPromotable) |
| WMLC | install_or_shortcut_windowed(NotPromotable) | navigate_browser(NotPromotable) | check_create_shortcut_not_shown |
| WMLC | install_or_shortcut_windowed(NotPromotable) | navigate_browser(NotPromotable) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_windowed(NotPromotable) | navigate_browser(NotPromotable) | check_launch_icon_shown |
| WMLC | install_or_shortcut_with_shortcut | check_platform_shortcut_and_icon |
| WMLC | install_or_shortcut_with_shortcut(NotPromotable) | check_platform_shortcut_and_icon(NotPromotable) |


## Uninstallation
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WML | install_or_shortcut_by_user_windowed | uninstall_by_user | check_app_not_in_list |
| WML | install_or_shortcut_by_user_windowed | uninstall_by_user | navigate_browser(Standalone) | check_install_icon_shown |
| WML | install_or_shortcut_by_user_windowed | uninstall_by_user | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed | uninstall_by_user | check_platform_shortcut_not_exists |
| WML | install_or_shortcut_by_user_windowed(NotPromotable) | uninstall_by_user(NotPromotable) | check_app_not_in_list |
| WML | install_or_shortcut_by_user_windowed(NotPromotable) | uninstall_by_user(NotPromotable) | check_platform_shortcut_not_exists(NotPromotable) |
| WMLC | install_or_shortcut_by_user_tabbed | uninstall_from_list | check_app_not_in_list |
| WMLC | install_or_shortcut_by_user_tabbed | uninstall_from_list | navigate_browser(Standalone) | check_install_icon_shown |
| WMLC | install_or_shortcut_by_user_tabbed | uninstall_from_list | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_by_user_tabbed | uninstall_from_list | check_platform_shortcut_not_exists |
| C | install_or_shortcut_by_user_windowed | uninstall_from_list | check_app_not_in_list |
| C | install_or_shortcut_by_user_windowed | uninstall_from_list | navigate_browser(Standalone) | check_install_icon_shown |
| C | install_or_shortcut_by_user_windowed | uninstall_from_list | navigate_browser(Standalone) | check_launch_icon_not_shown |
| C | install_or_shortcut_by_user_windowed | uninstall_from_list | check_platform_shortcut_not_exists |
| WMLC | install_or_shortcut_by_user_tabbed(NotPromotable) | uninstall_from_list(NotPromotable) | check_app_not_in_list |
| WMLC | install_or_shortcut_by_user_tabbed(NotPromotable) | uninstall_from_list(NotPromotable) | check_platform_shortcut_not_exists(NotPromotable) |
| C | install_or_shortcut_by_user_windowed(NotPromotable) | uninstall_from_list(NotPromotable) | check_app_not_in_list |
| C | install_or_shortcut_by_user_windowed(NotPromotable) | uninstall_from_list(NotPromotable) | check_platform_shortcut_not_exists(NotPromotable) |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_app_not_in_list |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | uninstall_policy_app | navigate_browser(Standalone) | check_install_icon_shown |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | uninstall_policy_app | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_platform_shortcut_not_exists | check_app_not_in_list |

# Launch behavior tests
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_windowed | launch | check_window_created |
| WMLC | install_or_shortcut_windowed | launch | check_window_display_standalone |
| WMLC | install_or_shortcut_tabbed | set_open_in_window | launch | check_window_created |
| WMLC | install_or_shortcut_windowed | set_open_in_tab | launch_from_shortcut_or_list | check_tab_created |
| WMLC | install_or_shortcut_tabbed(NotPromotable) | launch_from_shortcut_or_list(NotPromotable) | check_tab_created |
| WMLC | install_or_shortcut_windowed(MinimalUi) | launch(MinimalUi) | check_window_display_minimal |
| WMLC | install_or_shortcut_windowed(NotPromotable) | launch(NotPromotable) | check_window_created |

# Misc UX Flows
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_no_shortcut | create_shortcuts_from_list | check_platform_shortcut_and_icon |
| WMLC | install_or_shortcut | delete_profile | check_app_list_empty |
| WMLC | install_or_shortcut | delete_profile | check_app_not_in_list |
| WMLC | install_or_shortcut_with_shortcut | delete_profile | check_platform_shortcut_not_exists |
| WMLC | install_or_shortcut_tabbed_with_shortcut | delete_platform_shortcut | create_shortcuts_from_list | launch_from_platform_shortcut | check_tab_created |
| WMLC | install_or_shortcut_windowed_with_shortcut | delete_platform_shortcut | create_shortcuts_from_list | launch_from_platform_shortcut | check_window_created |
| WMLC | install_tabbed_no_shortcut | create_shortcuts_from_list | launch_from_platform_shortcut | check_tab_created |
| WMLC | install_windowed_no_shortcut | create_shortcuts_from_list | launch_from_platform_shortcut | check_window_created |
| WMLC | install_or_shortcut_by_user_windowed | open_in_chrome | check_tab_created |
| WMLC | install_or_shortcut_by_user_windowed | navigate_pwa(Standalone, MinimalUi) | open_in_chrome | check_tab_created |
| WML | install_or_shortcut_windowed | open_app_settings | check_browser_navigation_is_app_settings |

## Sync-initiated install tests
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WML | install_or_shortcut_by_user | switch_profile_clients | install_locally | check_platform_shortcut_and_icon |
| WML | install_or_shortcut_by_user_tabbed | switch_profile_clients | install_locally | check_app_in_list_tabbed |
| WML | install_or_shortcut_by_user_tabbed | switch_profile_clients | install_locally | navigate_browser(Standalone) | check_install_icon_shown |
| WML | install_or_shortcut_by_user_tabbed | switch_profile_clients | install_locally | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | install_locally | check_app_in_list_windowed |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | install_locally | navigate_browser(Standalone) | check_install_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | install_locally | navigate_browser(Standalone) | check_launch_icon_shown |
| WML | install_or_shortcut_by_user_tabbed(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | check_app_in_list_tabbed(NotPromotable) |
| WML | install_or_shortcut_by_user_tabbed(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | navigate_browser(NotPromotable) | check_launch_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | check_app_in_list_windowed(NotPromotable) |
| WML | install_or_shortcut_by_user_windowed(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | navigate_browser(NotPromotable) | check_install_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | navigate_browser(NotPromotable) | check_launch_icon_shown |
| WML | install_or_shortcut_by_user(NotPromotable) | switch_profile_clients | install_locally(NotPromotable) | check_platform_shortcut_and_icon(NotPromotable) |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | install_locally | launch | check_window_created |
| WMLC | install_or_shortcut_by_user_tabbed | switch_profile_clients | launch_from_shortcut_or_list | check_tab_created |
| WML | install_or_shortcut_by_user_tabbed | switch_profile_clients | install_locally | launch_from_shortcut_or_list | check_tab_created |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | launch_from_shortcut_or_list | check_tab_created |
| WMLC | install_or_shortcut_by_user | switch_profile_clients | uninstall_from_list | check_app_not_in_list |
| WMLC | install_or_shortcut_by_user | switch_profile_clients | uninstall_from_list | switch_profile_clients(Client1) | check_app_not_in_list |
| WML | install_or_shortcut_by_user | switch_profile_clients | check_app_in_list_not_locally_installed |
| C | install_or_shortcut_by_user | switch_profile_clients | check_platform_shortcut_and_icon(Standalone) |
| WML | install_or_shortcut_by_user | switch_profile_clients | check_platform_shortcut_not_exists |
| C | install_or_shortcut_by_user_tabbed | switch_profile_clients | check_app_in_list_tabbed |
| C | install_or_shortcut_by_user_windowed | switch_profile_clients | check_app_in_list_windowed |
| C | install_or_shortcut_by_user_windowed | switch_profile_clients | navigate_browser(Standalone) | check_install_icon_not_shown |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | navigate_browser(Standalone) | check_install_icon_shown |
| WML | install_or_shortcut_by_user_windowed | switch_profile_clients | navigate_browser(Standalone) | check_launch_icon_not_shown |
| C | install_or_shortcut_by_user_windowed | switch_profile_clients | navigate_browser(Standalone) | check_launch_icon_shown |
| WML | install_or_shortcut_by_user(NotPromotable) | switch_profile_clients | check_app_in_list_not_locally_installed(NotPromotable) |
| WML | install_or_shortcut_by_user(NotPromotable) | switch_profile_clients | check_platform_shortcut_not_exists(NotPromotable) |
| WML | sync_turn_off | install_or_shortcut_by_user | sync_turn_on | switch_profile_clients | check_app_in_list_not_locally_installed |
| WML | sync_turn_off | install_or_shortcut_by_user(NotPromotable) | sync_turn_on | switch_profile_clients | check_app_in_list_not_locally_installed(NotPromotable) |
| WML | install_or_shortcut_by_user | switch_profile_clients(Client2) | sync_turn_off | uninstall_not_locally_installed | sync_turn_on | check_app_in_list_not_locally_installed |
| WML | install_or_shortcut_by_user | switch_profile_clients(Client2) | sync_turn_off | uninstall_not_locally_installed | sync_turn_on | check_platform_shortcut_not_exists |
| C | install_or_shortcut_by_user_tabbed | switch_profile_clients(Client2) | sync_turn_off | uninstall_by_user | sync_turn_on | check_app_in_list_tabbed |
| C | install_or_shortcut_by_user_tabbed | switch_profile_clients(Client2) | sync_turn_off | uninstall_by_user | sync_turn_on | check_platform_shortcut_and_icon |
| C | install_or_shortcut_by_user_windowed | switch_profile_clients(Client2) | sync_turn_off | uninstall_by_user | sync_turn_on | check_app_in_list_windowed |
| C | install_or_shortcut_by_user_windowed | switch_profile_clients(Client2) | sync_turn_off | uninstall_by_user | sync_turn_on | check_platform_shortcut_and_icon |

## Policy installation and user installation interactions
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | check_platform_shortcut_and_icon |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, NoShortcut, WindowOptions::All, WebApp) | check_platform_shortcut_and_icon |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | check_app_in_list_windowed |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | navigate_browser(Standalone) | check_launch_icon_shown |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | check_app_in_list_tabbed |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | navigate_browser(Standalone) | check_install_icon_shown |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | launch | check_window_created |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | install_or_shortcut_by_user_windowed | check_app_in_list_windowed |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | install_or_shortcut_by_user_windowed | check_platform_shortcut_and_icon |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | install_or_shortcut_by_user_windowed | check_window_created |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, Windowed, WebApp) | launch_from_shortcut_or_list | check_tab_created |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_app_in_list_tabbed |
| WMLC | install_or_shortcut_by_user_tabbed | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_platform_shortcut_and_icon |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_app_in_list_windowed |
| WMLC | install_or_shortcut_by_user_windowed | install_policy_app(Standalone, ShortcutOptions::All, WindowOptions::All, WebApp) | uninstall_policy_app | check_platform_shortcut_and_icon |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | install_or_shortcut_by_user_windowed | uninstall_policy_app | check_app_in_list_windowed |
| WMLC | install_policy_app(Standalone, ShortcutOptions::All, Browser, WebApp) | install_or_shortcut_by_user_windowed | uninstall_policy_app | check_platform_shortcut_and_icon |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Windowed, WebShortcut) | launch(StandaloneNotStartUrl) | check_app_navigation(StandaloneNotStartUrl) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Windowed, WebApp) | launch(Standalone) | check_app_navigation(Standalone) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Browser, WebApp) | launch_from_chrome_apps(Standalone) | check_browser_navigation(Standalone) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Browser, WebApp) | launch_from_platform_shortcut(Standalone) | check_browser_navigation(Standalone) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Browser, WebShortcut) | launch_from_chrome_apps(StandaloneNotStartUrl) | check_browser_navigation(StandaloneNotStartUrl) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, Browser, WebShortcut) | launch_from_platform_shortcut(StandaloneNotStartUrl) | check_browser_navigation(StandaloneNotStartUrl) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, WindowOptions::All, WebApp) | check_app_not_in_list(StandaloneNotStartUrl) | check_app_in_list_icon_correct(Standalone) |
| WMLC | install_policy_app(StandaloneNotStartUrl, WithShortcut, WindowOptions::All, WebShortcut) | check_app_not_in_list(Standalone) | check_app_in_list_icon_correct(StandaloneNotStartUrl) |



## Manifest update tests
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_by_user_windowed | manifest_update_colors | await_manifest_update | launch | check_window_color_correct |
| WMLC | install_or_shortcut_by_user_windowed | manifest_update_display(Standalone, Browser) | await_manifest_update | launch | check_tab_not_created |
| WMLC | install_or_shortcut_by_user_windowed | manifest_update_display(Standalone, Browser) | await_manifest_update | launch | check_window_created |
| WMLC | install_or_shortcut_by_user_windowed | manifest_update_display(Standalone, Browser) | await_manifest_update | launch | check_window_display_minimal |
| WMLC | install_or_shortcut_by_user_windowed | manifest_update_display(Standalone, MinimalUi) | await_manifest_update | launch | check_window_display_minimal |
| WMLC | install_or_shortcut_by_user_windowed(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | launch_from_platform_shortcut(StandaloneNestedA) | navigate_browser(Standalone) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_by_user_windowed(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | launch_from_platform_shortcut(StandaloneNestedA) | navigate_browser(Standalone) | check_launch_icon_shown |
| WMLC | install_or_shortcut_by_user_windowed(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | launch(StandaloneNestedA) | navigate_pwa(StandaloneNestedA, StandaloneNestedB) | check_no_toolbar |
| WMLC | install_or_shortcut_by_user_windowed(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | navigate_browser(StandaloneNestedB) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_by_user_windowed(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | navigate_browser(StandaloneNestedB) | check_launch_icon_shown |
| WMLC | install_or_shortcut_by_user_windowed(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | navigate_browser(StandaloneNestedA) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_by_user_windowed(StandaloneNestedA) | manifest_update_scope_to(StandaloneNestedA, Standalone) | await_manifest_update(StandaloneNestedA) | navigate_browser(StandaloneNestedA) | check_launch_icon_shown |

## Browser UX with edge cases
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | switch_incognito_profile | navigate_browser(Standalone) | check_create_shortcut_not_shown |
| WMLC | switch_incognito_profile | navigate_browser(NotPromotable) | check_create_shortcut_not_shown |
| WMLC | navigate_crashed_url | check_create_shortcut_not_shown |
| WMLC | navigate_crashed_url | check_install_icon_not_shown |
| WMLC | navigate_notfound_url | check_create_shortcut_not_shown |
| WMLC | navigate_notfound_url | check_install_icon_not_shown |

## Site promotability checking
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | navigate_browser(Standalone) | check_create_shortcut_shown |
| WMLC | navigate_browser(StandaloneNestedA) | check_install_icon_shown |
| WMLC | navigate_browser(NotPromotable) | check_create_shortcut_shown |
| WMLC | navigate_browser(NotPromotable) | check_install_icon_not_shown |

## In-Browser UX (install icon, launch icon, etc)
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_windowed(StandaloneNestedA) | navigate_browser(NotInstalled) | check_install_icon_shown |
| WMLC | install_or_shortcut_windowed(StandaloneNestedA) | navigate_browser(NotInstalled) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_windowed | navigate_browser(StandaloneNestedA) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_windowed | navigate_browser(StandaloneNestedA) | check_launch_icon_shown |
| WMLC | install_or_shortcut_by_user_windowed | navigate_browser(MinimalUi) | check_install_icon_shown |
| WMLC | install_or_shortcut_by_user_windowed | navigate_browser(MinimalUi) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_by_user_windowed | navigate_pwa(Standalone, MinimalUi) | check_app_title(Standalone, StandaloneOriginal) |
| WMLC | install_or_shortcut_by_user_windowed | switch_incognito_profile | navigate_browser(Standalone) | check_launch_icon_not_shown |
| WMLC | install_or_shortcut_by_user_windowed | set_open_in_tab | check_app_in_list_tabbed |
| WMLC | install_or_shortcut_by_user_windowed | set_open_in_tab | navigate_browser(Standalone) | check_install_icon_shown |
| WMLC | install_or_shortcut_by_user_tabbed | set_open_in_window | check_app_in_list_windowed |
| WMLC | install_or_shortcut_by_user_tabbed | set_open_in_window | navigate_browser(Standalone) | check_install_icon_not_shown |
| WMLC | install_or_shortcut_by_user_tabbed | set_open_in_window | navigate_browser(Standalone) | check_launch_icon_shown |

## Windows Control Overlay
| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut_windowed(Wco) | check_window_controls_overlay_toggle(Wco, Shown) |
| WMLC | install_or_shortcut_windowed(Standalone) | check_window_controls_overlay_toggle(Standalone, NotShown) |
| WMLC | install_or_shortcut_windowed(Wco) | enable_window_controls_overlay(Wco) | check_window_controls_overlay(Wco, On) |
| WMLC | install_or_shortcut_windowed(Wco) | enable_window_controls_overlay(Wco) | check_window_controls_overlay_toggle(Wco, Shown) |
| WMLC | install_or_shortcut_windowed(Wco) | enable_window_controls_overlay(Wco) | disable_window_controls_overlay(Wco) | check_window_controls_overlay(Wco, Off) |
| WMLC | install_or_shortcut_windowed(Wco) | enable_window_controls_overlay(Wco) | disable_window_controls_overlay(Wco) | check_window_controls_overlay_toggle(Wco, Shown) |
| WMLC | install_or_shortcut_windowed(Wco) | enable_window_controls_overlay(Wco) | launch(Wco) | check_window_controls_overlay(Wco, On) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | manifest_update_display(MinimalUi, Wco) | await_manifest_update(MinimalUi) | launch(MinimalUi) | check_window_controls_overlay_toggle(MinimalUi, Shown) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | manifest_update_display(MinimalUi, Wco) | await_manifest_update(MinimalUi) | launch(MinimalUi) | check_window_controls_overlay_toggle(MinimalUi, Shown) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | manifest_update_display(MinimalUi, Wco) | await_manifest_update(MinimalUi) | launch(MinimalUi) | enable_window_controls_overlay(MinimalUi) | check_window_controls_overlay(MinimalUi, On) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | manifest_update_display(MinimalUi, Wco) | await_manifest_update(MinimalUi) | launch(MinimalUi) | enable_window_controls_overlay(MinimalUi) | check_window_controls_overlay_toggle(MinimalUi, Shown) |
| WMLC | install_or_shortcut_windowed(MinimalUi) | manifest_update_display(MinimalUi, Wco) | await_manifest_update(MinimalUi) | launch(MinimalUi) | enable_window_controls_overlay(MinimalUi) | check_window_controls_overlay_toggle(MinimalUi, Shown) |
| WMLC | install_or_shortcut_windowed(Wco) | manifest_update_display(Wco, Standalone) | await_manifest_update(Wco) | launch(Wco) | check_window_controls_overlay_toggle(Wco, NotShown) |
| WMLC | install_or_shortcut_windowed(Wco) | manifest_update_display(Wco, Standalone) | await_manifest_update(Wco) | launch(Wco) | check_window_controls_overlay(Wco, Off) |

## File Handling

| #Platforms | Test -> | | | | | | | | | | | | | | | | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| WMLC | install_or_shortcut(MinimalUi) | check_site_handles_file(MinimalUi, Txt) | check_site_handles_file(MinimalUi, Png) |
| # Single open & multiple open behavior |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(OneTextFile) | check_file_handling_dialog(Shown) |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(OneTextFile) | file_handling_dialog(Allow, AskAgain) | check_pwa_window_created(MinimalUi, One) | check_files_loaded_in_site(MinimalUi, OneTextFile) |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(MultipleTextFiles) | check_file_handling_dialog(Shown)
| WMLC | install_or_shortcut(MinimalUi) | launch_file(MultipleTextFiles) | file_handling_dialog(Allow, AskAgain) | check_pwa_window_created(MinimalUi, One) | check_files_loaded_in_site(MinimalUi, MultipleTextFiles) |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(OnePngFile) | check_file_handling_dialog(Shown) |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(OnePngFile) | file_handling_dialog(Allow, AskAgain) | check_pwa_window_created(MinimalUi, One) | check_files_loaded_in_site(MinimalUi, OnePngFile) |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(MultiplePngFiles) | check_file_handling_dialog(Shown) |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(MultiplePngFiles) | file_handling_dialog(Allow, AskAgain) | check_pwa_window_created(MinimalUi, Two) | check_files_loaded_in_site(MinimalUi, MultiplePngFiles) |
| # Dialog options |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(OneTextFile) | file_handling_dialog(Allow, Remember) | launch_file(OneTextFile) | check_file_handling_dialog(NotShown) | check_pwa_window_created(MinimalUi, One) |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(OneTextFile) | file_handling_dialog(Allow, AskAgain) | launch_file(OneTextFile) | check_file_handling_dialog(Shown) |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(OneTextFile) | file_handling_dialog(Deny, AskAgain) | check_window_not_created | check_site_handles_file(MinimalUi, Txt) | check_site_handles_file(MinimalUi, Png) |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(OneTextFile) | file_handling_dialog(Deny, AskAgain) | launch_file(OneTextFile) | check_file_handling_dialog(Shown) |
| WMLC | install_or_shortcut(MinimalUi) | launch_file(OneTextFile) | file_handling_dialog(Deny, Remember) | check_window_not_created | check_site_not_handles_file(MinimalUi, Txt) | check_site_not_handles_file(MinimalUi, Png) |
| # Policy approval |
| WMLC | install_or_shortcut(MinimalUi) | add_file_handling_policy_approval(MinimalUi) | launch_file(OneTextFile) | check_file_handling_dialog(NotShown) | check_pwa_window_created(MinimalUi, One) |
| WMLC | install_or_shortcut(MinimalUi) | add_file_handling_policy_approval(MinimalUi) | remove_file_handling_policy_approval(MinimalUi) | launch_file(OneTextFile) | check_file_handling_dialog(Shown) |
