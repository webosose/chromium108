// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/callback.h"
#include "base/path_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/history/history_service_factory.h"
#include "chrome/browser/optimization_guide/browser_test_util.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service.h"
#include "chrome/browser/optimization_guide/optimization_guide_keyed_service_factory.h"
#include "chrome/browser/optimization_guide/page_content_annotations_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/common/chrome_switches.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/history/core/browser/history_database.h"
#include "components/history/core/browser/history_db_task.h"
#include "components/history/core/browser/history_service.h"
#include "components/optimization_guide/content/browser/page_content_annotations_service.h"
#include "components/optimization_guide/content/browser/test_page_content_annotator.h"
#include "components/optimization_guide/core/execution_status.h"
#include "components/optimization_guide/core/optimization_guide_enums.h"
#include "components/optimization_guide/core/optimization_guide_features.h"
#include "components/optimization_guide/core/optimization_guide_switches.h"
#include "components/optimization_guide/core/optimization_guide_test_util.h"
#include "components/optimization_guide/core/test_model_info_builder.h"
#include "components/optimization_guide/machine_learning_tflite_buildflags.h"
#include "components/optimization_guide/proto/page_entities_metadata.pb.h"
#include "components/optimization_guide/proto/page_topics_model_metadata.pb.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "net/dns/mock_host_resolver.h"
#include "services/metrics/public/cpp/ukm_builders.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "services/metrics/public/mojom/ukm_interface.mojom-forward.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

#if BUILDFLAG(IS_CHROMEOS_ASH)
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/login/test/scoped_policy_update.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#endif

namespace optimization_guide {

namespace {

using ::testing::UnorderedElementsAre;

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
// Different platforms may execute float models slightly differently, and this
// results in a pretty big difference in the scores. This matcher offers up to
// 0.1 of absolute difference in score, but the topic itself must match.
// See crbug.com/1307251.
testing::Matcher<WeightedIdentifier> CrossPlatformMatcher(
    const WeightedIdentifier& wi) {
  return testing::AllOf(
      testing::Property(&WeightedIdentifier::value, wi.value()),
      testing::Property(
          &WeightedIdentifier::weight,
          testing::DoubleNear(wi.weight(), /*max_abs_error=*/0.1)));
}
#endif

}  // namespace

// A HistoryDBTask that retrieves content annotations.
class GetContentAnnotationsTask : public history::HistoryDBTask {
 public:
  GetContentAnnotationsTask(
      const GURL& url,
      base::OnceCallback<void(
          const absl::optional<history::VisitContentAnnotations>&)> callback)
      : url_(url), callback_(std::move(callback)) {}
  ~GetContentAnnotationsTask() override = default;

  // history::HistoryDBTask:
  bool RunOnDBThread(history::HistoryBackend* backend,
                     history::HistoryDatabase* db) override {
    // Get visits for URL.
    const history::URLID url_id = db->GetRowForURL(url_, nullptr);
    history::VisitVector visits;
    if (!db->GetVisitsForURL(url_id, &visits))
      return true;

    // No visits for URL.
    if (visits.empty())
      return true;

    history::VisitContentAnnotations annotations;
    if (db->GetContentAnnotationsForVisit(visits.at(0).visit_id, &annotations))
      stored_content_annotations_ = annotations;

    return true;
  }
  void DoneRunOnMainThread() override {
    std::move(callback_).Run(stored_content_annotations_);
  }

 private:
  // The URL to get content annotations for.
  const GURL url_;
  // The callback to invoke when the database call has completed.
  base::OnceCallback<void(
      const absl::optional<history::VisitContentAnnotations>&)>
      callback_;
  // The content annotations that were stored for |url_|.
  absl::optional<history::VisitContentAnnotations> stored_content_annotations_;
};

class PageContentAnnotationsServiceDisabledBrowserTest
    : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServiceDisabledBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{},
        {features::kOptimizationHints, features::kPageContentAnnotations});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceDisabledBrowserTest,
                       KeyedServiceEnabledButFeaturesDisabled) {
  EXPECT_EQ(nullptr, PageContentAnnotationsServiceFactory::GetForProfile(
                         browser()->profile()));
}

class PageContentAnnotationsServiceKioskModeBrowserTest
    : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServiceKioskModeBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOptimizationHints, features::kPageContentAnnotations},
        /*disabled_features=*/{});
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    command_line->AppendSwitch(::switches::kKioskMode);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceKioskModeBrowserTest,
                       DisabledInKioskMode) {
  EXPECT_EQ(nullptr, PageContentAnnotationsServiceFactory::GetForProfile(
                         browser()->profile()));
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
class PageContentAnnotationsServiceEphemeralProfileBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  PageContentAnnotationsServiceEphemeralProfileBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOptimizationHints, features::kPageContentAnnotations},
        /*disabled_features=*/{});
  }

  void SetUpInProcessBrowserTestFixture() override {
    MixinBasedInProcessBrowserTest::SetUpInProcessBrowserTestFixture();

    std::unique_ptr<ash::ScopedDevicePolicyUpdate> device_policy_update =
        device_state_.RequestDevicePolicyUpdate();
    device_policy_update->policy_payload()
        ->mutable_ephemeral_users_enabled()
        ->set_ephemeral_users_enabled(true);
  }

 protected:
  ash::DeviceStateMixin device_state_{
      &mixin_host_,
      ash::DeviceStateMixin::State::OOBE_COMPLETED_CLOUD_ENROLLED};

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceEphemeralProfileBrowserTest,
                       EphemeralProfileDoesNotInstantiateService) {
  EXPECT_EQ(nullptr, PageContentAnnotationsServiceFactory::GetForProfile(
                         browser()->profile()));
}
#endif

class PageContentAnnotationsServiceValidationBrowserTest
    : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServiceValidationBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOptimizationHints,
         features::kPageContentAnnotationsValidation},
        {features::kPageContentAnnotations});
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceValidationBrowserTest,
                       ValidationEnablesService) {
  EXPECT_NE(nullptr, PageContentAnnotationsServiceFactory::GetForProfile(
                         browser()->profile()));
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
class PageContentAnnotationsServicePageTopicsBrowserTest
    : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServicePageTopicsBrowserTest() {
    scoped_feature_list_.InitWithFeatures(
        {features::kOptimizationHints, features::kPageContentAnnotations},
        {features::kPreventLongRunningPredictionModels});
  }
  ~PageContentAnnotationsServicePageTopicsBrowserTest() override = default;

  void LoadPageTopicsV2Model() {
    proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/com.foo.PageTopicsModelMetadata");
    proto::PageTopicsModelMetadata page_topics_model_metadata;
    page_topics_model_metadata.set_version(123);
    page_topics_model_metadata.add_supported_output(
        proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES);
    auto* output_params =
        page_topics_model_metadata.mutable_output_postprocessing_params();
    auto* category_params = output_params->mutable_category_params();
    category_params->set_max_categories(5);
    category_params->set_min_none_weight(0.8);
    category_params->set_min_category_weight(0.1);
    category_params->set_min_normalized_weight_within_top_n(0.1);
    page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("optimization_guide")
            .AppendASCII("page_topics_128_model.tflite");

    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OverrideTargetModelForTesting(
            proto::OPTIMIZATION_TARGET_PAGE_TOPICS_V2,
            optimization_guide::TestModelInfoBuilder()
                .SetModelFilePath(model_file_path)
                .SetModelMetadata(any_metadata)
                .Build());

    PageContentAnnotationsService* service =
        PageContentAnnotationsServiceFactory::GetForProfile(
            browser()->profile());
    ASSERT_TRUE(service);

    base::RunLoop run_loop;
    service->RequestAndNotifyWhenModelAvailable(
        AnnotationType::kPageTopics,
        base::BindOnce(
            [](base::RunLoop* run_loop, bool success) {
              EXPECT_TRUE(success);
              run_loop->Quit();
            },
            &run_loop));
    run_loop.Run();
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServicePageTopicsBrowserTest,
                       E2EWithGoldenTestData) {
  PageContentAnnotationsService* service =
      PageContentAnnotationsServiceFactory::GetForProfile(browser()->profile());
  ASSERT_TRUE(service);
  LoadPageTopicsV2Model();

  std::vector<BatchAnnotationResult> results;
  base::RunLoop run_loop;
  service->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             std::vector<BatchAnnotationResult>* out_results,
             const std::vector<BatchAnnotationResult>& results) {
            *out_results = results;
            run_loop->Quit();
          },
          &run_loop, &results),
      std::vector<std::string>{
          "youtube.com",
          "chrome.com",
          "music.youtube.com",
      },
      AnnotationType::kPageTopics);
  run_loop.Run();

  ASSERT_EQ(results.size(), 3U);

  EXPECT_EQ(results[0].input(), "youtube.com");
  EXPECT_EQ(results[0].type(), AnnotationType::kPageTopics);
  ASSERT_TRUE(results[0].topics());
  EXPECT_THAT(*results[0].topics(),
              testing::ElementsAreArray({
                  CrossPlatformMatcher(WeightedIdentifier(250, 0.601997)),
                  CrossPlatformMatcher(WeightedIdentifier(43, 0.915914)),
              }));

  EXPECT_EQ(results[1].input(), "chrome.com");
  EXPECT_EQ(results[1].type(), AnnotationType::kPageTopics);
  ASSERT_TRUE(results[1].topics());
  EXPECT_THAT(*results[1].topics(),
              testing::ElementsAreArray({
                  CrossPlatformMatcher(WeightedIdentifier(223, 0.209933)),
                  CrossPlatformMatcher(WeightedIdentifier(43, 0.474946)),
                  CrossPlatformMatcher(WeightedIdentifier(148, 0.881723)),
              }));

  EXPECT_EQ(results[2].input(), "music.youtube.com");
  EXPECT_EQ(results[2].type(), AnnotationType::kPageTopics);
  ASSERT_TRUE(results[2].topics());
  EXPECT_THAT(*results[2].topics(),
              testing::ElementsAreArray({
                  CrossPlatformMatcher(WeightedIdentifier(250, 0.450154)),
                  CrossPlatformMatcher(WeightedIdentifier(1, 0.518014)),
                  CrossPlatformMatcher(WeightedIdentifier(43, 0.596481)),
                  CrossPlatformMatcher(WeightedIdentifier(23, 0.827426)),
              }));
}
#endif

class PageContentAnnotationsServiceBrowserTest : public InProcessBrowserTest {
 public:
  PageContentAnnotationsServiceBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationHints, {}},
         {features::kPageContentAnnotations,
          {
              {"write_to_history_service", "true"},
          }},
         {features::kPageVisibilityPageContentAnnotations, {}}},
        /*disabled_features=*/{features::kPreventLongRunningPredictionModels});
  }
  ~PageContentAnnotationsServiceBrowserTest() override = default;

  void set_load_model_on_startup(bool load_model_on_startup) {
    load_model_on_startup_ = load_model_on_startup;
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    InProcessBrowserTest::SetUpOnMainThread();

    embedded_test_server()->ServeFilesFromSourceDirectory(
        "chrome/test/data/optimization_guide");
    ASSERT_TRUE(embedded_test_server()->Start());

    if (load_model_on_startup_) {
      LoadAndWaitForModel();
    }
  }

  void LoadAndWaitForModel() {
    proto::Any any_metadata;
    any_metadata.set_type_url(
        "type.googleapis.com/com.foo.PageTopicsModelMetadata");
    proto::PageTopicsModelMetadata page_topics_model_metadata;
    page_topics_model_metadata.set_version(123);
    page_topics_model_metadata.add_supported_output(
        proto::PAGE_TOPICS_SUPPORTED_OUTPUT_CATEGORIES);
    auto* output_params =
        page_topics_model_metadata.mutable_output_postprocessing_params();
    auto* category_params = output_params->mutable_category_params();
    category_params->set_max_categories(5);
    category_params->set_min_none_weight(0.8);
    category_params->set_min_category_weight(0.0);
    category_params->set_min_normalized_weight_within_top_n(0.1);
    // TODO(crbug.com/1200677): migrate the category name on the test model
    // itself provided by model owners.
    output_params->mutable_visibility_params()->set_category_name(
        "FLOC_PROTECTED");
    page_topics_model_metadata.SerializeToString(any_metadata.mutable_value());
    base::FilePath source_root_dir;
    base::PathService::Get(base::DIR_SOURCE_ROOT, &source_root_dir);
    base::FilePath model_file_path =
        source_root_dir.AppendASCII("components")
            .AppendASCII("test")
            .AppendASCII("data")
            .AppendASCII("optimization_guide")
            .AppendASCII("bert_page_topics_model.tflite");

    base::HistogramTester histogram_tester;

    OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
        ->OverrideTargetModelForTesting(
            proto::OPTIMIZATION_TARGET_PAGE_VISIBILITY,
            optimization_guide::TestModelInfoBuilder()
                .SetModelFilePath(model_file_path)
                .SetModelMetadata(any_metadata)
                .Build());

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.ModelExecutor.ModelFileUpdated.PageVisibility", 1);
#else
    base::RunLoop().RunUntilIdle();
#endif
  }

  absl::optional<history::VisitContentAnnotations> GetContentAnnotationsForURL(
      const GURL& url) {
    history::HistoryService* history_service =
        HistoryServiceFactory::GetForProfile(
            browser()->profile(), ServiceAccessType::IMPLICIT_ACCESS);
    if (!history_service)
      return absl::nullopt;

    std::unique_ptr<base::RunLoop> run_loop = std::make_unique<base::RunLoop>();
    absl::optional<history::VisitContentAnnotations> got_content_annotations;

    base::CancelableTaskTracker task_tracker;
    history_service->ScheduleDBTask(
        FROM_HERE,
        std::make_unique<GetContentAnnotationsTask>(
            url, base::BindOnce(
                     [](base::RunLoop* run_loop,
                        absl::optional<history::VisitContentAnnotations>*
                            out_content_annotations,
                        const absl::optional<history::VisitContentAnnotations>&
                            content_annotations) {
                       *out_content_annotations = content_annotations;
                       run_loop->Quit();
                     },
                     run_loop.get(), &got_content_annotations)),
        &task_tracker);

    run_loop->Run();
    return got_content_annotations;
  }

  bool ModelAnnotationsFieldsAreSetForURL(const GURL& url) {
    absl::optional<history::VisitContentAnnotations> got_content_annotations =
        GetContentAnnotationsForURL(url);
    // No content annotations -> no model annotations fields.
    if (!got_content_annotations)
      return false;

    const history::VisitContentModelAnnotations& model_annotations =
        got_content_annotations->model_annotations;

    // Return true if any of the fields have non-empty/non-default values.
    return (model_annotations.visibility_score !=
            history::VisitContentModelAnnotations::kDefaultVisibilityScore) ||
           !model_annotations.categories.empty() ||
           !model_annotations.entities.empty();
  }

  void Annotate(const HistoryVisit& visit) {
    PageContentAnnotationsService* service =
        PageContentAnnotationsServiceFactory::GetForProfile(
            browser()->profile());
    service->Annotate(visit);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  bool load_model_on_startup_ = true;
};

// Disabled. https://crbug.com/1338408
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       DISABLED_ModelExecutes) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  int expected_count = 1;
#else
  int expected_count = 0;
#endif
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
      expected_count);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
      expected_count);
#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", true,
      1);
#else
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", false,
      1);
#endif

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      PageContentAnnotationsStorageStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus.ModelAnnotations",
      PageContentAnnotationsStorageStatus::kSuccess, 1);

  absl::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_NE(-1.0, got_content_annotations->model_annotations.visibility_score);
  EXPECT_TRUE(got_content_annotations->model_annotations.categories.empty());

#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)
}

#if BUILDFLAG(BUILD_WITH_TFLITE_LIB)
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       PageTopicsDomainPreProcessing) {
  PageContentAnnotationsService* service =
      PageContentAnnotationsServiceFactory::GetForProfile(browser()->profile());

  base::RunLoop run_loop;
  service->BatchAnnotate(
      base::BindOnce(
          [](base::RunLoop* run_loop,
             const std::vector<BatchAnnotationResult>& results) {
            ASSERT_EQ(results.size(), 1U);
            EXPECT_EQ(results[0].input(), "www.chromium.org");
            EXPECT_EQ(results[0].type(), AnnotationType::kPageTopics);
            // Intentionally does not test the output of model inference, since
            // that is well covered in the unittests for
            // PageContentAnnotationsModelManager.
            run_loop->Quit();
          },
          &run_loop),
      std::vector<std::string>{"www.chromium.org"},
      AnnotationType::kPageTopics);

  run_loop.Run();
}

#if !defined(NDEBUG)
// Flaky timeout in debug builds (crbug.com/1338408).
#define MAYBE_NoVisitsForUrlInHistory DISABLED_NoVisitsForUrlInHistory
#else
#define MAYBE_NoVisitsForUrlInHistory NoVisitsForUrlInHistory
#endif
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       MAYBE_NoVisitsForUrlInHistory) {
  HistoryVisit history_visit;
  history_visit.url = GURL("https://probablynotarealurl.com/");
  history_visit.text_to_annotate = "sometext";

  {
    base::HistogramTester histogram_tester;

    Annotate(history_visit);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
        true, 1);

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PageContentAnnotationsService."
        "ContentAnnotationsStorageStatus",
        1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotationsService."
        "ContentAnnotationsStorageStatus",
        PageContentAnnotationsStorageStatus::kNoVisitsForUrl, 1);
    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotationsService."
        "ContentAnnotationsStorageStatus.ModelAnnotations",
        PageContentAnnotationsStorageStatus::kNoVisitsForUrl, 1);

    EXPECT_FALSE(GetContentAnnotationsForURL(history_visit.url).has_value());
  }

  {
    base::HistogramTester histogram_tester;

    // Make sure a repeat visit is not annotated again.
    Annotate(history_visit);

    base::RunLoop().RunUntilIdle();

    histogram_tester.ExpectTotalCount(
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 0);
  }
}

// Flaky timeout in debug builds (crbug.com/1338408).
// TODO(crbug.com/1365619): Flaky on several OSes in non-debug.
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       DISABLED_OgImagePresent) {
  base::HistogramTester histogram_tester;
  ukm::TestAutoSetUkmRecorder ukm_recorder;

  GURL url(embedded_test_server()->GetURL("a.com", "/og_image.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  // Value taken from SalientImageAvailability enum.
  static const int kAvailableFromOgImage = 3;

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability", 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability",
      kAvailableFromOgImage, 1);

  std::vector<const ukm::mojom::UkmEntry*> entries =
      ukm_recorder.GetEntriesByName(
          ukm::builders::SalientImageAvailability::kEntryName);
  ASSERT_EQ(1u, entries.size());

  ASSERT_EQ(1u, entries[0]->metrics.size());
  EXPECT_EQ(kAvailableFromOgImage, entries[0]->metrics.begin()->second);
}

// Flaky timeout in debug builds (crbug.com/1338408).
// TODO(crbug.com/1365619): Flaky on several OSes in non-debug.
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       DISABLED_OgImagePresentButMalformed) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/og_image_malformed.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability", 1);

  // Malformed URL is also reported as og image unavailable.
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability", 1,
      1);
}

#if !defined(NDEBUG)
// Flaky timeout in debug builds (crbug.com/1338408).
#define MAYBE_OgImageNotPresent DISABLED_OgImageNotPresent
#else
#define MAYBE_OgImageNotPresent OgImageNotPresent
#endif
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBrowserTest,
                       MAYBE_OgImageNotPresent) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/no_og_image.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability", 1);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.PageContentAnnotations.SalientImageAvailability", 1,
      1);
}

class PageContentAnnotationsServiceRemoteMetadataBrowserTest
    : public PageContentAnnotationsServiceBrowserTest {
 public:
  PageContentAnnotationsServiceRemoteMetadataBrowserTest() {
    // Make sure remote page metadata works without page content annotations
    // enabled.
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationHints, {}},
         {features::kRemotePageMetadata, {{"min_page_category_score", "80"}}}},
        /*disabled_features=*/{{features::kPageContentAnnotations}});
    set_load_model_on_startup(false);
  }
  ~PageContentAnnotationsServiceRemoteMetadataBrowserTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceRemoteMetadataBrowserTest,
                       StoresAllTheThingsFromRemoteService) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));

  proto::PageEntitiesMetadata page_entities_metadata;
  proto::Entity* entity = page_entities_metadata.add_entities();
  entity->set_entity_id("entity1");
  entity->set_score(50);
  proto::Category* category = page_entities_metadata.add_categories();
  category->set_category_id("category1");
  category->set_score(0.85);
  proto::Category* category2 = page_entities_metadata.add_categories();
  category2->set_category_id("othercategory");
  category2->set_score(0.75);
  page_entities_metadata.set_alternative_title("alternative title");
  OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(page_entities_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, proto::PAGE_ENTITIES, metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      2);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      PageContentAnnotationsStorageStatus::kSuccess, 2);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus.ModelAnnotations",
      PageContentAnnotationsStorageStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus.RemoteMetadata",
      PageContentAnnotationsStorageStatus::kSuccess, 1);

  absl::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_THAT(
      got_content_annotations->model_annotations.entities,
      UnorderedElementsAre(
          history::VisitContentModelAnnotations::Category("entity1", 50)));
  EXPECT_THAT(
      got_content_annotations->model_annotations.categories,
      UnorderedElementsAre(
          history::VisitContentModelAnnotations::Category("category1", 85)));
  EXPECT_EQ(got_content_annotations->alternative_title, "alternative title");
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceRemoteMetadataBrowserTest,
                       StoresPageEntitiesAndCategoriesFromRemoteService) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));

  proto::PageEntitiesMetadata page_entities_metadata;
  proto::Entity* entity = page_entities_metadata.add_entities();
  entity->set_entity_id("entity1");
  entity->set_score(50);
  proto::Category* category = page_entities_metadata.add_categories();
  category->set_category_id("category1");
  category->set_score(0.85);
  proto::Category* category2 = page_entities_metadata.add_categories();
  category2->set_category_id("othercategory");
  category2->set_score(0.75);
  OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(page_entities_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, proto::PAGE_ENTITIES, metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      PageContentAnnotationsStorageStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus.ModelAnnotations",
      PageContentAnnotationsStorageStatus::kSuccess, 1);

  absl::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_THAT(
      got_content_annotations->model_annotations.entities,
      UnorderedElementsAre(
          history::VisitContentModelAnnotations::Category("entity1", 50)));
  EXPECT_THAT(
      got_content_annotations->model_annotations.categories,
      UnorderedElementsAre(
          history::VisitContentModelAnnotations::Category("category1", 85)));
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceRemoteMetadataBrowserTest,
                       StoresAlternateTitleFromRemoteService) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));

  proto::PageEntitiesMetadata page_entities_metadata;
  page_entities_metadata.set_alternative_title("alternative title");
  OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(page_entities_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, proto::PAGE_ENTITIES, metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      PageContentAnnotationsStorageStatus::kSuccess, 1);
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus.RemoteMetadata",
      PageContentAnnotationsStorageStatus::kSuccess, 1);

  absl::optional<history::VisitContentAnnotations> got_content_annotations =
      GetContentAnnotationsForURL(url);
  ASSERT_TRUE(got_content_annotations.has_value());
  EXPECT_EQ(got_content_annotations->alternative_title, "alternative title");
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceRemoteMetadataBrowserTest,
                       EmptyMetadataNotStored) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));

  proto::PageEntitiesMetadata page_entities_metadata;
  OptimizationMetadata metadata;
  metadata.SetAnyMetadataForTesting(page_entities_metadata);
  OptimizationGuideKeyedServiceFactory::GetForProfile(browser()->profile())
      ->AddHintForTesting(url, proto::PAGE_ENTITIES, metadata);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));
  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus.RemoteMetadata",
      0);
}

class PageContentAnnotationsServiceNoHistoryTest
    : public PageContentAnnotationsServiceBrowserTest {
 public:
  PageContentAnnotationsServiceNoHistoryTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationHints, {}},
         {features::kPageContentAnnotations,
          {
              {"write_to_history_service", "false"},
          }},
         {features::kPageVisibilityPageContentAnnotations, {}}},
        /*disabled_features=*/{});
  }
  ~PageContentAnnotationsServiceNoHistoryTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky on Linux Tests (dbg): crbug.com/1338040
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_ModelExecutesButDoesntWriteToHistory \
  DISABLED_ModelExecutesButDoesntWriteToHistory
#else
#define MAYBE_ModelExecutesButDoesntWriteToHistory \
  ModelExecutesButDoesntWriteToHistory
#endif
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceNoHistoryTest,
                       MAYBE_ModelExecutesButDoesntWriteToHistory) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", true,
      1);

  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      0);

  // The ContentAnnotations should either not exist at all, or if they do
  // (because some other code added some annotations), the model-related fields
  // should be empty/unset.
  EXPECT_FALSE(ModelAnnotationsFieldsAreSetForURL(url));
}

// Flaky on Linux Tests (dbg): crbug.com/1338408
#if BUILDFLAG(IS_LINUX) && !defined(NDEBUG)
#define MAYBE_ModelExecutesAndUsesCachedResult \
  DISABLED_ModelExecutesAndUsesCachedResult
#else
#define MAYBE_ModelExecutesAndUsesCachedResult ModelExecutesAndUsesCachedResult
#endif
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceNoHistoryTest,
                       MAYBE_ModelExecutesAndUsesCachedResult) {
  {
    base::HistogramTester histogram_tester;

    GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
        false, 1);
  }
  {
    base::HistogramTester histogram_tester;
    GURL url2(embedded_test_server()->GetURL("a.com", "/hello.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

    RetryForHistogramUntilCountReached(
        &histogram_tester,
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 1);

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated",
        true, 1);

    base::RunLoop().RunUntilIdle();

    histogram_tester.ExpectUniqueSample(
        "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
        true, 1);
  }
}

class PageContentAnnotationsServiceBatchVisitTest
    : public PageContentAnnotationsServiceNoHistoryTest {
 public:
  PageContentAnnotationsServiceBatchVisitTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationHints, {}},
         {features::kPageContentAnnotations,
          {{"write_to_history_service", "false"},
           {"annotate_visit_batch_size", "2"}}},
         {features::kPageVisibilityPageContentAnnotations, {}}},
        /*disabled_features=*/{});
  }
  ~PageContentAnnotationsServiceBatchVisitTest() override = default;

  void SetUpOnMainThread() override {
    PageContentAnnotationsServiceNoHistoryTest::SetUpOnMainThread();

    PageContentAnnotationsService* service =
        PageContentAnnotationsServiceFactory::GetForProfile(
            browser()->profile());

    annotator_.UsePageEntities(
        /*model_info=*/absl::nullopt,
        {
            {
                "Test Page",
                {
                    ScoredEntityMetadata(0.6,
                                         EntityMetadata("test", "test", {})),
                    ScoredEntityMetadata(0.4,
                                         EntityMetadata("page", "page", {})),
                },
            },
            {
                "sometext",
                {
                    ScoredEntityMetadata(0.7,
                                         EntityMetadata("some", "some", {})),
                    ScoredEntityMetadata(0.3,
                                         EntityMetadata("text", "text", {})),
                },
            },
        });
    service->OverridePageContentAnnotatorForTesting(&annotator_);
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
  TestPageContentAnnotator annotator_;
};

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBatchVisitTest,
                       ModelExecutesWithFullBatch) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "PageContentAnnotations.AnnotateVisit.AnnotationRequested", 1);

  GURL url2(embedded_test_server()->GetURL("b.com", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", 2);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotationsService.ContentAnnotated", true,
      2);

  base::RunLoop().RunUntilIdle();

  // The cache is missed because we are batching requests. The cache check
  // happens before adding a visit annotation request to the batch.
  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
      false, 2);
  histogram_tester.ExpectUniqueSample(
      "PageContentAnnotations.AnnotateVisit.BatchAnnotationStarted", true, 1);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      0);

  // The ContentAnnotations should either not exist at all, or if they do
  // (because some other code added some annotations), the model-related fields
  // should be empty/unset.
  EXPECT_FALSE(ModelAnnotationsFieldsAreSetForURL(url));
}

class PageContentAnnotationsServiceBatchVisitNoAnnotateTest
    : public PageContentAnnotationsServiceBatchVisitTest {
 public:
  PageContentAnnotationsServiceBatchVisitNoAnnotateTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        {{features::kOptimizationHints, {}},
         {features::kPageContentAnnotations,
          {{"write_to_history_service", "false"},
           {"annotate_visit_batch_size", "2"}}},
         {features::kPageVisibilityPageContentAnnotations, {}}},
        /*disabled_features=*/{});
  }
  ~PageContentAnnotationsServiceBatchVisitNoAnnotateTest() override = default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(https://crbug/1291486): Re-enable once flakiness is fixed.
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBatchVisitNoAnnotateTest,
                       DISABLED_QueueFullAndVisitBatchActive) {
  base::HistogramTester histogram_tester;
  HistoryVisit history_visit(base::Time::Now(),
                             GURL("https://probablynotarealurl.com/"), 0);
  HistoryVisit history_visit2(base::Time::Now(),
                              GURL("https://probablynotarealurl.com/"), 0);
  HistoryVisit history_visit3(base::Time::Now(),
                              GURL("https://probablynotarealurl.com/"), 0);
  HistoryVisit history_visit4(base::Time::Now(),
                              GURL("https://probablynotarealurl.com/"), 0);
  history_visit.text_to_annotate = "sometext";
  history_visit2.text_to_annotate = "sometext";
  history_visit3.text_to_annotate = "sometext";
  history_visit4.text_to_annotate = "sometext";

  Annotate(history_visit);
  Annotate(history_visit2);
  Annotate(history_visit3);
  Annotate(history_visit4);

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "PageContentAnnotations.AnnotateVisit.QueueFullVisitDropped", 1);

  histogram_tester.ExpectUniqueSample(
      "PageContentAnnotations.AnnotateVisit.BatchAnnotationStarted", true, 1);
  histogram_tester.ExpectUniqueSample(
      "PageContentAnnotations.AnnotateVisit.QueueFullVisitDropped", true, 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.PageContentAnnotations.AnnotateVisitResultCached",
      false, 4);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      0);
}

IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceBatchVisitTest,
                       NoModelExecutionWithoutFullBatch) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "PageContentAnnotations.AnnotateVisit.AnnotationRequestQueued", 1);

  base::RunLoop().RunUntilIdle();

  histogram_tester.ExpectTotalCount(
      "PageContentAnnotations.AnnotateVisit.BatchAnnotationStarted", 0);

  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.PageContentAnnotationsService."
      "ContentAnnotationsStorageStatus",
      0);

  // The ContentAnnotations should either not exist at all, or if they do
  // (because some other code added some annotations), the model-related fields
  // should be empty/unset.
  EXPECT_FALSE(ModelAnnotationsFieldsAreSetForURL(url));
}

class PageContentAnnotationsServiceModelNotLoadedOnStartupTest
    : public PageContentAnnotationsServiceBrowserTest {
 public:
  PageContentAnnotationsServiceModelNotLoadedOnStartupTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kOptimizationHints,
                              features::kPageContentAnnotations,
                              features::kPageVisibilityPageContentAnnotations},
        /*disabled_features=*/{});
    set_load_model_on_startup(false);
  }
  ~PageContentAnnotationsServiceModelNotLoadedOnStartupTest() override =
      default;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Flaky on Win 7 Tests x64: crbug.com/1223172
// Flaky timeout in debug builds: crbug.com/1338408.
#if BUILDFLAG(IS_WIN) || !defined(NDEBUG)
#define MAYBE_ModelNotAvailableForFirstNavigation \
  DISABLED_ModelNotAvailableForFirstNavigation
#else
#define MAYBE_ModelNotAvailableForFirstNavigation \
  ModelNotAvailableForFirstNavigation
#endif
IN_PROC_BROWSER_TEST_F(PageContentAnnotationsServiceModelNotLoadedOnStartupTest,
                       MAYBE_ModelNotAvailableForFirstNavigation) {
  base::HistogramTester histogram_tester;

  GURL url(embedded_test_server()->GetURL("a.com", "/hello.html"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PageVisibility", 1);

  histogram_tester.ExpectUniqueSample(
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PageVisibility",
      ExecutionStatus::kErrorModelFileNotAvailable, 1);

  LoadAndWaitForModel();

  GURL url2(
      embedded_test_server()->GetURL("a.com", "/hello.html?totally=different"));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url2));

  RetryForHistogramUntilCountReached(
      &histogram_tester,
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PageVisibility", 2);

  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PageVisibility",
      ExecutionStatus::kErrorModelFileNotAvailable, 1);
  histogram_tester.ExpectBucketCount(
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PageVisibility",
      ExecutionStatus::kSuccess, 1);
  histogram_tester.ExpectTotalCount(
      "OptimizationGuide.ModelExecutor.ExecutionStatus.PageVisibility", 2);
}

#endif  // BUILDFLAG(BUILD_WITH_TFLITE_LIB)

}  // namespace optimization_guide
