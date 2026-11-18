// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let libmpvVersion = "v0.0.5"
let libmpvURL = "https://github.com/alexmercerind/libmpv-build/releases/download/\(libmpvVersion)/libmpv_macos_universal_audio.zip"
let libmpvChecksum = "3b5db00234f4b1ffb659546b681d7eea66f0f0d2bd874ff3f96d9e3017ff1bfa"

let package = Package(
    name: "media_kit_libs_macos_audio",
    platforms: [
        .macOS("10.15")
    ],
    products: [
        .library(name: "media-kit-libs-macos-audio", targets: ["media_kit_libs_macos_audio", "Mpv"]),
        .library(name: "Mpv", targets: ["Mpv"])
    ],
    dependencies: [],
    targets: [
        .binaryTarget(
            name: "Mpv",
            url: libmpvURL,
            checksum: libmpvChecksum
        ),
        .target(
            name: "media_kit_libs_macos_audio",
            dependencies: [.target(name: "Mpv")],
            resources: [
                .process("PrivacyInfo.xcprivacy")
            ]
        )
    ]
)
