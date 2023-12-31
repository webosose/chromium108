# Tests of tf.io.*proto.

load("//tensorflow:tensorflow.bzl", "tf_cc_shared_object")
load("//tensorflow:tensorflow.default.bzl", "tf_py_test")
load("//tensorflow/core/platform:build_config_root.bzl", "if_static")
load("//tensorflow/core/platform:build_config.bzl", "tf_additional_all_protos", "tf_proto_library")

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],
)

exports_files([
    "test_example.proto",
])

tf_py_test(
    name = "decode_proto_op_test",
    size = "small",
    srcs = ["decode_proto_op_test.py"],
    data = if_static(
        [],
        otherwise = [":libtestexample.so"],
    ),
    python_version = "PY3",
    tags = [
        "no_pip",  # TODO(b/78026780)
        "no_windows",  # TODO(b/78028010)
    ],
    deps = [
        ":decode_proto_op_test_base",
        ":py_test_deps",
        "//tensorflow/python:proto_ops",
    ],
)

tf_py_test(
    name = "encode_proto_op_test",
    size = "small",
    srcs = ["encode_proto_op_test.py"],
    data = if_static(
        [],
        otherwise = [":libtestexample.so"],
    ),
    python_version = "PY3",
    tags = [
        "no_pip",  # TODO(b/78026780)
        "no_windows",  # TODO(b/78028010)
    ],
    deps = [
        ":encode_proto_op_test_base",
        ":py_test_deps",
        "//tensorflow/python:proto_ops",
    ],
)

py_library(
    name = "proto_op_test_base",
    testonly = 1,
    srcs = ["proto_op_test_base.py"],
    srcs_version = "PY3",
    deps = [
        ":test_example_proto_py",
        "//tensorflow/python:client_testlib",
    ],
)

py_library(
    name = "decode_proto_op_test_base",
    testonly = 1,
    srcs = ["decode_proto_op_test_base.py"],
    srcs_version = "PY3",
    deps = [
        ":proto_op_test_base",
        ":test_example_proto_py",
        "//third_party/py/numpy",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_library(
    name = "encode_proto_op_test_base",
    testonly = 1,
    srcs = ["encode_proto_op_test_base.py"],
    srcs_version = "PY3",
    deps = [
        ":proto_op_test_base",
        ":test_example_proto_py",
        "//third_party/py/numpy",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_library(
    name = "py_test_deps",
    srcs_version = "PY3",
)

tf_proto_library(
    name = "test_example_proto",
    srcs = ["test_example.proto"],
    cc_api_version = 2,
    protodeps = tf_additional_all_protos(),
)

tf_cc_shared_object(
    name = "libtestexample.so",
    linkstatic = 1,
    deps = [
        ":test_example_proto_cc",
    ],
)

py_library(
    name = "descriptor_source_test_base",
    testonly = 1,
    srcs = ["descriptor_source_test_base.py"],
    srcs_version = "PY3",
    deps = [
        ":proto_op_test_base",
        "//third_party/py/numpy",
        "@absl_py//absl/testing:parameterized",
        "@com_google_protobuf//:protobuf_python",
    ],
)

tf_py_test(
    name = "descriptor_source_test",
    size = "small",
    srcs = ["descriptor_source_test.py"],
    python_version = "PY3",
    tags = [
        "no_pip",
    ],
    deps = [
        ":descriptor_source_test_base",
        "//tensorflow/python:client_testlib",
        "//tensorflow/python:proto_ops",
    ],
)

# copybara:uncomment_begin(google-only)
# py_proto_library(
#     name = "test_example_proto_py",
#     api_version = 2,
#     deps = [":test_example_proto"],
# )
# copybara:uncomment_end
