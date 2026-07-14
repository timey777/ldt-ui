package com.example.pureopengl;

import android.app.Activity;
import android.graphics.Color;
import android.graphics.drawable.ColorDrawable;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.content.res.AssetManager;
import android.os.Build;
import android.util.DisplayMetrics;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import java.lang.ref.WeakReference;

public class RendererLib {
    private static Context appContext;
    private static WeakReference<Activity> activityRef = new WeakReference<>(null);
    private static WeakReference<MyGLSurfaceView> inputViewRef = new WeakReference<>(null);
    private static boolean useStatusBarDisplayArea = false;
    private static boolean showStatusBarInfo = true;
    private static int windowBackgroundColor = Color.WHITE;

    static {
        System.loadLibrary("native-lib");
    }

    public static void initializeContext(Context context) {
        appContext = context.getApplicationContext();
    }

    public static String getAppVersionLabel() {
        if (appContext == null) {
            return "v?";
        }

        try {
            PackageInfo packageInfo = appContext
                    .getPackageManager()
                    .getPackageInfo(appContext.getPackageName(), 0);
            long versionCode = Build.VERSION.SDK_INT >= Build.VERSION_CODES.P
                    ? packageInfo.getLongVersionCode()
                    : packageInfo.versionCode;
            String versionName = packageInfo.versionName != null ? packageInfo.versionName : "?";
            return "v" + versionName + " (" + versionCode + ")";
        } catch (Exception ignored) {
            return "v?";
        }
    }

    private static void rememberActivity(Activity activity) {
        if (activity != null) {
            activityRef = new WeakReference<>(activity);
        }
    }

    private static Activity resolveActivity() {
        final Activity rememberedActivity = activityRef.get();
        if (rememberedActivity != null) {
            return rememberedActivity;
        }

        if (appContext instanceof Activity) {
            return (Activity) appContext;
        }

        final MyGLSurfaceView view = inputViewRef.get();
        if (view != null && view.getContext() instanceof Activity) {
            final Activity activity = (Activity) view.getContext();
            rememberActivity(activity);
            return activity;
        }

        return null;
    }

    private static void requestViewportSync() {
        final MyGLSurfaceView view = inputViewRef.get();
        if (view == null) {
            return;
        }

        view.requestApplyInsets();
        view.requestLayout();
        view.post(() -> {
            final int width = view.getWidth();
            final int height = view.getHeight();
            if (width > 0 && height > 0) {
                final float viewportScale = getViewportScale(view.getContext());
                view.queueEvent(() -> RendererLib.resize(width, height, viewportScale, viewportScale));
            }
        });
    }

    private static float getViewportScale(Context context) {
        if (context == null) {
            return 1.0f;
        }

        final DisplayMetrics metrics = context.getResources().getDisplayMetrics();
        if (metrics == null || metrics.density <= 0.0f) {
            return 1.0f;
        }
        return metrics.density;
    }

    private static boolean shouldUseDarkStatusBarIcons() {
        final int red = Color.red(windowBackgroundColor);
        final int green = Color.green(windowBackgroundColor);
        final int blue = Color.blue(windowBackgroundColor);
        final double luminance = (0.299 * red) + (0.587 * green) + (0.114 * blue);
        return luminance >= 186.0;
    }

    private static void applyWindowMode(Activity activity) {
        if (activity == null) {
            return;
        }
        rememberActivity(activity);

        final Window window = activity.getWindow();
        if (window == null) {
            return;
        }

        window.clearFlags(WindowManager.LayoutParams.FLAG_TRANSLUCENT_STATUS);
        window.addFlags(WindowManager.LayoutParams.FLAG_DRAWS_SYSTEM_BAR_BACKGROUNDS);
        window.clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        window.setBackgroundDrawable(new ColorDrawable(windowBackgroundColor));
        window.setStatusBarColor(
            useStatusBarDisplayArea ? Color.TRANSPARENT : windowBackgroundColor);

        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            WindowManager.LayoutParams attributes = window.getAttributes();
            attributes.layoutInDisplayCutoutMode =
                    useStatusBarDisplayArea
                            ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES
                            : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_DEFAULT;
            window.setAttributes(attributes);
        }

        final View decorView = window.getDecorView();
        final boolean useDarkStatusBarIcons = shouldUseDarkStatusBarIcons();
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.R) {
            window.setDecorFitsSystemWindows(!useStatusBarDisplayArea);
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.setSystemBarsBehavior(
                        WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                int appearance = 0;
                if (showStatusBarInfo && useDarkStatusBarIcons) {
                    appearance |= WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS;
                }
                controller.setSystemBarsAppearance(
                        appearance,
                        WindowInsetsController.APPEARANCE_LIGHT_STATUS_BARS);
                if (showStatusBarInfo) {
                    controller.show(WindowInsets.Type.statusBars());
                } else {
                    controller.hide(WindowInsets.Type.statusBars());
                }
            }
        } else {
            int visibility = View.SYSTEM_UI_FLAG_LAYOUT_STABLE;
            if (useStatusBarDisplayArea) {
                visibility |= View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN;
            }
            if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.M && showStatusBarInfo && useDarkStatusBarIcons) {
                visibility |= View.SYSTEM_UI_FLAG_LIGHT_STATUS_BAR;
            }
            if (!showStatusBarInfo) {
                visibility |= View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY;
            }
            decorView.setSystemUiVisibility(visibility);
        }
    }

    public static void configureEdgeToEdge(Activity activity, boolean fullscreen) {
        useStatusBarDisplayArea = true;
        showStatusBarInfo = !fullscreen;
        applyWindowMode(activity);
    }

    public static void setWindowBackgroundColor(int color) {
        windowBackgroundColor = color;
        final Activity activity = resolveActivity();
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> applyWindowMode(activity));
    }

    public static void setUseStatusBarDisplayArea(boolean enabled) {
        useStatusBarDisplayArea = enabled;
        final Activity activity = resolveActivity();
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> {
            applyWindowMode(activity);
            requestViewportSync();
        });
    }

    public static void setStatusBarInfoVisible(boolean visible) {
        showStatusBarInfo = visible;
        final Activity activity = resolveActivity();
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> {
            applyWindowMode(activity);
            requestViewportSync();
        });
    }

    public static void toggleFullscreen() {
        final boolean nextUseStatusBarDisplayArea = !useStatusBarDisplayArea;
        final boolean nextShowStatusBarInfo = nextUseStatusBarDisplayArea ? showStatusBarInfo : true;
        applyWindowDisplayMode(nextUseStatusBarDisplayArea, nextShowStatusBarInfo);
    }

    public static void applyWindowDisplayMode(boolean fullscreenLayout, boolean visibleStatusBarInfo) {
        useStatusBarDisplayArea = fullscreenLayout;
        showStatusBarInfo = visibleStatusBarInfo;
        final Activity activity = resolveActivity();
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> {
            applyWindowMode(activity);
            requestViewportSync();
        });
    }

    public static void applyWindowDisplayMode(Activity activity, boolean fullscreenLayout, boolean visibleStatusBarInfo) {
        useStatusBarDisplayArea = fullscreenLayout;
        showStatusBarInfo = visibleStatusBarInfo;
        if (activity == null) {
            return;
        }
        activity.runOnUiThread(() -> applyWindowMode(activity));
    }

    public static void registerInputView(MyGLSurfaceView view) {
        inputViewRef = new WeakReference<>(view);
    }

    public static void setClipboardText(String text) {
        if (appContext == null) {
            return;
        }
        ClipboardManager clipboardManager =
                (ClipboardManager) appContext.getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboardManager == null) {
            return;
        }
        clipboardManager.setPrimaryClip(ClipData.newPlainText("ldt", text));
    }

    public static String getClipboardText() {
        if (appContext == null) {
            return "";
        }
        ClipboardManager clipboardManager =
                (ClipboardManager) appContext.getSystemService(Context.CLIPBOARD_SERVICE);
        if (clipboardManager == null || !clipboardManager.hasPrimaryClip()) {
            return "";
        }
        ClipData clipData = clipboardManager.getPrimaryClip();
        if (clipData == null || clipData.getItemCount() == 0) {
            return "";
        }
        CharSequence text = clipData.getItemAt(0).coerceToText(appContext);
        return text != null ? text.toString() : "";
    }

    public static void hideSoftKeyboard(android.view.View view) {
        if (appContext == null || view == null) {
            return;
        }
        android.view.inputmethod.InputMethodManager imm =
                (android.view.inputmethod.InputMethodManager) appContext.getSystemService(Context.INPUT_METHOD_SERVICE);
        if (imm != null) {
            imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
        }
    }

    public static void syncSoftInputState(boolean active, boolean multiline) {
        final MyGLSurfaceView view = inputViewRef.get();
        if (view == null) {
            return;
        }

        view.post(() -> {
            view.setNativeTextInputState(active, multiline);

            android.view.inputmethod.InputMethodManager imm =
                    (android.view.inputmethod.InputMethodManager) view.getContext().getSystemService(Context.INPUT_METHOD_SERVICE);
            if (imm == null) {
                return;
            }

            imm.restartInput(view);
            if (active) {
                if (!view.hasFocus()) {
                    view.requestFocus();
                }
                imm.showSoftInput(view, android.view.inputmethod.InputMethodManager.SHOW_IMPLICIT);
            } else {
                imm.hideSoftInputFromWindow(view.getWindowToken(), 0);
            }
        });
    }

    public static native void configurePlatform(AssetManager assetManager, String filesDir);
    public static native void setDslSource(String dslSource);
    public static native void setDslSourceFromAsset(String assetPath);
    public static native void init();
    public static native void onSurfaceCreated();
    public static native void onPause();
    public static native void onResume();
    public static native void resize(int width, int height, float scaleX, float scaleY);
    public static native void step();
    public static native void clearPointerState();
    public static native void onTouch(int pointerId, int action, float x, float y);
    public static native void onScroll(float dx, float dy);
    public static native void onKey(int keyCode, int action, int metaState, int unicodeCodePoint);
    public static native boolean isTextInputActive();
    public static native boolean isTextInputMultiline();
    public static native boolean handleBackPressed();
}
