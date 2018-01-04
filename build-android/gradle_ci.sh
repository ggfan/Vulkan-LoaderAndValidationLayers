#!/bin/bash

# Copyright 2017 The Android Open Source Project

# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at

#      http://www.apache.org/licenses/LICENSE-2.0

# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

export ANDROID_HOME=$HOME/dev/sdk_current
export ANDROID_NDK_HOME=$HOME/dev/android-ndk-r16b
export PATH=$ANDROID_NDK_HOME:$PATH

./update_external_sources_android.sh
./android-generate.sh

pushd cmake/test
errorCode=0
./gradlew clean assembleDebug
if [ ! -f app/build/outputs/apk/debug/app-debug.apk ]; then
  echo "***** ERROR: Validation Layers for Android build failed *****"
  errorCode=-1
fi
popd

exit $errorCode
