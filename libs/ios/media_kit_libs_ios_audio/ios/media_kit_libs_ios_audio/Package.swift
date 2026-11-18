// swift-tools-version: 5.9
// The swift-tools-version declares the minimum version of Swift required to build this package.

import PackageDescription

let libmpvVersion = "v0.0.5"
let libmpvURL = "https://github.com/alexmercerind/libmpv-build/releases/download/\(libmpvVersion)/libmpv_ios_universal_audio.zip"
let libmpvChecksum = "92c9a49890437609b64279ff8eec001845e2788bf2a893a1e90e8d38a6c13e86"

let package = Package(
    name: "media_kit_libs_ios_audio",
    platforms: [
        .iOS("14.0")
    ],
    products: [
        .library(name: "media-kit-libs-ios-audio", targets: ["media_kit_libs_ios_audio", "Mpv"]),
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
            name: "media_kit_libs_ios_audio",
            dependencies: [.target(name: "Mpv")],
            resources: [
                .process("PrivacyInfo.xcprivacy")
            ]
        )
    ]
)
