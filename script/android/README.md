# Package installation for Android Studio

Run the `install_packages.bat` or `install_packages.sh` and it will try to install everything at once.

## Native SDL3 build

SDL2 remains the default. The opt-in SDL3 build uses CMake and compiles both SDL3 and SDL3_mixer from the immutable revisions in
`cmake/sdl3-dependencies.json`. Java bindings come from that same SDL checkout, avoiding Java/native version mismatches.
No SDL2 prebuilt package is needed for this configuration.

With Java 17, the Android SDK and its command-line tools installed, run from the repository root:

```sh
sdkmanager "platforms;android-35" "ndk;28.2.13676358" "cmake;3.30.5"
python3 script/android/prepare_sdl3.py
cd android
./gradlew -PuseSDL3=true -PandroidAbis=arm64-v8a,x86_64 assembleDebug bundleDebug
cd ..
python3 script/android/check_sdl3_apk.py android/app/build/outputs/apk/debug/app-debug.apk arm64-v8a x86_64
```

Omit `androidAbis` to build all four supported ABIs. The APK and bundle are under `android/app/build/outputs/`.
Dependency sources are under `android/build/sdl3-deps/`. CMake output and generated assets remain in build directories;
the packaging tasks do not write into `android/app/src/main/assets`. Asset digests use stable path order and SHA-512.

Public CI builds ARM64 and x86_64 packages and verifies their native library architecture and complete asset digests without original game data.
SDL3 input tests cover letterboxed touch conversion and canceled gestures. A device playtest is still required for touch gestures, external mouse/controller,
music and sound, background/resume, screen locking, rotation, and quitting/relaunching the game activity. Verify that cancellation never activates a button
and that packaged asset extraction still works on first launch and after an update. Original game data for this playtest is supplied locally by the developer.
