// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package org.chromium.android_webview.shell;

import android.Manifest;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.Intent;
import android.content.SharedPreferences;
import android.content.SharedPreferences.Editor;
import android.content.ServiceConnection;
import android.content.ComponentName;
import android.content.DialogInterface;
import android.content.pm.PackageManager;
import android.content.res.Configuration;
import android.os.IBinder;
import android.os.Bundle;
import android.text.TextUtils;
import android.view.Gravity;
import android.view.KeyEvent;
import android.view.View;
import android.view.View.OnClickListener;
import android.view.View.OnFocusChangeListener;
import android.view.ViewGroup;
import android.view.ViewGroup.LayoutParams;
import android.view.WindowManager;
import android.view.Display;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputMethodManager;
import android.webkit.GeolocationPermissions;
import android.webkit.WebChromeClient;
import android.widget.EditText;
import android.widget.FrameLayout;
import android.widget.ImageButton;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.TextView.OnEditorActionListener;
import android.support.v4.content.ContextCompat;
import android.support.v4.app.ActivityCompat;
import android.support.v4.app.ActivityCompat.OnRequestPermissionsResultCallback;
import android.hardware.Camera;

import org.chromium.android_webview.AwBrowserContext;
import org.chromium.android_webview.AwBrowserProcess;
import org.chromium.android_webview.AwContents;
import org.chromium.android_webview.AwContentsClient;
import org.chromium.android_webview.AwDevToolsServer;
import org.chromium.android_webview.AwSettings;
import org.chromium.android_webview.test.AwTestContainerView;
import org.chromium.android_webview.test.NullContentsClient;
import org.chromium.android_webview.JsResultReceiver;
import org.chromium.android_webview.JsPromptResultReceiver;
import org.chromium.android_webview.AwWebResourceResponse;
import org.chromium.android_webview.AwContentsClient.AwWebResourceRequest;
import org.chromium.android_webview.permission.AwPermissionRequest;
import org.chromium.base.BaseSwitches;
import org.chromium.base.CommandLine;
import org.chromium.base.ContextUtils;
import org.chromium.base.Log;
import org.chromium.base.TraceEvent;
import org.chromium.content.app.ContentApplication;
import org.chromium.content_public.browser.NavigationController;
import org.chromium.content_public.browser.WebContents;

import java.net.URI;
import java.net.URL;
import java.net.URISyntaxException;
import java.net.MalformedURLException;
import java.util.Locale;

import com.google.zxing.integration.android.IntentIntegrator;
import com.google.zxing.integration.android.IntentResult;

import java.util.ArrayList;

import org.json.JSONException;
import org.json.JSONObject;
import org.json.JSONArray;

import android.speech.SpeechRecognizer;
import android.speech.RecognizerIntent;
import android.speech.RecognitionListener;

import android.webkit.JavascriptInterface;

import com.google.atap.tangoservice.Tango;
import com.google.atap.tangoservice.SupportedDevices;

/**
 * This is a lightweight activity for tests that only require WebView functionality.
 */
public class AwShellActivity extends Activity implements OnRequestPermissionsResultCallback {
    private static final String TAG = "cr.AwShellActivity";
    private static final String PREFERENCES_NAME = "AwShellPrefs";
    private static final String INITIAL_URL = "https://developers.google.com/ar/develop/web/getting-started#examples";
    private static final String LAST_USED_URL_PREFERENCE_NAME = "url";
    private static final int ADF_PERMISSION_ID = 2;
    private static final int CAMERA_ID = 0;
    private static final int MULTIPLE_PERMISSIONS_REQUEST_CODE = 12345;
    private static final String[] PERMISSIONS = new String[] {
        Manifest.permission.CAMERA
        , Manifest.permission.READ_EXTERNAL_STORAGE
        , Manifest.permission.RECORD_AUDIO
        , Manifest.permission.MODIFY_AUDIO_SETTINGS
        , Manifest.permission.ACCESS_COARSE_LOCATION
    };
    private AwBrowserContext mBrowserContext;
    private AwDevToolsServer mDevToolsServer;
    private AwTestContainerView mAwTestContainerView;
    private WebContents mWebContents;
    private NavigationController mNavigationController;
    private EditText mUrlTextView;
    // private ImageButton mPrevButton;
    // private ImageButton mNextButton;
    // private ImageButton mQRCodeButton;
    private ImageButton mBackButton;
    private ImageButton mRefreshButton;
    private boolean mInitialized = false;
    private boolean mAllPermissionsGranted = false;
    private boolean mResumed = false;
    private Tango mTango = null;
    private String mStartupUrl = null;

    private class SpeechRecognition implements RecognitionListener
    {
        public static final String JS_INTERFACE_INSTANCE_NAME = "webarSpeechRecognitionInstance";
        private SpeechRecognizer speechRecognizer = null;
        private Intent speechRecognizerIntent = null;
        private Runnable startRunnable = new Runnable()
        {
            @Override
            public void run()
            {
                speechRecognizer.startListening(speechRecognizerIntent);
            }
        };
        private Runnable stopRunnable = new Runnable()
        {
            @Override
            public void run()
            {
                speechRecognizer.cancel();
            }
        };


        public SpeechRecognition()
        {
            speechRecognizer = SpeechRecognizer.createSpeechRecognizer(AwShellActivity.this);
            speechRecognizerIntent = new Intent(RecognizerIntent.ACTION_RECOGNIZE_SPEECH);
            speechRecognizerIntent.putExtra(RecognizerIntent.EXTRA_LANGUAGE_MODEL, RecognizerIntent.LANGUAGE_MODEL_FREE_FORM);
            speechRecognizerIntent.putExtra(RecognizerIntent.EXTRA_CALLING_PACKAGE, AwShellActivity.this.getPackageName());
            speechRecognizer.setRecognitionListener(this);
        }

        void destroy() 
        {
            speechRecognizer.destroy();
        }

        @Override
        public void onBeginningOfSpeech()
        {               
            // System.out.println("SpeechRecognitionListener.onBeginningOfSpeech");
            dispatchEventToJSInterfaceInstance("speechstart", "{}");
        }

        @Override
        public void onBufferReceived(byte[] buffer)
        {
            // System.out.println("SpeechRecognitionListener.onBufferReceived");
        }

        @Override
        public void onEndOfSpeech()
        {
            // System.out.println("SpeechRecognitionListener.onEndOfSpeech");
            dispatchEventToJSInterfaceInstance("speechend", "{}");
         }

        @Override
        public void onError(int error)
        {
            // System.out.println("SpeechRecognitionListener.onError: " + error);
            String errorString = "Unknown error.";
            switch(error)
            {
                case SpeechRecognizer.ERROR_AUDIO:
                    errorString = "Audio recording error.";
                    break;
                case SpeechRecognizer.ERROR_CLIENT:
                    errorString = "Other client side errors.";
                    break;
                case SpeechRecognizer.ERROR_INSUFFICIENT_PERMISSIONS:
                    errorString = "Insufficient permissions";
                    break;
                case SpeechRecognizer.ERROR_NETWORK:
                    errorString = "Other network related errors.";
                    break;
                case SpeechRecognizer.ERROR_NETWORK_TIMEOUT:
                    errorString = "Network operation timed out.";
                    break;
                case SpeechRecognizer.ERROR_NO_MATCH:
                    errorString = "No recognition result matched.";
                    break;
                case SpeechRecognizer.ERROR_RECOGNIZER_BUSY:
                    errorString = "RecognitionService busy.";
                    break;
                case SpeechRecognizer.ERROR_SERVER:
                    errorString = "Server sends error status.";
                    break;
                case SpeechRecognizer.ERROR_SPEECH_TIMEOUT:
                    errorString = "No speech input or timeout.";
                    break;
            }
            dispatchEventToJSInterfaceInstance("error", "{ error: '" + errorString + "'}");
        }

        @Override
        public void onEvent(int eventType, Bundle params)
        {
            // System.out.println("SpeechRecognitionListener.onEvent");
        }

        @Override
        public void onPartialResults(Bundle partialResults)
        {
            // System.out.println("SpeechRecognitionListener.onPartialResults");
        }

        @Override
        public void onReadyForSpeech(Bundle params)
        {
            // System.out.println("SpeechRecognitionListener.onReadyForSpeech");
            dispatchEventToJSInterfaceInstance("start", 
                "{}");
        }

        @Override
        public void onResults(Bundle results)
        {
            // System.out.println("SpeechRecognitionListener.onResults");
            ArrayList<String> matches = results.getStringArrayList(SpeechRecognizer.RESULTS_RECOGNITION);
            float[] confidences = results.getFloatArray(SpeechRecognizer.CONFIDENCE_SCORES);
            if (!matches.isEmpty())
            {
                try
                {
                    JSONObject resultJSONObject = new JSONObject();
                    JSONArray resultsJSONArray = new JSONArray();
                    for (int i = 0; i < matches.size(); i++)
                    {
                        JSONObject matchJSONObject = new JSONObject();
                        matchJSONObject.put("isFinal", true);
                        matchJSONObject.put("length", 1);
                        JSONObject transcriptJSONObject = new JSONObject();
                        transcriptJSONObject.put("transcript", matches.get(i));
                        transcriptJSONObject.put("confidence", confidences[i]);
                        matchJSONObject.put("0", transcriptJSONObject);
                        resultsJSONArray.put(matchJSONObject);
                    }
                    resultJSONObject.put("results", resultsJSONArray);
                    String jsonString = resultJSONObject.toString();

                    // String jsonString = "{ results: [";
                    // for (int i = 0; i < matches.size(); i++)
                    // {
                    //     jsonString += "{ isFinal: true, length: 1, 0: { transcript: '" + matches.get(i) + "', confidence: " + confidences[i] + " } }" + (i < matches.size() - 1 ? ", " : "");
                    // }
                    // jsonString += "] }";

                    dispatchEventToJSInterfaceInstance("result", jsonString);
                    dispatchEventToJSInterfaceInstance("end", jsonString);
                }
                catch(JSONException e)
                {
                    dispatchEventToJSInterfaceInstance("error", "{ error: 'JSON exception while creating the speech recognition results.'}");
                    e.printStackTrace();
                }
            }
        }

        @Override
        public void onRmsChanged(float rmsdB)
        {
            // System.out.println("SpeechRecognitionListener.onRmsChanged");
        }

        @JavascriptInterface
        public void start()
        {
            runOnUiThread(startRunnable);
        }

        @JavascriptInterface
        public void stop()
        {
            runOnUiThread(stopRunnable);
        }

        private void dispatchEventToJSInterfaceInstance(String eventType, String eventJSONString)
        {
            String jsCode = "window." + JS_INTERFACE_INSTANCE_NAME + ".callEventListeners('" + eventType + "', " + eventJSONString +");";
            mAwTestContainerView.getAwContents().evaluateJavaScript(jsCode, null);
        }
    }

    private static AlertDialog createAlertDialog(Context context, String title,
            String message, DialogInterface.OnClickListener onClickListener,
            int numberOfButtons, String yesButtonText, String noButtonText,
            String cancelButtonText)
    {
        AlertDialog alertDialog = new AlertDialog.Builder(context).create();
        alertDialog.setTitle(title);
        alertDialog.setMessage(message);
        alertDialog.setButton(AlertDialog.BUTTON_POSITIVE, yesButtonText,
                onClickListener);
        if (numberOfButtons > 1)
        {
            alertDialog.setButton(AlertDialog.BUTTON_NEGATIVE, noButtonText,
                    onClickListener);
        }
        if (numberOfButtons > 2)
        {
            alertDialog.setButton(AlertDialog.BUTTON_NEUTRAL, cancelButtonText,
                    onClickListener);
        }
        return alertDialog;
    }    

    private static AlertDialog createPromptDialog(Context context, EditText editText, String title,
            String message, DialogInterface.OnClickListener onClickListener,
            int numberOfButtons, String yesButtonText, String noButtonText,
            String cancelButtonText)
    {
        AlertDialog.Builder builder = new AlertDialog.Builder(context);
        builder.setView(editText);        
        AlertDialog alertDialog = builder.create();
        alertDialog.setTitle(title);
        alertDialog.setMessage(message);
        alertDialog.setButton(AlertDialog.BUTTON_POSITIVE, yesButtonText,
                onClickListener);
        if (numberOfButtons > 1)
        {
            alertDialog.setButton(AlertDialog.BUTTON_NEGATIVE, noButtonText,
                    onClickListener);
        }
        if (numberOfButtons > 2)
        {
            alertDialog.setButton(AlertDialog.BUTTON_NEUTRAL, cancelButtonText,
                    onClickListener);
        }
        return alertDialog;
    }    

    private void saveStringToPreferences(String name, String value) 
    {
        Editor editor = getPreferences(Activity.MODE_PRIVATE).edit();
        editor.putString(name, value);
        if (!editor.commit())
        {
            System.err.println("ERROR: Could not save the preference: " + name + " - " + value);
        }
    }       

    private void requestPermissions() {
        String permissionsString = "";
        for (int i = 0; i < PERMISSIONS.length; i++)
        {
            permissionsString += PERMISSIONS[i];
        }

        if (ContextCompat.checkSelfPermission(this,
               permissionsString)
                != PackageManager.PERMISSION_GRANTED) {

            // if (ActivityCompat.shouldShowRequestPermissionRationale
            //         (this, Manifest.permission.READ_EXTERNAL_STORAGE) ||
            //         ActivityCompat.shouldShowRequestPermissionRationale
            //                 (this, Manifest.permission.CAMERA)) 
            // {
            // } 
            // else 
            {
                requestPermissions(PERMISSIONS, MULTIPLE_PERMISSIONS_REQUEST_CODE);
            }
        } 
        else 
        {
            mAllPermissionsGranted = true;
            tryToConnectWithTango();
        }
    }

    private void requestADFPermission()
    {
        final String EXTRA_KEY_PERMISSIONTYPE = "PERMISSIONTYPE";
        final String EXTRA_VALUE_ADF = "ADF_LOAD_SAVE_PERMISSION";

        Intent intent = new Intent();
        intent.setAction("android.intent.action.REQUEST_TANGO_PERMISSION");
        intent.putExtra(EXTRA_KEY_PERMISSIONTYPE, EXTRA_VALUE_ADF);
        startActivityForResult(intent, ADF_PERMISSION_ID);
    }

   @Override
    public void onRequestPermissionsResult(int requestCode, String[] permissions, int[] grantResults) 
    {
        System.out.println("onRequestPermissionsResult: " + requestCode + ", " + grantResults.length);
        if (requestCode == MULTIPLE_PERMISSIONS_REQUEST_CODE) 
        {
            boolean allPermissionsGranted = true;
            for (int i = 0; allPermissionsGranted && i < grantResults.length; i++)
            {
                allPermissionsGranted &= grantResults[i] == PackageManager.PERMISSION_GRANTED;
            }
            mAllPermissionsGranted = allPermissionsGranted;
            tryToConnectWithTango();
        }
    }

    @Override
    public void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);

        // If the device is not supported, show an alert and quit.
        if (!SupportedDevices.isSupported(this)) 
        {
            createAlertDialog(AwShellActivity.this, "ERROR", "This device is not currently supported by WebARonARCore.", new DialogInterface.OnClickListener()
                {
                    @Override
                    public void onClick(DialogInterface dialog, int which)
                    {
                        AwShellActivity.this.finish();
                    }
                }, 1, "Ok", null, null).show();
            return;
        }

        requestPermissions();
        // requestADFPermission();

        WindowManager windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        Camera.CameraInfo info = new Camera.CameraInfo();
        Camera.getCameraInfo(CAMERA_ID, info);

        TangoJniNative.onCreate(this, display.getRotation(), info.orientation);

        CommandLine.init(new String[] { "chrome", "--ignore-gpu-blacklist", "--enable-webvr", "--enable-blink-features=ScriptedSpeech" });

        AwShellResourceProvider.registerResources(this);

        ContentApplication.initCommandLine(this);
        waitForDebuggerIfNeeded();

        ContextUtils.initApplicationContext(getApplicationContext());
        AwBrowserProcess.loadLibrary();

        if (CommandLine.getInstance().hasSwitch(AwShellSwitches.ENABLE_ATRACE)) {
            Log.e(TAG, "Enabling Android trace.");
            TraceEvent.setATraceEnabled(true);
        }

        setContentView(R.layout.testshell_activity);

        mAwTestContainerView = createAwTestContainerView();

        mWebContents = mAwTestContainerView.getContentViewCore().getWebContents();
        mNavigationController = mWebContents.getNavigationController();
        LinearLayout contentContainer = (LinearLayout) findViewById(R.id.content_container);
        mAwTestContainerView.setLayoutParams(new LinearLayout.LayoutParams(
                LayoutParams.MATCH_PARENT, LayoutParams.MATCH_PARENT, 1f));
        contentContainer.addView(mAwTestContainerView);
        mAwTestContainerView.requestFocus();

        initializeUrlField();
        initializeNavigationButtons();

        mAwTestContainerView.getAwContents().clearCache(true);        

        mAwTestContainerView.getAwContents().addJavascriptInterface(new SpeechRecognition(), SpeechRecognition.JS_INTERFACE_INSTANCE_NAME);

        mStartupUrl = getUrlFromIntent(getIntent());
        if (TextUtils.isEmpty(mStartupUrl)) {
            mStartupUrl = getPreferences(Activity.MODE_PRIVATE).getString(LAST_USED_URL_PREFERENCE_NAME, INITIAL_URL);
        }

        getWindow().addFlags(WindowManager.LayoutParams.FLAG_KEEP_SCREEN_ON);
        
        mInitialized = true;
    }

    private void tryToConnectWithTango()
    {
        if (mInitialized && mResumed && mAllPermissionsGranted)
        {
            mTango = new Tango(this, new Runnable() {
                @Override
                public void run() {
                    TangoJniNative.onTangoServiceConnected(mTango);

                    // Finally load the URL in the main UI thread.
                    AwShellActivity.this.runOnUiThread(new Runnable() {
                        @Override
                        public void run() {
                            // Now we can finally load the URL
                            mAwTestContainerView.getAwContents().loadUrl(mStartupUrl);
                            AwContents.setShouldDownloadFavicons();
                            mUrlTextView.setText(mStartupUrl);
                        }
                    });
                }
            });
        }
    }

    @Override
    protected void onResume()
    {
        super.onResume();

        if (!mInitialized) return;

        mResumed = true;
        tryToConnectWithTango();
    }

    @Override
    protected void onPause()
    {
        super.onPause();

        if (!mInitialized) return;

        mResumed = false;

        if (mTango != null) 
        {
            TangoJniNative.onPause();
        }
    }

    @Override
    public void onDestroy() {
        if (!mInitialized)
        {
            super.onDestroy();
            return;
        }

        TangoJniNative.onDestroy();

        if (mDevToolsServer != null) {
            mDevToolsServer.destroy();
            mDevToolsServer = null;
        }
        super.onDestroy();
    }

    private AwTestContainerView createAwTestContainerView() {
        AwBrowserProcess.start();
        AwTestContainerView testContainerView = new AwTestContainerView(this, true);
        AwContentsClient awContentsClient = new NullContentsClient() {
            private View mCustomView;

            @Override
            public void handleJsAlert(String url, String message, final JsResultReceiver receiver) {
                createAlertDialog(AwShellActivity.this, url, message, new DialogInterface.OnClickListener()
                {
                    @Override
                    public void onClick(DialogInterface dialog, int which)
                    {
                        receiver.confirm();
                    }
                }, 1, "Ok", null, null).show();                
            }

            @Override
            public void handleJsPrompt(String url, String message, String defaultValue, final JsPromptResultReceiver receiver) {
                final EditText editText = new EditText(AwShellActivity.this);
                createPromptDialog(AwShellActivity.this, editText, url, message, new DialogInterface.OnClickListener()
                {
                    @Override
                    public void onClick(DialogInterface dialog, int which)
                    {
                        if (which == DialogInterface.BUTTON_POSITIVE)
                        {
                            receiver.confirm(editText.getText().toString());
                        }
                        else 
                        {
                            receiver.cancel();
                        }
                    }
                }, 2, "Ok", "Cancel", null).show();                
            }

            @Override
            public void handleJsConfirm(String url, String message, final JsResultReceiver receiver) {
                createAlertDialog(AwShellActivity.this, url, message, new DialogInterface.OnClickListener()
                {
                    @Override
                    public void onClick(DialogInterface dialog, int which)
                    {
                        if (which == DialogInterface.BUTTON_POSITIVE) 
                        {
                            receiver.confirm();
                        }
                        else 
                        {
                            receiver.cancel();
                        }

                    }
                }, 2, "Yes", "No", null).show();                
            }

            @Override
            public void onReceivedError(int errorCode, String description, String failingUrl) { 
                createAlertDialog(AwShellActivity.this, "ERROR: " + errorCode, failingUrl + ": " + description, null, 1, "Ok", null, null).show();                
            }

            // @Override
            // public void onReceivedError2(AwWebResourceRequest request, AwWebResourceError error) {
            //     String failingUrl = request.url;
            //     int errorCode = error.errorCode;
            //     String description = error.description;
            //     createAlertDialog(AwShellActivity.this, "ERROR: " + errorCode, failingUrl + ": " + description, null, 1, "Ok", null, null).show();                
            // }

            @Override
            public void onReceivedHttpError(AwWebResourceRequest request, AwWebResourceResponse response) {
                String failingUrl = request.url;
                // HACK! Do not show the icon loading error.
                if (failingUrl.toLowerCase(Locale.getDefault()).contains("favicon.ico")) return;
                int errorCode = response.getStatusCode();
                String description = response.getReasonPhrase();
                createAlertDialog(AwShellActivity.this, "HTTP ERROR: " + errorCode, failingUrl + ": " + description, null, 1, "Ok", null, null).show();                
            }

            @Override
            public void onPageStarted(String url) {
                if (mUrlTextView != null) {
                    mUrlTextView.setText(url);
                    saveStringToPreferences(LAST_USED_URL_PREFERENCE_NAME, url);
                }
                TangoJniNative.resetPose();
            }

            @Override
            public void onShowCustomView(View view, WebChromeClient.CustomViewCallback callback) {
                System.out.println("WebAR: onShowCustomView");
                getWindow().setFlags(
                        WindowManager.LayoutParams.FLAG_FULLSCREEN,
                        WindowManager.LayoutParams.FLAG_FULLSCREEN);

                getWindow().addContentView(view,
                        new FrameLayout.LayoutParams(
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                ViewGroup.LayoutParams.MATCH_PARENT,
                                Gravity.CENTER));
                mCustomView = view;
            }

            @Override
            public void onHideCustomView() {
                getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
                FrameLayout decorView = (FrameLayout) getWindow().getDecorView();
                decorView.removeView(mCustomView);
                mCustomView = null;
            }

            @Override
            public boolean shouldOverrideKeyEvent(KeyEvent event) {
                if (event.getKeyCode() == KeyEvent.KEYCODE_BACK) {
                    return true;
                }
                return false;
            }

            @Override
            public void onGeolocationPermissionsShowPrompt(String origin,
                    GeolocationPermissions.Callback callback) {
                callback.invoke(origin, true, true);
                // callback.invoke(origin, false, false);
            }

            @Override
            public void onGeolocationPermissionsHidePrompt() {
            }

            @Override
            public void onPermissionRequest(AwPermissionRequest awPermissionRequest) {
                // awPermissionRequest.deny();
                awPermissionRequest.grant();
            }

            @Override
            public void onPermissionRequestCanceled(AwPermissionRequest awPermissionRequest) {
            }
        };

        SharedPreferences sharedPreferences =
                getSharedPreferences(PREFERENCES_NAME, Context.MODE_PRIVATE);
        if (mBrowserContext == null) {
            mBrowserContext = new AwBrowserContext(sharedPreferences, getApplicationContext());
        }
        final AwSettings awSettings = new AwSettings(this /* context */,
                false /* isAccessFromFileURLsGrantedByDefault */, false /* supportsLegacyQuirks */,
                false /* allowEmptyDocumentPersistence */,
                true /* allowGeolocationOnInsecureOrigins */);
        // Required for WebGL conformance tests.
        awSettings.setMediaPlaybackRequiresUserGesture(false);
        // Allow zoom and fit contents to screen
        awSettings.setBuiltInZoomControls(false);
        awSettings.setDisplayZoomControls(false);
        awSettings.setUseWideViewPort(true);
        awSettings.setLoadWithOverviewMode(true);
        awSettings.setAllowFileAccessFromFileURLs(true);
        awSettings.setAllowUniversalAccessFromFileURLs(true);
        awSettings.setDomStorageEnabled(true);
        awSettings.setLayoutAlgorithm(android.webkit.WebSettings.LayoutAlgorithm.TEXT_AUTOSIZING);

        testContainerView.initialize(new AwContents(mBrowserContext, testContainerView,
                testContainerView.getContext(), testContainerView.getInternalAccessDelegate(),
                testContainerView.getNativeDrawGLFunctorFactory(), awContentsClient, awSettings));
        testContainerView.getAwContents().getSettings().setJavaScriptEnabled(true);
        if (mDevToolsServer == null) {
            mDevToolsServer = new AwDevToolsServer();
            mDevToolsServer.setRemoteDebuggingEnabled(true);
        }
        return testContainerView;
    }

    private static String getUrlFromIntent(Intent intent) {
        return intent != null ? intent.getDataString() : null;
    }

    private void setKeyboardVisibilityForUrl(boolean visible) {
        InputMethodManager imm = (InputMethodManager) getSystemService(
                Context.INPUT_METHOD_SERVICE);
        if (visible) {
            imm.showSoftInput(mUrlTextView, InputMethodManager.SHOW_IMPLICIT);
        } else {
            imm.hideSoftInputFromWindow(mUrlTextView.getWindowToken(), 0);
        }
    }

    private void initializeUrlField() {
        mUrlTextView = (EditText) findViewById(R.id.url);
        mUrlTextView.setOnEditorActionListener(new OnEditorActionListener() {
            @Override
            public boolean onEditorAction(TextView v, int actionId, KeyEvent event) {
                if ((actionId != EditorInfo.IME_ACTION_GO) && (event == null
                        || event.getKeyCode() != KeyEvent.KEYCODE_ENTER
                        || event.getAction() != KeyEvent.ACTION_DOWN)) {
                    return false;
                }

                String url = mUrlTextView.getText().toString();
                try {
                    URI uri = new URI(url);
                    if (uri.getScheme() == null) {
                        url = "http://" + uri.toString();
                    } else {
                        url = uri.toString();
                    }
                } catch (URISyntaxException e) {
                    // Ignore syntax errors.
                }
                mAwTestContainerView.getAwContents().loadUrl(url);
                mUrlTextView.clearFocus();
                setKeyboardVisibilityForUrl(false);
                mAwTestContainerView.requestFocus();
                return true;
            }
        });
        mUrlTextView.setOnFocusChangeListener(new OnFocusChangeListener() {
            @Override
            public void onFocusChange(View v, boolean hasFocus) {
                setKeyboardVisibilityForUrl(hasFocus);
                // mNextButton.setVisibility(hasFocus ? View.GONE : View.VISIBLE);
                // mPrevButton.setVisibility(hasFocus ? View.GONE : View.VISIBLE);
                if (!hasFocus) {
                    mUrlTextView.setText(mWebContents.getUrl());
                }
            }
        });
    }

    private void initializeNavigationButtons() {
        // mPrevButton = (ImageButton) findViewById(R.id.prev);
        // mPrevButton.setOnClickListener(new OnClickListener() {
        //     @Override
        //     public void onClick(View v) {
        //         if (mNavigationController.canGoBack()) {
        //             mNavigationController.goBack();
        //         }
        //     }
        // });

        // mNextButton = (ImageButton) findViewById(R.id.next);
        // mNextButton.setOnClickListener(new OnClickListener() {
        //     @Override
        //     public void onClick(View v) {
        //         if (mNavigationController.canGoForward()) {
        //             mNavigationController.goForward();
        //         }
        //     }
        // });

        // mQRCodeButton = (ImageButton) findViewById(R.id.qrcodeImageButton);
        // mQRCodeButton.setOnClickListener(new OnClickListener() {
        //     @Override
        //     public void onClick(View v) {
        //         IntentIntegrator intentIntegrator = new IntentIntegrator(AwShellActivity.this);
        //         intentIntegrator.initiateScan();
        //     }
        // });

        mBackButton = (ImageButton) findViewById(R.id.backImageButton);
        mBackButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                if (mNavigationController.canGoBack()) {
                    mNavigationController.goBack();
                }
            }
        });

        mRefreshButton = (ImageButton) findViewById(R.id.refreshImageButton);
        mRefreshButton.setOnClickListener(new OnClickListener() {
            @Override
            public void onClick(View v) {
                mNavigationController.reload(true);
            }
        });
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK) {
            if (mNavigationController.canGoBack()) {
                mNavigationController.goBack();
                return true;
            }
        }

        return super.onKeyUp(keyCode, event);
    }

    @Override
    public void onConfigurationChanged(Configuration newConfig) {
        super.onConfigurationChanged(newConfig);

        WindowManager windowManager = (WindowManager) getSystemService(WINDOW_SERVICE);
        Display display = windowManager.getDefaultDisplay();
        Camera.CameraInfo info = new Camera.CameraInfo();
        Camera.getCameraInfo(CAMERA_ID, info);

        TangoJniNative.onConfigurationChanged(display.getRotation(), info.orientation);
    }

    @Override
    public void onActivityResult(int requestCode, int resultCode, Intent intent) 
    {
        IntentResult scanResult = IntentIntegrator.parseActivityResult(requestCode, resultCode, intent);
        if (scanResult != null && scanResult.getContents() != null) 
        {
            String url = scanResult.getContents();
            try
            {
                new URL(url);
                mUrlTextView.setText(url);
                mAwTestContainerView.getAwContents().loadUrl(url);
                mUrlTextView.clearFocus();
                setKeyboardVisibilityForUrl(false);
                mAwTestContainerView.requestFocus();
            }
            catch(MalformedURLException e)
            {               
                createAlertDialog(this, "Not an URL", "The read QRCode does not represent a valid URL.", null, 1, "Ok", null, null).show();
            }
        }
    }

/*
    boolean firstTouchDone = false;

    @Override 
    public boolean dispatchTouchEvent( android.view.MotionEvent event )
    {
        android.view.Display display = getWindowManager().getDefaultDisplay(); 
        int width = display.getWidth();  // deprecated
        int height = display.getHeight();  // deprecated
        int action = event.getAction();
        float x = event.getRawX();
        float y = event.getRawY();

        System.out.println("x = " + x  + ", width / 2 = " + width / 2);

        if (!firstTouchDone && x > (width >> 1))
        {
//            mAwTestContainerView.getAwContents().loadUrl("http://judax.github.io/tango/");
            mAwTestContainerView.getAwContents().loadUrl("http://192.168.0.104/judax/tango/video3js/");
            mUrlTextView.clearFocus();
            setKeyboardVisibilityForUrl(false);
            mAwTestContainerView.requestFocus();
            firstTouchDone = true;
            return true;
        }
        else
        {
            return super.dispatchTouchEvent(event);
        }
    }
*/

    private void waitForDebuggerIfNeeded() {
        if (CommandLine.getInstance().hasSwitch(BaseSwitches.WAIT_FOR_JAVA_DEBUGGER)) {
            Log.e(TAG, "Waiting for Java debugger to connect...");
            android.os.Debug.waitForDebugger();
            Log.e(TAG, "Java debugger connected. Resuming execution.");
        }
    }
}
