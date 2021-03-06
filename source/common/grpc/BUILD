licenses(["notice"])  # Apache 2

load(
    "//bazel:envoy_build_system.bzl",
    "envoy_cc_library",
    "envoy_package",
)

envoy_package()

envoy_cc_library(
    name = "async_client_lib",
    hdrs = ["async_client_impl.h"],
    deps = [
        ":codec_lib",
        ":common_lib",
        "//include/envoy/grpc:async_client_interface",
        "//source/common/buffer:zero_copy_input_stream_lib",
        "//source/common/http:async_client_lib",
    ],
)

envoy_cc_library(
    name = "codec_lib",
    srcs = ["codec.cc"],
    hdrs = ["codec.h"],
    deps = [
        "//include/envoy/buffer:buffer_interface",
        "//source/common/buffer:buffer_lib",
    ],
)

envoy_cc_library(
    name = "common_lib",
    srcs = ["common.cc"],
    hdrs = ["common.h"],
    external_deps = ["protobuf"],
    deps = [
        "//include/envoy/common:optional",
        "//include/envoy/grpc:status",
        "//include/envoy/http:header_map_interface",
        "//include/envoy/http:message_interface",
        "//include/envoy/stats:stats_interface",
        "//include/envoy/upstream:cluster_manager_interface",
        "//include/envoy/upstream:upstream_interface",
        "//source/common/buffer:buffer_lib",
        "//source/common/common:assert_lib",
        "//source/common/common:empty_string",
        "//source/common/common:enum_to_int",
        "//source/common/common:utility_lib",
        "//source/common/http:filter_utility_lib",
        "//source/common/http:headers_lib",
        "//source/common/http:message_lib",
        "//source/common/http:utility_lib",
    ],
)

envoy_cc_library(
    name = "http1_bridge_filter_lib",
    srcs = ["http1_bridge_filter.cc"],
    hdrs = ["http1_bridge_filter.h"],
    deps = [
        ":common_lib",
        "//include/envoy/http:codes_interface",
        "//include/envoy/http:filter_interface",
        "//include/envoy/upstream:cluster_manager_interface",
        "//source/common/common:enum_to_int",
        "//source/common/common:utility_lib",
        "//source/common/http:headers_lib",
        "//source/common/http/http1:codec_lib",
    ],
)

envoy_cc_library(
    name = "grpc_web_filter_lib",
    srcs = ["grpc_web_filter.cc"],
    hdrs = ["grpc_web_filter.h"],
    deps = [
        ":codec_lib",
        ":common_lib",
        "//include/envoy/http:filter_interface",
        "//include/envoy/upstream:cluster_manager_interface",
        "//source/common/common:base64_lib",
        "//source/common/common:utility_lib",
        "//source/common/http:headers_lib",
    ],
)

envoy_cc_library(
    name = "rpc_channel_lib",
    srcs = ["rpc_channel_impl.cc"],
    hdrs = ["rpc_channel_impl.h"],
    external_deps = ["protobuf"],
    deps = [
        ":common_lib",
        "//include/envoy/grpc:rpc_channel_interface",
        "//include/envoy/upstream:cluster_manager_interface",
        "//source/common/buffer:zero_copy_input_stream_lib",
        "//source/common/common:assert_lib",
        "//source/common/common:enum_to_int",
        "//source/common/common:utility_lib",
        "//source/common/http:headers_lib",
        "//source/common/http:message_lib",
        "//source/common/http:utility_lib",
    ],
)

envoy_cc_library(
    name = "transcoder_input_stream_lib",
    srcs = ["transcoder_input_stream_impl.cc"],
    hdrs = ["transcoder_input_stream_impl.h"],
    external_deps = ["grpc_transcoding"],
    deps = [
        "//source/common/buffer:zero_copy_input_stream_lib",
    ],
)
