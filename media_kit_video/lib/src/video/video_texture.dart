/// This file is a part of media_kit (https://github.com/media-kit/media-kit).
///
/// Copyright © 2021 & onwards, Hitesh Kumar Saini <saini123hitesh@gmail.com>.
/// All rights reserved.
/// Use of this source code is governed by MIT license that can be found in the LICENSE file.
import 'dart:io';
import 'dart:async';
import 'package:flutter/widgets.dart';
import 'package:flutter/services.dart';

import 'package:media_kit_video/src/subtitle/subtitle_view.dart';
import 'package:media_kit_video/media_kit_video_controls/media_kit_video_controls.dart'
    as media_kit_video_controls;

import 'package:media_kit_video/src/utils/wakelock.dart';
import 'package:media_kit_video/src/video_controller/video_controller.dart';
import 'package:media_kit_video/src/video_controller/platform_video_controller.dart';

/// {@template video}
///
/// Video
/// -----
/// [Video] widget is used to display video output.
///
/// Use [VideoController] to initialize & handle the video rendering.
///
/// **Example:**
///
/// ```dart
/// class MyScreen extends StatefulWidget {
///   const MyScreen({Key? key}) : super(key: key);
///   @override
///   State<MyScreen> createState() => MyScreenState();
/// }
///
/// class MyScreenState extends State<MyScreen> {
///   late final player = Player();
///   late final controller = VideoController(player);
///
///   @override
///   void initState() {
///     super.initState();
///     player.open(Media('https://user-images.githubusercontent.com/28951144/229373695-22f88f13-d18f-4288-9bf1-c3e078d83722.mp4'));
///   }
///
///   @override
///   void dispose() {
///     player.dispose();
///     super.dispose();
///   }
///
///   @override
///   Widget build(BuildContext context) {
///     return Scaffold(
///       body: Video(
///         controller: controller,
///       ),
///     );
///   }
/// }
/// ```
///
/// {@endtemplate}
class Video extends StatefulWidget {
  /// The [VideoController] reference to control this [Video] output.
  final VideoController controller;

  /// Height of this viewport.
  final double? width;

  /// Width of this viewport.
  final double? height;

  /// Fit of the viewport.
  final BoxFit fit;

  /// Background color to fill the video background.
  final Color fill;

  /// Alignment of the viewport.
  final Alignment alignment;

  /// Preferred aspect ratio of the viewport.
  final double? aspectRatio;

  /// Filter quality of the [Texture] widget displaying the video output.
  final FilterQuality filterQuality;

  /// Video controls builder.
  final VideoControlsBuilder? controls;

  /// Whether to acquire wake lock while playing the video.
  final bool wakelock;

  /// Whether to pause the video when application enters background mode.
  final bool pauseUponEnteringBackgroundMode;

  /// Whether to resume the video when application enters foreground mode.
  ///
  /// This attribute is only applicable if [pauseUponEnteringBackgroundMode] is `true`.
  ///
  final bool resumeUponEnteringForegroundMode;

  /// The configuration for subtitles e.g. [TextStyle] & padding etc.
  final SubtitleViewConfiguration subtitleViewConfiguration;

  /// The callback invoked when the [Video] enters fullscreen.
  final Future<void> Function() onEnterFullscreen;

  /// The callback invoked when the [Video] exits fullscreen.
  final Future<void> Function() onExitFullscreen;

  /// {@macro video}
  const Video({
    Key? key,
    required this.controller,
    this.width,
    this.height,
    this.fit = BoxFit.contain,
    this.fill = const Color(0xFF000000),
    this.alignment = Alignment.center,
    this.aspectRatio,
    this.filterQuality = FilterQuality.low,
    this.controls = media_kit_video_controls.AdaptiveVideoControls,
    this.wakelock = true,
    this.pauseUponEnteringBackgroundMode = true,
    this.resumeUponEnteringForegroundMode = false,
    this.subtitleViewConfiguration = const SubtitleViewConfiguration(),
    this.onEnterFullscreen = defaultEnterNativeFullscreen,
    this.onExitFullscreen = defaultExitNativeFullscreen,
  }) : super(key: key);

  @override
  State<Video> createState() => VideoState();
}

class VideoState extends State<Video> with WidgetsBindingObserver {
  final _contextNotifier = ValueNotifier<BuildContext?>(null);
  final _subtitleViewKey = GlobalKey<SubtitleViewState>();
  final _wakelock = Wakelock();
  final _subscriptions = <StreamSubscription>[];
  late int? _width = widget.controller.player.state.width;
  late int? _height = widget.controller.player.state.height;
  late bool _visible = (_width ?? 0) > 0 && (_height ?? 0) > 0;
  bool _pauseDueToPauseUponEnteringBackgroundMode = false;

  ValueNotifier<BoxFit?> _fitNotifier = ValueNotifier<BoxFit?>(null);
  ValueNotifier<Color?> _fillNotifier = ValueNotifier<Color?>(null);
  ValueNotifier<Alignment?> _alignmentNotifier =
      ValueNotifier<Alignment?>(null);
  ValueNotifier<double?> _aspectRatioNotifier = ValueNotifier<double?>(null);

  // Public API:

  bool isFullscreen() {
    return media_kit_video_controls.isFullscreen(_contextNotifier.value!);
  }

  Future<void> enterFullscreen() {
    return media_kit_video_controls.enterFullscreen(_contextNotifier.value!);
  }

  Future<void> exitFullscreen() {
    return media_kit_video_controls.exitFullscreen(_contextNotifier.value!);
  }

  Future<void> toggleFullscreen() {
    return media_kit_video_controls.toggleFullscreen(_contextNotifier.value!);
  }

  void setSubtitleViewPadding(
    EdgeInsets padding, {
    Duration duration = const Duration(milliseconds: 100),
  }) {
    return _subtitleViewKey.currentState?.setPadding(
      padding,
      duration: duration,
    );
  }

  void update({
    BoxFit? fit,
    Color? fill,
    Alignment? alignment,
    double? aspectRatio,
  }) {
    if (fit != null) _fitNotifier.value = fit;
    if (fill != null) _fillNotifier.value = fill;
    if (alignment != null) _alignmentNotifier.value = alignment;
    if (aspectRatio != null) _aspectRatioNotifier.value = aspectRatio;
  }

  @override
  void didChangeAppLifecycleState(AppLifecycleState state) {
    if (widget.pauseUponEnteringBackgroundMode) {
      if ([
        AppLifecycleState.paused,
        AppLifecycleState.detached,
      ].contains(state)) {
        if (widget.controller.player.state.playing) {
          _pauseDueToPauseUponEnteringBackgroundMode = true;
          widget.controller.player.pause();
        }
      } else {
        if (widget.resumeUponEnteringForegroundMode &&
            _pauseDueToPauseUponEnteringBackgroundMode) {
          _pauseDueToPauseUponEnteringBackgroundMode = false;
          widget.controller.player.play();
        }
      }
    }
  }

  @override
  void didChangeDependencies() {
    media_kit_video_controls.VideoStateInheritedWidget? wrapping =
        media_kit_video_controls.VideoStateInheritedWidget.maybeOf(context);

    // set initial values
    if (wrapping != null) {
      _fitNotifier = wrapping.fitNotifier;
      _fillNotifier = wrapping.fillNotifier;
      _alignmentNotifier = wrapping.alignmentNotifier;
      _aspectRatioNotifier = wrapping.aspectRatioNotifier;
    }
    super.didChangeDependencies();
  }

  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addObserver(this);

    // set initial values

    _fitNotifier.value = widget.fit;
    _fillNotifier.value = widget.fill;
    _alignmentNotifier.value = widget.alignment;
    _aspectRatioNotifier.value = widget.aspectRatio;

    // --------------------------------------------------
    // Do not show the video frame until width & height are available.
    // Since [ValueNotifier<Rect?>] inside [VideoController] only gets updated by the render loop (i.e. it will not fire when video's width & height are not available etc.), it's important to handle this separately here.
    _subscriptions.addAll(
      [
        widget.controller.player.stream.width.listen(
          (value) {
            _width = value;
            final visible = (_width ?? 0) > 0 && (_height ?? 0) > 0;
            if (_visible != visible) {
              setState(() {
                _visible = visible;
              });
            }
          },
        ),
        widget.controller.player.stream.height.listen(
          (value) {
            _height = value;
            final visible = (_width ?? 0) > 0 && (_height ?? 0) > 0;
            if (_visible != visible) {
              setState(() {
                _visible = visible;
              });
            }
          },
        ),
      ],
    );
    // --------------------------------------------------
    if (widget.wakelock) {
      if (widget.controller.player.state.playing) {
        _wakelock.enable();
      }
      _subscriptions.add(
        widget.controller.player.stream.playing.listen(
          (value) {
            if (value) {
              _wakelock.enable();
            } else {
              _wakelock.disable();
            }
          },
        ),
      );
    }
  }

  @override
  void dispose() {
    WidgetsBinding.instance.removeObserver(this);
    _wakelock.disable();
    for (final subscription in _subscriptions) {
      subscription.cancel();
    }
    super.dispose();
  }

  void refreshView() {}

  @override
  Widget build(BuildContext context) {
    final controls = widget.controls;
    final controller = widget.controller;
    // final aspectRatio = widget.aspectRatio;
    final subtitleViewConfiguration = widget.subtitleViewConfiguration;

    return media_kit_video_controls.VideoStateInheritedWidget(
        state: this as dynamic,
        contextNotifier: _contextNotifier,
        fitNotifier: _fitNotifier,
        fillNotifier: _fillNotifier,
        alignmentNotifier: _alignmentNotifier,
        aspectRatioNotifier: _aspectRatioNotifier,
        child: ValueListenableBuilder<Color?>(
            valueListenable: _fillNotifier,
            builder: (context, fill, _) {
              return Container(
                clipBehavior: Clip.none,
                width: widget.width ?? double.infinity,
                height: widget.height ?? double.infinity,
                color: fill,
                child: Stack(
                  fit: StackFit.expand,
                  children: [
                    ClipRect(
                      child: ValueListenableBuilder<Alignment?>(
                          valueListenable: _alignmentNotifier,
                          builder: (context, alignment, _) {
                            return ValueListenableBuilder<BoxFit?>(
                                valueListenable: _fitNotifier,
                                builder: (context, fit, _) {
                                  return FittedBox(
                                    alignment: alignment!,
                                    fit: fit!,
                                    child: ValueListenableBuilder<
                                        PlatformVideoController?>(
                                      valueListenable: controller.notifier,
                                      builder: (context, notifier, _) =>
                                          notifier == null
                                              ? const SizedBox.shrink()
                                              : ValueListenableBuilder<int?>(
                                                  valueListenable: notifier.id,
                                                  builder: (context, id, _) {
                                                    return ValueListenableBuilder<
                                                        double?>(
                                                      valueListenable:
                                                          _aspectRatioNotifier,
                                                      builder: (context,
                                                          aspectRatio, _) {
                                                        return ValueListenableBuilder<
                                                            Rect?>(
                                                          valueListenable:
                                                              notifier.rect,
                                                          builder: (context,
                                                              rect, _) {
                                                            if (id != null &&
                                                                rect != null &&
                                                                _visible) {
                                                              return SizedBox(
                                                                // Apply aspect ratio if provided.
                                                                width: aspectRatio ==
                                                                        null
                                                                    ? rect.width
                                                                    : rect.height *
                                                                        aspectRatio,
                                                                height:
                                                                    rect.height,
                                                                child: Stack(
                                                                  children: [
                                                                    const SizedBox(),
                                                                    Positioned
                                                                        .fill(
                                                                      child:
                                                                          Texture(
                                                                        textureId:
                                                                            id,
                                                                        filterQuality:
                                                                            widget.filterQuality,
                                                                      ),
                                                                    ),
                                                                    // Keep the |Texture| hidden before the first frame renders. In native implementation, if no default frame size is passed (through VideoController), a starting 1 pixel sized texture/surface is created to initialize the render context & check for H/W support.
                                                                    // This is then resized based on the video dimensions & accordingly texture ID, texture, EGLDisplay, EGLSurface etc. (depending upon platform) are also changed. Just don't show that 1 pixel texture to the UI.
                                                                    // NOTE: Unmounting |Texture| causes the |MarkTextureFrameAvailable| to not do anything on GNU/Linux.
                                                                    if (rect.width <=
                                                                            1.0 &&
                                                                        rect.height <=
                                                                            1.0)
                                                                      Positioned
                                                                          .fill(
                                                                        child:
                                                                            Container(
                                                                          color:
                                                                              widget.fill,
                                                                        ),
                                                                      ),
                                                                  ],
                                                                ),
                                                              );
                                                            }
                                                            return const SizedBox
                                                                .shrink();
                                                          },
                                                        );
                                                      },
                                                    );
                                                  }),
                                    ),
                                  );
                                });
                          }),
                    ),
                    if (subtitleViewConfiguration.visible &&
                        !(controller.player.platform?.configuration.libass ??
                            false))
                      Positioned.fill(
                        child: SubtitleView(
                          controller: controller,
                          key: _subtitleViewKey,
                          configuration: subtitleViewConfiguration,
                        ),
                      ),
                    if (controls != null)
                      Positioned.fill(
                        child: controls.call(this),
                      ),
                  ],
                ),
              );
            }));
  }
}

typedef VideoControlsBuilder = Widget Function(VideoState state);

// --------------------------------------------------

/// Makes the native window enter fullscreen.
Future<void> defaultEnterNativeFullscreen() async {
  try {
    if (Platform.isAndroid || Platform.isIOS) {
      await Future.wait(
        [
          SystemChrome.setEnabledSystemUIMode(
            SystemUiMode.immersiveSticky,
            overlays: [],
          ),
          SystemChrome.setPreferredOrientations(
            [
              DeviceOrientation.landscapeLeft,
              DeviceOrientation.landscapeRight,
            ],
          ),
        ],
      );
    } else if (Platform.isMacOS || Platform.isWindows || Platform.isLinux) {
      await const MethodChannel('com.alexmercerind/media_kit_video')
          .invokeMethod(
        'Utils.EnterNativeFullscreen',
      );
    }
  } catch (exception, stacktrace) {
    debugPrint(exception.toString());
    debugPrint(stacktrace.toString());
  }
}

/// Makes the native window exit fullscreen.
Future<void> defaultExitNativeFullscreen() async {
  try {
    if (Platform.isAndroid || Platform.isIOS) {
      await Future.wait(
        [
          SystemChrome.setEnabledSystemUIMode(
            SystemUiMode.manual,
            overlays: SystemUiOverlay.values,
          ),
          SystemChrome.setPreferredOrientations(
            [],
          ),
        ],
      );
    } else if (Platform.isMacOS || Platform.isWindows || Platform.isLinux) {
      await const MethodChannel('com.alexmercerind/media_kit_video')
          .invokeMethod(
        'Utils.ExitNativeFullscreen',
      );
    }
  } catch (exception, stacktrace) {
    debugPrint(exception.toString());
    debugPrint(stacktrace.toString());
  }
}

// --------------------------------------------------
