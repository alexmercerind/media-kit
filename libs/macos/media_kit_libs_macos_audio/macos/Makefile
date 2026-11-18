all: Frameworks/*.xcframework Frameworks/.symlinks

MPV_VERSION=v0.0.5
MPV_REPO=https://github.com/alexmercerind/libmpv-build/releases/download/$(MPV_VERSION)
MPV_MACOS_UNIVERSAL_SHA256SUM=3b5db00234f4b1ffb659546b681d7eea66f0f0d2bd874ff3f96d9e3017ff1bfa

SED=sed
SED_INPLACE_FLAG := $(shell ${SED} --version 2>&1 | grep -q 'GNU' && echo "" || echo "''")

.cache/xcframeworks/libmpv_macos_universal_audio-${MPV_VERSION}.zip:
	mkdir -p .cache/xcframeworks
	rm -f .cache/xcframeworks/*.tmp .cache/xcframeworks/libmpv_macos_universal_audio-*.zip
	curl -L \
		$(MPV_REPO)/libmpv_macos_universal_audio.zip \
		-o .cache/xcframeworks/libmpv_macos_universal_audio.zip.tmp
	shasum -a 256 -c <<< '$(MPV_MACOS_UNIVERSAL_SHA256SUM)  .cache/xcframeworks/libmpv_macos_universal_audio.zip.tmp'
	mv .cache/xcframeworks/libmpv_macos_universal_audio.zip.tmp .cache/xcframeworks/libmpv_macos_universal_audio-${MPV_VERSION}.zip
	touch .cache/xcframeworks/libmpv_macos_universal_audio-${MPV_VERSION}.zip

Frameworks/*.xcframework: .cache/xcframeworks/libmpv_macos_universal_audio-${MPV_VERSION}.zip
	mkdir -p Frameworks
	rm -rf Frameworks/*.xcframework
	unzip -q -o .cache/xcframeworks/libmpv_macos_universal_audio-${MPV_VERSION}.zip -d Frameworks
	touch Frameworks/*.xcframework

Frameworks/.symlinks: Frameworks/*.xcframework
	rm -rf Frameworks/.symlinks
	mkdir -p Frameworks/.symlinks/mpv
	${SED} -i ${SED_INPLACE_FLAG} 's/\r$$//g' create_framework_symlinks.sh # remove CRLF line terminator added by Flutter packaging (https://github.com/media-kit/media-kit/issues/338)
	sh create_framework_symlinks.sh Frameworks/Mpv.xcframework Frameworks/.symlinks/mpv

.PHONY: clean
clean:
	rm -rf .cache
	rm -f ./Headers/mpv/*.h
	rm -rf ./Frameworks/*.xcframework
