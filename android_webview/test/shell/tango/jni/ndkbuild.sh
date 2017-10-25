# Copyright 2014 Google Inc. All Rights Reserved.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.


# Rebuilt (-B) specifying the makefile to use and the output folder for the final libraries and intermediate files
echo "Rebuilding..."
# Add V=1 for a verbose output that shows the commands that are being executed.
# $ANDROID_NDK_PATH/ndk-build NDK_APPLICATION_MK=./Application.mk NDK_LIBS_OUT=../libs NDK_OUT=./objs -B 
../../../../../third_party/android_tools/ndk/ndk-build NDK_APPLICATION_MK=./Application.mk NDK_LIBS_OUT=../libs NDK_OUT=./objs -B
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then 
	exit $EXIT_CODE
fi
rm -rf ../../../../../third_party/tango/libtango_chromium
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then 
	exit $EXIT_CODE
fi
mkdir -p ../../../../../third_party/tango/libtango_chromium/arm64-v8a
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then 
	exit $EXIT_CODE
fi
cp ../libs/arm64-v8a/libtango_chromium.so ../../../../../third_party/tango/libtango_chromium/arm64-v8a/libtango_chromium.so
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then 
	exit $EXIT_CODE
fi
cp TangoHandler.h ../../../../../third_party/tango/libtango_chromium
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then 
	exit $EXIT_CODE
fi
cp Anchor.h ../../../../../third_party/tango/libtango_chromium
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then 
  exit $EXIT_CODE
fi
cp AnchorManager.h ../../../../../third_party/tango/libtango_chromium
EXIT_CODE=$?
if [ $EXIT_CODE -ne 0 ]; then 
  exit $EXIT_CODE
fi
echo "Rebuilt!"

