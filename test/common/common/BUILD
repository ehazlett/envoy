licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_test",
    "envoy_package",
)

envoy_package()

envoy_cc_test(
    name = "base64_test",
    srcs = ["base64_test.cc"],
    deps = [
        "//source/common/buffer:buffer_lib",
        "//source/common/common:base64_lib",
    ],
)

envoy_cc_test(
    name = "hex_test",
    srcs = ["hex_test.cc"],
    deps = ["//source/common/common:hex_lib"],
)

envoy_cc_test(
    name = "optional_test",
    srcs = ["optional_test.cc"],
    deps = ["//include/envoy/common:optional"],
)

envoy_cc_test(
    name = "log_macros_test",
    srcs = ["log_macros_test.cc"],
    deps = [
        "//source/common/common:logger_lib",
        "//test/mocks/http:http_mocks",
        "//test/mocks/network:network_mocks",
        "//test/mocks/upstream:upstream_mocks",
    ],
)

envoy_cc_test(
    name = "utility_test",
    srcs = ["utility_test.cc"],
    deps = ["//source/common/common:utility_lib"],
)

envoy_cc_test(
    name = "to_lower_table_test",
    srcs = ["to_lower_table_test.cc"],
    deps = ["//source/common/common:to_lower_table_lib"],
)
