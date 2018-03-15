# WebARonARCore

**An experimental browser for Android that lets developers create Augmented Reality (AR) experiences using web technologies. An [iOS version](https://github.com/google-ar/WebARonARKit) is also available.**

<img alt="Spawn-at-Camera example" src="https://github.com/google-ar/three.ar.js/raw/master/examples/screencaps/20170829-arcore-spawnAtCamera-1.gif" style="float: left; object-fit: cover; width: 45%; height: 20em; margin-right: 1em; "><img alt="Spawn-at-Surface example" src="https://github.com/google-ar/three.ar.js/raw/master/examples/screencaps/20170829-arcore-spawnAtSurface-1.gif" style="width: 45%; height: 20em; object-fit: cover;">

**Note:** This is not an official Google product. Nor is it a fully-featured web browser. Nor are the enabling JavaScript APIs standards, or on the standardization path. WebARonARCore is only meant to enable developer experimentation.

## Getting started
 
### <a name="InstallingTheARCoreSDK">1. Install the ARCore APK Developer Preview 1</a>

WebARonARCore is still built on top of the Android ARCore Developer Preview 1 APK and because of that, only a certain number of devices are supported officially: Google Pixel 1 and 2 (both standard and XL) and Samsung Galaxy S8. The check to only detect these devices at runtime has been removed but the behavior on other ARCore models (supported by newer ARCore versions) is undefined.

Install the ARCore APK, either directly from a device:

* Visit [this link](https://github.com/google-ar/arcore-android-sdk/releases/download/sdk-preview/arcore-preview.apk) from a web browser on your Android device to download and install the ARCore Developer Preview 1 APK.

...or by using ADB:

* Download the ARCore Developer Preview 1 APK to your computer from [here](https://github.com/google-ar/arcore-android-sdk/releases/download/sdk-preview/arcore-preview.apk) and install the APK on your device:
  * `$ adb install -r path/to/arcore_preview.apk`

### <a name="InstallTheWebARonARCoreAPK">2. Install the WebARonARCore APK</a>

Directly from a device: 

* Visit [this link](https://github.com/google-ar/WebARonARCore/raw/webarcore_57.0.2987.5/apk/WebARonARCore.apk) from a web browser on your Android device to download and install the WebARonARCore APK.

...or by using ADB:

* Download the [WebARonARCore APK](https://github.com/google-ar/WebARonARCore/raw/webarcore_57.0.2987.5/apk/WebARonARCore.apk). 
* Install the APK to your device:
  * `$ adb install -r /path/to/WebARonARCore.apk`

Alternatively, the WebARonARCore APK can be [built and installed from source](#BuildingFromSource).

### <a name="ViewingExamples">3. Viewing examples</a>
A [list of example scenes](https://developers.google.com/ar/develop/web/getting-started#examples) compatible with WebARonARCore and [WebARonARKit](https://github.com/google-ar/WebARonARKit) are available at [developers.google.com](https://developers.google.com/ar/develop/web/getting-started#examples).

### <a name="BuildingScenes">4. Building your own scenes</a>
To build AR web experiences that work with WebARonARCore (or [WebARonARKit for iOS](https://github.com/google-ar/WebARonARKit)), we recommend **[three.ar.js](https://github.com/google-ar/three.ar.js)**, a helper library that works with the popular [three.js](http://threejs.org) WebGL framework. [Three.ar.js](https://github.com/google-ar/three.ar.js) provides common AR building blocks, such as a visible reticle that draws on top of real world surfaces, and [example scenes](https://github.com/google-ar/three.ar.js#examples).

### <a name="debugging">5. Debugging</a>

WebARonARCore uses WebViews, which is a similar debugging process to debugging Chrome for Android tabs. Check out the prereqs for your device at [Get Started with Remote Debugging Android Devices](https://developers.google.com/web/tools/chrome-devtools/remote-debugging/), and learn more about [Remote Debugging WebViews](https://developers.google.com/web/tools/chrome-devtools/remote-debugging/webviews#open_a_webview_in_devtools) by opening `chrome://inspect` in the desktop browser while your device is connected via USB.

## <a name="BuildingFromSource">Building the WebARonARCore APK from source</a>

WebARonARCore can optionally be built and installed from source. Instructions for [cloning and building Chromium](https://www.chromium.org/developers/how-tos/android-build-instructions) are available at [chromium.org](https://www.chromium.org/developers/how-tos/android-build-instructions)

Prerequisites:

* Linux machine
* GIT
* Python

We recommend the following steps:

1. Open a terminal window
2. Install depot_tools. You can follow this [tutorial](https://commondatastorage.googleapis.com/chrome-infra-docs/flat/depot_tools/docs/html/depot_tools_tutorial.html#_setting_up) or simply follow these 2 steps:
  * `git clone https://chromium.googlesource.com/chromium/tools/depot_tools.git`
  * `export PATH=$PATH:/path/to/depot_tools`
3. Create a folder to contain `chromium` and move to it: `$ mkdir ~/chromium && cd ~/chromium`
4. Checkout the Chromium repo: `~/chromium$ fetch --nohooks android`. **Note**: This process may take a long time (an hour?)
5. Enter the `src` folder: `$ cd src`.
6. Add this git repo as a remote (we are going to call it 'github'): `git remote add github https://github.com/google-ar/WebARonARCore.git`
7. Fetch the newly added remote: `git fetch github`
8. Checkout the webarcore branch from the github remote: `git checkout --track github/webarcore_57.0.2987.5`
9. Synchronize the dependencies with this command: `~/chromium/src$ gclient sync --disable-syntax-validation`. **Note**: This process may take some time too.
10. Create a folder where to make the final product compilation: `~/chromium/src$ mkdir -p out/build`.
11. Create and edit a new file `out/build/args.gn`. Copy and paste the following content in the `args.gn` file:
```
  target_os = "android"
  target_cpu = "arm64"
  is_debug = false
  is_component_build = true
  enable_webvr = true
  proprietary_codecs = false
  ffmpeg_branding = "Chromium"
  enable_nacl = false
  remove_webcore_debug_symbols = true
```
12. Prepare to build: `~/chromium/src$ gn args out/build`. **Note**: once the command is executed, the vi editor will show you the content of the `args.gn` file just edited a few steps before. Just exit by pressing ESC and typing colon and `x`.
13. Install the build dependencies: `~/chromium/src$ build/install-build-deps-android.sh`
14. Synchronize the resources once again: `~/chromium/src$ gclient sync --disable-syntax-validation`
15. Setup the environment: `~/chromium/src$ . build/android/envsetup.sh`

##### Build, install and run

The line below not only compiles Chromium but also installs the final APK on to a connected device and runs it, so it is convenient that you to connect the device via USB before executing it. The project that will be built by default is the Chromium WebView project, the only one that has been modified to provide AR capabilities.
```
~/chromium/src$ ./build_install_run.sh
```
You can review the content of the script to see what it does (it is a fairly simple script) but if you would like to compile the final APK on your own you could do it by executing the following command:
```
~/chromium/src$ ninja -C out/build android_webview_apk
```

The final APK will be built in the folders `~/chromium/src/apk` and `~/chromium/src/out/build/apks`.

## <a name="HowWebARonARCoreWorks">How WebARonARCore works</a>

WebARonARCore is built of two essential technologies: ARCore and Chromium. We also extend the WebVR 1.1 API, which gives us much of what we need for augmented reality, with a few more essentials, such as motion tracking, rendering of the camera's video feed, and basic understanding of the real world. For details, see [WebVR API extension for smartphone AR](https://github.com/google-ar/three.ar.js/blob/master/webvr_ar_extension.md)

## <a name="KnownIssues">Known issues</a>
* The current implementation of WebARonARCore is built on top of the Chromium WebView flavor. This has some implementation advantages but some performance and use disadvantages. We are working on making the implementation on a full version of Chromium.
* The current implementarion of WebARonARCore is built on top of the ARCore Developer Preview version 1. This may lead to uncertain behavior in some devices that were not supported by that ARCore version.
* Pausing/resuming/switching away from the app causes screen to turn black. This is a consequence of having built the implementation on top of the WebView flavor of Chromium. A proper implementation on full Chromium or a rebase to a more recent Chromium WebView version (>57.0.2987.5) might solve this problem.
* The [Web Speech](https://dvcs.w3.org/hg/speech-api/raw-file/tip/speechapi.html) API is a standard web API for text to speech and speech to text conversion that is available in Chromium. As WebARonARCore is built on top of the WebView version of Chromium, does not provide this functionality by default. There is a solution though, using a polyfill we provide, but in order to use it, you need to either a) include the [three.ar.js](https://github.com/google-ar/three.ar.js) library before making any use of the Web Speech API or b) include the [ARSpeechRecognition.js](https://github.com/google-ar/three.ar.js/blob/master/src/ARSpeechRecognition.js) file also before making any reference to the Web Speech API. Only speech recognition is suspported, not speech synthesis for now.

## <a name="FutureWork">Future work</a>
* Add more AR-related features.
* Adapt the implementation to the WebXR spec proposal.
* Implement the prototype on full Chromium (not on the WebView flavor) and to a newer tag version (>57.0.2987.5).
* Improve the VRPassThroughCamera rendering pipeline either making it obscure for the developer or by using regular WebGL textures and shader samplers without having to use the external image texture extension.
* Adapt the prototype to the latest ARCore.

## <a name="License">License</a>
Apache License Version 2.0 (see the `LICENSE` file inside this repo).

