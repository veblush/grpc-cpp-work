workspace(name = "grpc_gcp_cpp")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "com_google_absl",
    sha256 = "c14b840dc57926b8b671805426a82249e5ea0d7fddf709fd4619eb38cbb36fb5",
    strip_prefix = "abseil-cpp-b832dce8489ef7b6231384909fd9b68d5a5ff2b7",
    url = "https://github.com/abseil/abseil-cpp/archive/b832dce8489ef7b6231384909fd9b68d5a5ff2b7.tar.gz",
)

http_archive(
    name = "com_github_grpc_grpc",
    sha256 = "23fe1e37eef78f5ca282c0d8d0d118a449e43cb49c72101b7525a910d3dab693",
    strip_prefix = "grpc-a14bc445d2674766525c5788a59c699970edd506",
    url = "https://github.com/grpc/grpc/archive/a14bc445d2674766525c5788a59c699970edd506.tar.gz",
)

http_archive(
    name = "com_google_googleapis",
    sha256 = "136e333508337030e112afe4974e2e595a8f4751e9a1aefc598b7aa7282740db",
    strip_prefix = "googleapis-a9a9950dc472e7036e05df8dd29597cd19235649",
    urls = [
        "https://github.com/googleapis/googleapis/archive/a9a9950dc472e7036e05df8dd29597cd19235649.tar.gz",
    ],
)

load("@com_google_googleapis//:repository_rules.bzl", "switched_rules_by_language")

switched_rules_by_language(
    name = "com_google_googleapis_imports",
    cc = True,
)

load('@com_github_grpc_grpc//bazel:grpc_deps.bzl', 'grpc_deps')
grpc_deps()

load('@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl', 'grpc_extra_deps')
grpc_extra_deps()
