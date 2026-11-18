all: Frameworks/*.xcframework Frameworks/.symlinks

MPV_VERSION=v0.0.5
MPV_REPO=https://github.com/alexmercerind/libmpv-build/releases/download/$(MPV_VERSION)
MPV_IOS_UNIVERSAL_SHA256SUM=92c9a49890437609b64279ff8eec001845e2788bf2a893a1e90e8d38a6c13e86

SED=sed
SED_INPLACE_FLAG := $(shell ${SED} --version 2>&1 | grep -q 'GNU' && echo "" || echo "''")

.cache/xcframeworks/libmpv_ios_universal_audio-${MPV_VERSION}.zip:
	mkdir -p .cache/xcframeworks
	rm -f .cache/xcframeworks/*.tmp .cache/xcframeworks/libmpv_ios_universal_audio-*.zip
	curl -L \
		$(MPV_REPO)/libmpv_ios_universal_audio.zip \
		-o .cache/xcframeworks/libmpv_ios_universal_audio.zip.tmp
	shasum -a 256 -c <<< '$(MPV_IOS_UNIVERSAL_SHA256SUM)  .cache/xcframeworks/libmpv_ios_universal_audio.zip.tmp'
	mv .cache/xcframeworks/libmpv_ios_universal_audio.zip.tmp .cache/xcframeworks/libmpv_ios_universal_audio-${MPV_VERSION}.zip
	touch .cache/xcframeworks/libmpv_ios_universal_audio-${MPV_VERSION}.zip

Frameworks/*.xcframework: .cache/xcframeworks/libmpv_ios_universal_audio-${MPV_VERSION}.zip
	mkdir -p Frameworks
	rm -rf Frameworks/*.xcframework
	unzip -q -o .cache/xcframeworks/libmpv_ios_universal_audio-${MPV_VERSION}.zip -d Frameworks
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
