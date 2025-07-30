if [ "$1" = "qnx" ]; then
    cd tools/toolchains/qnx/qnx-sdp/
    source qnxsdp-env.sh
    cd -
    ./tools/iceoryx_build_test.sh -b build-qnx/ debug -t `pwd`/tools/toolchains/qnx/qnx_sdp70_aarch64le.cmake examples
elif [ "$1" = "android" ]; then
    cp ~/work/projects/maxus/qcom_la/lagvm/LINUX/android/prebuilts/build-tools/sysroots/aarch64-unknown-linux-musl/lib/libpthread.a   ~/work/projects/maxus/qcom_la/lagvm/LINUX/android/kernel_platform/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/
    cp ~/work/projects/maxus/qcom_la/lagvm/LINUX/android/prebuilts/build-tools/sysroots/aarch64-unknown-linux-musl/lib/librt.a   ~/work/projects/maxus/qcom_la/lagvm/LINUX/android/kernel_platform/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/lib/
    ./tools/iceoryx_build_test-android.sh -b build-android -t ~/work/projects/maxus/qcom_la/lagvm/LINUX/android/kernel_platform/prebuilts/ndk-r23/build/cmake/android.toolchain.cmake debug examples
elif [ "$1" = "host" ]; then
    ./tools/iceoryx_build_test-host.sh -b build-host/ debug examples
else
    echo "Usage: $0 [qnx|android]"
    exit 1
fi
