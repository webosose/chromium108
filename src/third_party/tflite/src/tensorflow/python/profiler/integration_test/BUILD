load("//tensorflow:tensorflow.default.bzl", "cuda_py_test")

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],
)

py_library(
    name = "mnist_testing_utils",
    srcs = ["mnist_testing_utils.py"],
    srcs_version = "PY3",
    deps = [
        "//tensorflow:tensorflow_py",
        "//tensorflow/python:extra_py_tests_deps",
    ],
)

cuda_py_test(
    name = "profiler_api_test",
    srcs = ["profiler_api_test.py"],
    python_version = "PY3",
    tags = [
        "no_pip",
        "no_windows",  # TODO(b/192257727)
    ],
    deps = [
        ":mnist_testing_utils",
        "//tensorflow/python:constant_op",
        "//tensorflow/python:errors",
        "//tensorflow/python:framework_test_lib",
        "//tensorflow/python:platform",
        "//tensorflow/python/data/ops:dataset_ops",
        "//tensorflow/python/distribute:collective_all_reduce_strategy",
        "//tensorflow/python/distribute:multi_process_runner",
        "//tensorflow/python/distribute:multi_worker_test_base",
        "//tensorflow/python/eager:test",
        "//tensorflow/python/profiler:profiler_client",
        "//tensorflow/python/profiler:profiler_v2",
    ],
)
