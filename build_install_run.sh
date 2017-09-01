# Copyright 2017 Google Inc. All Rights Reserved.
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

clear; reset;

# Rebuild the tango_chromium library
echo "Rebuilding libtango.so..."
cd android_webview/test/shell/tango/jni
./ndkbuild.sh

# Check if everything went well
if [ $? -eq 0 ]; then
	cd ../../../../..
else
	cd ../../../../..
	exit 1
fi	

# Get the name of the current branch as it will be used to determine the out folder.
BRANCH_NAME=$(git symbolic-ref -q HEAD)
BRANCH_NAME=${BRANCH_NAME##refs/heads/}
BRANCH_NAME=${BRANCH_NAME:-HEAD}
BRANCH_NAME="build"

# Try to identify if goma is being used to
# a) Make sure the proxy is running.
# b) Use goma in the ninja command below
USE_GOMA=`grep "use_goma" out/build/args.gn | head -1`
if [ ! -z "$USE_GOMA" ]; then
	# TODO: This parsing takes care of just the first occurrence of use_gome in the file, but what if there are more than one, some with a comment? It is really a corner case so we won't take it into account for now.
	COMMENT_POS_IN_USE_GOMA=`echo "$USE_GOMA" | grep -b -o "#" | head -1 | cut -d: -f1`
	USE_GOMA_POS_IN_USE_GOMA=`echo "$USE_GOMA" | grep -b -o "use_goma" | head -1 | cut -d: -f1`
	TRUE_POS_IN_USE_GOMA=`echo "$USE_GOMA" | grep -b -o "true" | head -1 | cut -d: -f1`
	# All inside the 'use_goma' line, cancel USE_GOMA if:
	# (there is no 'true' word OR (there is a comment AND the comment is before the 'true' word)) OR (there is is a comment AND the comment is before the 'use_goma' word)
	if ([ -z "$TRUE_POS_IN_USE_GOMA" ] || ([ ! -z "$COMMENT_POS_IN_USE_GOMA" ] && [ "$COMMENT_POS_IN_USE_GOMA" -lt "$TRUE_POS_IN_USE_GOMA" ])) || ([ ! -z "$COMMENT_POS_IN_USE_GOMA" ] && [ "$COMMENT_POS_IN_USE_GOMA" -lt "$USE_GOMA_POS_IN_USE_GOMA" ]); then
		# Not using goma, so set an empty string
		USE_GOMA=""
	fi	
fi

# If goma is being used, make sure the service has started.
if [ ! -z "$USE_GOMA" ]; then
	echo "Making sure that goma is started..."
	if [ ! -z "$GOMA_DIR" ]; then
		python "$GOMA_DIR/goma_ctl.py" ensure_start
	else
		# TODO: Identify and use the non-commented-out path to goma from the args.gn file (goma_dir)
		# GOMA_DIR=`grep "goma_dir" out/build/args.gn`
		if [ -z "$GOMA_DIR" ]; then
			python "~/goma/goma_ctl.py" ensure_start
		fi
	fi
	echo "Ensured that goma has started!"
fi

if [ $? -eq 0 ]; then
	# Determine the type of chromium to build
	APK_TYPE="android_webview_apk"
	if [ $# -eq 1 ]; then
		APK_TYPE="$1"
	fi

	if [ "$APK_TYPE" == "chrome_public_apk" ]; then
		APK_FILE_NAME="ChromePublic"
		APK_RUN_SCRIPT_NAME="adb_run_chrome_public"
		COMMAND_LINE="adb_chrome_public_command_line"
		APP_CLASSPATH="org.chromium.chrome/com.google.android.apps.chrome"
	elif [ "$APK_TYPE" == "android_webview_apk" ]; then
		APK_FILE_NAME="AndroidWebView"
		APK_RUN_SCRIPT_NAME="adb_run_android_webview_shell"
		COMMAND_LINE="adb_android_webview_command_line"
		APP_CLASSPATH="org.chromium.android_webview.shell"
	elif [ "$APK_TYPE" == "content_shell_apk" ]; then
		APK_FILE_NAME="ContentShell"
		APK_RUN_SCRIPT_NAME="adb_run_content_shell"
		COMMAND_LINE="adb_content_shell_command_line"
		APP_CLASSPATH="org.chromium.content_shell_apk"
	else
		echo "ERROR: The provided apk target name to be built '$APK_TYPE' is not one of the 3 possibilities: chrome_public_apk, android_webview_apk, content_shell_apk"
		exit 1
	fi

	# Prepare the build
	echo "Regenerating ninja files from GN..."
	gn gen out/build
	if [ $? -eq 0 ]; then
		echo "Regenerated!"
		echo "Building $APK_TYPE for Android at 'out/build' folder..."
		NINJA_RESULT=0
		# The ninja command is different if using goma
		if [ -z "$USE_GOMA" ]; then
			ninja -C out/build "$APK_TYPE"
			NINJA_RESULT=$?
		else
			ninja -C out/build -j 320 "$APK_TYPE"
			NINJA_RESULT=$?
		fi
		if [ $NINJA_RESULT -eq 0 ]; then
			# adb uninstall "$APP_CLASSPATH"
			echo "Built!"
			# Copy the final APK to the apk folder
			rm -rf apk 
			mkdir apk
			cp "out/build/apks/AndroidWebView.apk" apk/WebARonARCore.apk
			echo "Installing $APK_TYPE on Android device..."
			adb install -r "out/build/apks/$APK_FILE_NAME.apk"
			if [ $? -eq 0 ]; then
				echo "Installed!"
				adb shell pm grant "$APP_CLASSPATH" android.permission.CAMERA
				adb shell pm grant "$APP_CLASSPATH" android.permission.READ_EXTERNAL_STORAGE
				adb shell pm grant "$APP_CLASSPATH" android.permission.RECORD_AUDIO
				adb shell pm grant "$APP_CLASSPATH" android.permission.MODIFY_AUDIO_SETTINGS
				adb shell pm grant "$APP_CLASSPATH" android.permission.ACCESS_COARSE_LOCATION
				# echo "Setting up Chrome flags..."
				# build/android/$COMMAND_LINE --ignore-gpu-blacklist --enable-webvr
				# if [ $? -eq 0 ]; then
				# 	echo "Chrome flags set!"
					build/android/"$APK_RUN_SCRIPT_NAME"
				# fi
			fi
		fi
	fi
fi
