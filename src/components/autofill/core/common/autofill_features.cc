// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/autofill/core/common/autofill_features.h"

#include "build/build_config.h"
#include "build/chromeos_buildflags.h"

namespace autofill::features {

// Controls whether to flatten and fill cross-iframe forms.
// TODO(crbug.com/1187842) Remove once launched.
BASE_FEATURE(kAutofillAcrossIframes,
             "AutofillAcrossIframes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/1135188): Remove this feature flag after the explicit save
// prompts for address profiles is complete.
// When enabled, a save prompt will be shown to user upon form submission before
// storing any detected address profile.
BASE_FEATURE(kAutofillAddressProfileSavePrompt,
             "AutofillAddressProfileSavePrompt",
             base::FEATURE_ENABLED_BY_DEFAULT);

// This parameter controls if save profile prompts are automatically blocked for
// a given domain after N (default is 3) subsequent declines.
const base::FeatureParam<bool> kAutofillAutoBlockSaveAddressProfilePrompt{
    &kAutofillAddressProfileSavePrompt, "save_profile_prompt_auto_block", true};
// The auto blocking feature is based on a strike model. This parameter defines
// the months before such strikes expire.
const base::FeatureParam<int>
    kAutofillAutoBlockSaveAddressProfilePromptExpirationDays{
        &kAutofillAddressProfileSavePrompt,
        "save_profile_prompt_auto_block_strike_expiration_days", 180};
// The number of strikes before the prompt gets blocked.
const base::FeatureParam<int>
    kAutofillAutoBlockSaveAddressProfilePromptStrikeLimit{
        &kAutofillAddressProfileSavePrompt,
        "save_profile_prompt_auto_block_strike_limit", 3};

// Same as above but for update bubbles.
const base::FeatureParam<bool> kAutofillAutoBlockUpdateAddressProfilePrompt{
    &kAutofillAddressProfileSavePrompt, "update_profile_prompt_auto_block",
    true};
// Same as above but for update bubbles.
const base::FeatureParam<int>
    kAutofillAutoBlockUpdateAddressProfilePromptExpirationDays{
        &kAutofillAddressProfileSavePrompt,
        "update_profile_prompt_auto_block_strike_expiration_days", 180};
// Same as above but for update bubbles.
const base::FeatureParam<int>
    kAutofillAutoBlockUpdateAddressProfilePromptStrikeLimit{
        &kAutofillAddressProfileSavePrompt,
        "update_profile_prompt_auto_block_strike_limit", 3};

// TODO(crbug.com/1135188): Remove this feature flag after the explicit save
// prompts for address profiles is complete.
// When enabled, address data will be verified and autocorrected in the
// save/update prompt before saving an address profile. Relevant only if the
// AutofillAddressProfileSavePrompt feature is enabled.
BASE_FEATURE(kAutofillAddressProfileSavePromptAddressVerificationSupport,
             "AutofillAddressProfileSavePromptAddressVerificationSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// TODO(crbug.com/1135188): Remove this feature flag after the explicit save
// prompts for address profiles is complete.
// When enabled, address profile save problem will contain a dropdown for
// assigning a nickname to the address profile. Relevant only if the
// AutofillAddressProfileSavePrompt feature is enabled.
BASE_FEATURE(kAutofillAddressProfileSavePromptNicknameSupport,
             "AutofillAddressProfileSavePromptNicknameSupport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// By default, AutofillAgent and, if |kAutofillProbableFormSubmissionInBrowser|
// is enabled, also ContentAutofillDriver omit duplicate form submissions, even
// though the form's data may have changed substantially. If enabled, the
// below feature allows duplicate form submissions.
// TODO(crbug/1117451): Remove once the form-submission experiment is over.
BASE_FEATURE(kAutofillAllowDuplicateFormSubmissions,
             "AutofillAllowDuplicateFormSubmissions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether autofill activates on non-HTTP(S) pages. Useful for
// automated with data URLS in cases where it's too difficult to use the
// embedded test server. Generally avoid using.
BASE_FEATURE(kAutofillAllowNonHttpActivation,
             "AutofillAllowNonHttpActivation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the two most recent address forms and the most recent credit card
// forms, which were submitted on the same origin, are associated with each
// other. The association only happens if at most `kAutofillAssociateFormsTTL`
// time passes between all submissions.
BASE_FEATURE(kAutofillAssociateForms,
             "AutofillAssociateForms",
             base::FEATURE_DISABLED_BY_DEFAULT);
const base::FeatureParam<base::TimeDelta> kAutofillAssociateFormsTTL{
    &kAutofillAssociateForms, "associate_forms_ttl", base::Minutes(5)};

// When enabled, Autofill ignores invalid country information on import, which
// would otherwise prevent an import. Instead, ignoring it will trigger the
// country complemention logic.
// TODO(crbug.com/1362472): Cleanup when launched.
BASE_FEATURE(kAutofillIgnoreInvalidCountryOnImport,
             "AutofillIgnoreInvalidCountryOnImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, the country calling code for nationally formatted phone numbers
// is inferred from the profile's country, if available.
// TODO(crbug.com/1311937): Cleanup when launched.
BASE_FEATURE(kAutofillInferCountryCallingCode,
             "AutofillInferCountryCallingCode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, complementing the country happens before setting the phone number
// on profile import. This way, the variation country code takes precedence over
// the app locale.
// TODO(crbug.com/1295721): Cleanup when launched.
BASE_FEATURE(kAutofillComplementCountryEarly,
             "AutofillComplementCountryEarly",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, label inference considers strings entirely made up of  '(', ')'
// and '-' as valid labels.
// TODO(crbug.com/1311937): Cleanup when launched.
BASE_FEATURE(kAutofillConsiderPhoneNumberSeparatorsValidLabels,
             "AutofillConsiderPhoneNumberSeparatorsValidLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, local heuristics fall back to the fields placeholder attribute.
BASE_FEATURE(kAutofillConsiderPlaceholderForParsing,
             "AutofillConsiderPlaceholderForParsing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Chrome needs to map country names ("Italy"/"Italien") to country codes
// ("IT"). If enabled, the lookup considers all locales that are registered
// for a country. This helps in case a Chrome fails to determine the language
// of a website.
// TODO(crbug.com/1360502): Cleanup when launched.
BASE_FEATURE(kAutofillCountryFromLocalName,
             "AutofillCountryFromLocalName",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, three address profiles are created for testing.
BASE_FEATURE(kAutofillCreateDataForTest,
             "AutofillCreateDataForTest",
             base::FEATURE_DISABLED_BY_DEFAULT);

// FormStructure::RetrieveFromCache used to preserve an AutofillField's
// is_autofilled from the cache of previously parsed forms. This makes little
// sense because the renderer sends us the autofill state and has the most
// recent information. Dropping the old behavior should not make any difference
// but to be sure, this is gated by a finch experiment.
// TODO(crbug.com/1373362) Cleanup when launched.
BASE_FEATURE(kAutofillDontPreserveAutofillState,
             "AutofillDontPreserveAutofillState",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, checking whether a form has disappeared after an Ajax response is
// delayed because subsequent Ajax responses may restore the form. If disabled,
// the check happens right after a successful Ajax response.
BASE_FEATURE(kAutofillDeferSubmissionClassificationAfterAjax,
             "AutofillDeferSubmissionClassificationAfterAjax",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, we try to fill and import from fields based on available
// heuristic or server suggestions even if the autocomplete attribute is not
// specified by the web standard. This does not affect the moments when the UI
// is shown.
// TODO(crbug.com/1295728): Remove the feature when the experiment is completed.
BASE_FEATURE(kAutofillFillAndImportFromMoreFields,
             "AutofillFillAndImportFromMoreFields",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, autofill searches for format strings (like "MM/YY", "MM / YY",
// "MM/YYYY") in the label or placeholder of input elements and uses these
// to fill expiration dates.
// TODO(crbug.com/1326244): Cleanup when launched.
BASE_FEATURE(kAutofillFillCreditCardAsPerFormatString,
             "AutofillFillCreditCardAsPerFormatString",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Kill switch for Autofill filling.
BASE_FEATURE(kAutofillDisableFilling,
             "AutofillDisableFilling",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for Autofill address import.
BASE_FEATURE(kAutofillDisableAddressImport,
             "AutofillDisableAddressImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Kill switch for computing heuristics other than the active ones
// (GetActivePatternSource()).
BASE_FEATURE(kAutofillDisableShadowHeuristics,
             "AutofillDisableShadowHeuristics",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, autofill will use the new ranking algorithm for card and
// profile autofill suggestions.
BASE_FEATURE(kAutofillEnableRankingFormula,
             "AutofillEnableRankingFormula",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The half life applied to the use count.
const base::FeatureParam<int> kAutofillRankingFormulaUsageHalfLife{
    &kAutofillEnableRankingFormula, "autofill_ranking_formula_usage_half_life",
    20};
// The boost factor applied to ranking virtual cards.
const base::FeatureParam<int> kAutofillRankingFormulaVirtualCardBoost{
    &kAutofillEnableRankingFormula,
    "autofill_ranking_formula_virtual_card_boost", 5};
// The half life applied to the virtual card boost.
const base::FeatureParam<int> kAutofillRankingFormulaVirtualCardBoostHalfLife{
    &kAutofillEnableRankingFormula,
    "autofill_ranking_formula_virtual_card_boost_half_life", 15};

// Controls if the heuristic field parsing utilizes shared labels.
// TODO(crbug.com/1165780): Remove once shared labels are launched.
BASE_FEATURE(kAutofillEnableSupportForParsingWithSharedLabels,
             "AutofillEnableSupportForParsingWithSharedLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Chrome support filling and importing apartment numbers.
// TODO(crbug.com/1153715): Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForApartmentNumbers,
             "AutofillEnableSupportForApartmentNumbers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether we download server credit cards to the ephemeral
// account-based storage when sync the transport is enabled.
BASE_FEATURE(kAutofillEnableAccountWalletStorage,
             "AutofillEnableAccountWalletStorage",
#if BUILDFLAG(IS_CHROMEOS_ASH)
             // Wallet transport is currently unavailable on ChromeOS.
             base::FEATURE_DISABLED_BY_DEFAULT
#else
             base::FEATURE_ENABLED_BY_DEFAULT
#endif
);

// Enables parsing for birthdate fields. Filling is not supported and parsing
// is meant to prevent false positive credit card expiration dates.
// TODO(crbug.com/1306654): Remove once launched.
BASE_FEATURE(kAutofillEnableBirthdateParsing,
             "AutofillEnableBirthdateParsing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Autofill parses ADDRESS_HOME_DEPENDENT_LOCALITY.
// TODO(crbug.com/1157405): Remove once launched.
BASE_FEATURE(kAutofillEnableDependentLocalityParsing,
             "AutofillEnableDependentLocalityParsing",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enables the augmentation layer for setting-inaccessible fields, which allows
// extending Autofill's address format by additional fields.
// TODO(crbug.com/1300548) Remove when launched.
BASE_FEATURE(kAutofillEnableExtendedAddressFormats,
             "AutofillEnableExtendedAddressFormats",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to save the first number in a form with multiple phone
// numbers instead of aborting the import.
// TODO(crbug.com/1167484) Remove once launched.
BASE_FEATURE(kAutofillEnableImportWhenMultiplePhoneNumbers,
             "AutofillEnableImportWhenMultiplePhoneNumbers",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, candidate profiles are temporary stored on import, and merged
// with future candidate profiles, to create an importable profile. This makes
// importing from multi-step input flows possible.
BASE_FEATURE(kAutofillEnableMultiStepImports,
             "AutofillEnableMultiStepImports",
             base::FEATURE_DISABLED_BY_DEFAULT);
// When enabled, imported profiles are stored as multi-step candidates too,
// which enables complementing a recently imported profile during later steps of
// a multi-step input flow.
const base::FeatureParam<bool> kAutofillEnableMultiStepImportComplements{
    &kAutofillEnableMultiStepImports, "enable_multistep_complement", false};
// Configures the TTL of multi-step import candidates.
const base::FeatureParam<base::TimeDelta> kAutofillMultiStepImportCandidateTTL{
    &kAutofillEnableMultiStepImports, "multistep_candidate_ttl",
    base::Minutes(30)};

// When enabled, phone number local heuristics match empty labels when looking
// for composite phone number inputs. E.g. Phone number <input><input>.
BASE_FEATURE(kAutofillEnableParsingEmptyPhoneNumberLabels,
             "AutofillEnableParsingEmptyPhoneNumberLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the precedence is given to the field label over the name when
// they match different types. Applied only for parsing of address forms in
// Turkish.
// TODO(crbug.com/1156315): Remove once launched.
BASE_FEATURE(kAutofillEnableLabelPrecedenceForTurkishAddresses,
             "AutofillEnableLabelPrecedenceForTurkishAddresses",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, the address profile deduplication logic runs after the browser
// startup, once per chrome version.
BASE_FEATURE(kAutofillEnableProfileDeduplication,
             "AutofillEnableProfileDeduplication",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if Autofill supports merging subset names.
// TODO(crbug.com/1098943): Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForMergingSubsetNames,
             "AutofillEnableSupportForMergingSubsetNames",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether honorific prefix is shown and editable in Autofill Settings
// on Android, iOS and Desktop.
// TODO(crbug.com/1141460): Remove once launched.
BASE_FEATURE(kAutofillEnableSupportForHonorificPrefixes,
             "AutofillEnableSupportForHonorificPrefixes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, trunk prefix-related phone number types are added to the
// supported and matching types of |PhoneNumber|. Local heuristics for these
// types are enabled as well.
BASE_FEATURE(kAutofillEnableSupportForPhoneNumberTrunkTypes,
             "AutofillEnableSupportForPhoneNumberTrunkTypes",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill monitors whether JavaScript modifies autofilled
// credit card expiration dates and tries to fix a specific failure scenario.
// After filling an expiration date "05/2023", some websites try to fix the
// formatting and replace it with "05 / 20" instead of "05 / 23". When this
// experiment is enabled, Chrome replaces the "05 / 20" with "05 / 23".
// TODO(crbug.com/1314360): Remove once launched.
BASE_FEATURE(kAutofillRefillModifiedCreditCardExpirationDates,
             "AutofillRefillModifiedCreditCardExpirationDates",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Enables autofill to function within a FencedFrame, and is disabled by
// default.
// TODO(crbug.com/1294378): Remove once launched.
BASE_FEATURE(kAutofillEnableWithinFencedFrame,
             "AutofillEnableWithinFencedFrame",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether or not all datalist shall be extracted into FormFieldData.
// This feature is enabled in both WebView and WebLayer where all datalists
// instead of only the focused one shall be extracted and sent to Android
// autofill service when the autofill session created.
BASE_FEATURE(kAutofillExtractAllDatalists,
             "AutofillExtractAllDatalists",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls if type-specific popup widths are used.
// TODO(crbug.com/1250729): Remove once launched.
BASE_FEATURE(kAutofillTypeSpecificPopupWidth,
             "AutofillTypeSpecificPopupWidth",
             base::FEATURE_ENABLED_BY_DEFAULT);

// When enabled, HTML autocomplete values that do not map to any known type, but
// look reasonable (e.g. contain "address") are simply ignored. Without the
// feature, Autofill is disabled on such fields.
BASE_FEATURE(kAutofillIgnoreUnmappableAutocompleteValues,
             "AutofillIgnoreUnmappableAutocompleteValues",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, <label for=..> inference relies on control.labels() instead of
// iterating through all <label> tags manually.
// TODO(crbug.com/1339277) Remove once launched.
BASE_FEATURE(kAutofillImprovedLabelForInference,
             "AutofillImprovedLabelForInference",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, only changed values are highlighted in preview mode.
// TODO(crbug/1248585): Remove when launched.
BASE_FEATURE(kAutofillHighlightOnlyChangedValuesInPreviewMode,
             "AutofillHighlightOnlyChangedValuesInPreviewMode",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill suggestions are displayed in the keyboard accessory
// instead of the regular popup.
BASE_FEATURE(kAutofillKeyboardAccessory,
             "AutofillKeyboardAccessory",
             base::FEATURE_DISABLED_BY_DEFAULT);

// When enabled, Autofill will use new logic to strip both prefixes
// and suffixes when setting FormStructure::parseable_name_
BASE_FEATURE(kAutofillLabelAffixRemoval,
             "AutofillLabelAffixRemoval",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Enabled a suggestion menu that is aligned to the center of the field.
// TODO(crbug/1248339): Remove once experiment is finished.
BASE_FEATURE(kAutofillCenterAlignedSuggestions,
             "AutofillCenterAlignedSuggestions",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls the maximum pixels the popup is shifted towards the center.
// TODO(crbug/1248339): Remove once experiment is finished.
extern const base::FeatureParam<int>
    kAutofillMaximumPixelsToMoveSuggestionopupToCenter{
        &kAutofillCenterAlignedSuggestions,
        "maximum_pixels_to_move_the_suggestion_popup__towards_the_fields_"
        "center",
        120};

// Controls the width percentage to move the popup towards the center.
// TODO(crbug/1248339): Remove once experiment is finished.
extern const base::FeatureParam<int>
    kAutofillMaxiumWidthPercentageToMoveSuggestionPopupToCenter{
        &kAutofillCenterAlignedSuggestions,
        "width_percentage_to_shift_the_suggestion_popup_towards_the_center_of_"
        "fields",
        50};

// When enabled, Autofill would not override the field values that were either
// filled by Autofill or on page load.
// TODO(crbug/1275649): Remove once experiment is finished.
BASE_FEATURE(kAutofillPreventOverridingPrefilledValues,
             "AutofillPreventOverridingPrefilledValues",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, use the parsing patterns from a JSON file for heuristics, rather
// than the hardcoded ones from autofill_regex_constants.cc.
// The specific pattern set is controlled by the
// `kAutofillParsingPatternActiveSource` parameter.
//
// This feature is intended to work with kAutofillPageLanguageDetection.
//
// Enabling this feature is also a prerequisite for emitting shadow metrics.
// TODO(crbug/1121990): Remove once launched.
BASE_FEATURE(kAutofillParsingPatternProvider,
             "AutofillParsingPatternProvider",
             base::FEATURE_DISABLED_BY_DEFAULT);

// The specific pattern set is controlled by the `kAutofillParsingPatternActive`
// parameter. One of "legacy", "default", "experimental", "nextgen". All other
// values are equivalent to "default".
// TODO(crbug/1248339): Remove once experiment is finished.
const base::FeatureParam<std::string> kAutofillParsingPatternActiveSource{
    &kAutofillParsingPatternProvider, "prediction_source", "default"};

// Enables detection of language from Translate.
// TODO(crbug/1150895): Cleanup when launched.
BASE_FEATURE(kAutofillPageLanguageDetection,
             "AutofillPageLanguageDetection",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, AutofillManager::ParseForm() isn't called synchronously.
// Instead, all incoming events parse the form asynchronously and proceed
// afterwards.
// TODO(crbug.com/1309848) Remove once launched.
BASE_FEATURE(kAutofillParseAsync,
             "AutofillParseAsync",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, local heuristics fall back to interpreting the fields' name as an
// autocomplete type.
// TODO(crbug.com/1345879) Remove once launched.
BASE_FEATURE(kAutofillParseNameAsAutocompleteType,
             "AutofillParseNameAsAutocompleteType",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If the feature is enabled, FormTracker's probable-form-submission detection
// is disabled and replaced with browser-side detection.
// TODO(crbug/1117451): Remove once it works.
BASE_FEATURE(kAutofillProbableFormSubmissionInBrowser,
             "AutofillProbableFormSubmissionInBrowser",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If we observe a sequence of fields of (street address, address line 2), these
// get rationalized to (address line 1, address line 2).
// TODO(crbug.com/1326425): Remove once feature is lanuched.
BASE_FEATURE(kAutofillRationalizeStreetAddressAndAddressLine,
             "AutofillRationalizeStreetAddressAndAddressLine",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Removes setting-inaccessible field types from existing profiles on startup.
// TODO(crbug.com/1300548): Cleanup when launched.
BASE_FEATURE(kAutofillRemoveInaccessibleProfileValuesOnStartup,
             "AutofillRemoveInaccessibleProfileValuesOnStartup",
             base::FEATURE_DISABLED_BY_DEFAULT);

// If enabled, invalid phone numbers are removed on profile import, rather than
// invalidating the entire profile.
// TODO(crbug.com/1298424): Cleanup when launched.
BASE_FEATURE(kAutofillRemoveInvalidPhoneNumberOnImport,
             "AutofillRemoveInvalidPhoneNumberOnImport",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether or not overall prediction are retrieved from the cache.
BASE_FEATURE(kAutofillRetrieveOverallPredictionsFromCache,
             "AutofillRetrieveOverallPredictionsFromCache",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether UPI/VPA values will be saved and filled into payment forms.
BASE_FEATURE(kAutofillSaveAndFillVPA,
             "AutofillSaveAndFillVPA",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls non-default Autofill API predictions. See crbug.com/1331322.
BASE_FEATURE(kAutofillServerBehaviors,
             "AutofillServerBehaviors",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Chrome doesn't need to know the meaning of the value. Chrome only needs to
// forward it to the Autofill API, to let the server know which group the client
// belongs to.
const base::FeatureParam<int> kAutofillServerBehaviorsParam{
    &kAutofillServerBehaviors, "server_prediction_source", 0};

// Enables or Disables (mostly for hermetic testing) autofill server
// communication. The URL of the autofill server can further be controlled via
// the autofill-server-url param. The given URL should specify the complete
// autofill server API url up to the parent "directory" of the "query" and
// "upload" resources.
// i.e., https://other.autofill.server:port/tbproxy/af/
BASE_FEATURE(kAutofillServerCommunication,
             "AutofillServerCommunication",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether Autofill may fill across origins as part of the
// AutofillAcrossIframes experiment.
// TODO(crbug.com/1304721): Clean up when launched.
BASE_FEATURE(kAutofillSharedAutofill,
             "AutofillSharedAutofill",
             base::FEATURE_DISABLED_BY_DEFAULT);
// Relaxes the conditions under which a field is safe to fill.
// See FormForest::GetRendererFormsOfBrowserForm() for details.
const base::FeatureParam<bool> kAutofillSharedAutofillRelaxedParam{
    &kAutofillSharedAutofill, "relax_shared_autofill", false};

// Controls whether Manual fallbacks would be shown in the context menu for
// filling. Used only in Desktop.
// TODO(crbug.com/1326895): Clean up when launched.
BASE_FEATURE(kAutofillShowManualFallbackInContextMenu,
             "AutofillShowManualFallbackInContextMenu",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls attaching the autofill type predictions to their respective
// element in the DOM.
BASE_FEATURE(kAutofillShowTypePredictions,
             "AutofillShowTypePredictions",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Allows silent profile updates even when the profile import requirements are
// not met.
BASE_FEATURE(kAutofillSilentProfileUpdateForInsufficientImport,
             "AutofillSilentProfileUpdateForInsufficientImport",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether inferred label is considered for comparing in
// FormFieldData.SimilarFieldAs.
// TODO(crbug.com/1211834): The experiment seems dead; remove?
BASE_FEATURE(kAutofillSkipComparingInferredLabels,
             "AutofillSkipComparingInferredLabels",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether Autofill should search prefixes of all words/tokens when
// filtering profiles, or only on prefixes of the whole string.
BASE_FEATURE(kAutofillTokenPrefixMatching,
             "AutofillTokenPrefixMatching",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Autofill upload throttling is used for testing.
BASE_FEATURE(kAutofillUploadThrottling,
             "AutofillUploadThrottling",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to use the AutofillUseAlternativeStateNameMap for filling
// of state selection fields, comparison of profiles and sending state votes to
// the server.
// TODO(crbug.com/1143516): Remove the feature when the experiment is completed.
BASE_FEATURE(kAutofillUseAlternativeStateNameMap,
             "AutofillUseAlternativeStateNameMap",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether suggestions' labels use the improved label disambiguation
// format.
BASE_FEATURE(kAutofillUseImprovedLabelDisambiguation,
             "AutofillUseImprovedLabelDisambiguation",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use the same icon for the settings section in the popup
// footer.
BASE_FEATURE(kAutofillUseConsistentPopupSettingsIcons,
             "AutofillUseConsistentPopupSettingsIcons",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls whether to use the combined heuristic and the autocomplete section
// implementation for section splitting or not. See https://crbug.com/1076175.
BASE_FEATURE(kAutofillUseNewSectioningMethod,
             "AutofillUseNewSectioningMethod",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use the newest, parameterized sectioning algorithm.
// Use together with `kAutofillRefillByFormRendererId`.
// TODO(crbug.com/1153539): Remove the feature when the experiment is completed.
BASE_FEATURE(kAutofillUseParameterizedSectioning,
             "AutofillUseParameterizedSectioning",
             base::FEATURE_DISABLED_BY_DEFAULT);
// In the experiment, we test different combinations of these parameters.
const base::FeatureParam<bool> kAutofillSectioningModeIgnoreAutocomplete{
    &kAutofillUseParameterizedSectioning, "ignore_autocomplete", false};
const base::FeatureParam<bool> kAutofillSectioningModeCreateGaps{
    &kAutofillUseParameterizedSectioning, "create_gaps", false};
const base::FeatureParam<bool> kAutofillSectioningModeExpand{
    &kAutofillUseParameterizedSectioning, "expand_assigned_sections", false};

// Controls whether to use form renderer IDs to find the form which contains the
// field that was last interacted with in
// `AutofillAgent::TriggerRefillIfNeeded()`.
// TODO(crbug.com/1360988): Remove the feature when the experiment is completed.
BASE_FEATURE(kAutofillRefillByFormRendererId,
             "AutofillRefillByFormRendererId",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Introduces various visual improvements of the Autofill suggestion UI that is
// also used for the password manager.
BASE_FEATURE(kAutofillVisualImprovementsForSuggestionUi,
             "AutofillVisualImprovementsForSuggestionUi",
             base::FEATURE_ENABLED_BY_DEFAULT);

// Controls an ablation study in which autofill for addresses and payment data
// can be suppressed.
BASE_FEATURE(kAutofillEnableAblationStudy,
             "AutofillEnableAblationStudy",
             base::FEATURE_DISABLED_BY_DEFAULT);
// The following parameters are only effective if the study is enabled.
const base::FeatureParam<bool> kAutofillAblationStudyEnabledForAddressesParam{
    &kAutofillEnableAblationStudy, "enabled_for_addresses", false};
const base::FeatureParam<bool> kAutofillAblationStudyEnabledForPaymentsParam{
    &kAutofillEnableAblationStudy, "enabled_for_payments", false};
// The ratio of ablation_weight_per_mille / 1000 determines the chance of
// autofill being disabled on a given combination of site * day * browser
// session.
const base::FeatureParam<int> kAutofillAblationStudyAblationWeightPerMilleParam{
    &kAutofillEnableAblationStudy, "ablation_weight_per_mille", 10};

#if BUILDFLAG(IS_ANDROID)
// Controls whether the Autofill manual fallback for Addresses and Payments is
// present on Android.
BASE_FEATURE(kAutofillManualFallbackAndroid,
             "AutofillManualFallbackAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether to use modernized style for the Autofill dropdown.
BASE_FEATURE(kAutofillRefreshStyleAndroid,
             "AutofillRefreshStyleAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

// Controls whether the touch to fill surface is shown for credit cards on
// Android.
BASE_FEATURE(kAutofillTouchToFillForCreditCardsAndroid,
             "AutofillTouchToFillForCreditCardsAndroid",
             base::FEATURE_DISABLED_BY_DEFAULT);

#endif  // BUILDFLAG(IS_ANDROID)

#if BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)
BASE_FEATURE(kAutofillUseMobileLabelDisambiguation,
             "AutofillUseMobileLabelDisambiguation",
             base::FEATURE_DISABLED_BY_DEFAULT);
const char kAutofillUseMobileLabelDisambiguationParameterName[] = "variant";
const char kAutofillUseMobileLabelDisambiguationParameterShowAll[] = "show-all";
const char kAutofillUseMobileLabelDisambiguationParameterShowOne[] = "show-one";
#endif  // BUILDFLAG(IS_ANDROID) || BUILDFLAG(IS_IOS)

#if BUILDFLAG(IS_ANDROID)
bool IsAutofillManualFallbackEnabled() {
  return base::FeatureList::IsEnabled(kAutofillKeyboardAccessory) &&
         base::FeatureList::IsEnabled(kAutofillManualFallbackAndroid);
}
#endif  // BUILDFLAG(IS_ANDROID)

}  // namespace autofill::features
