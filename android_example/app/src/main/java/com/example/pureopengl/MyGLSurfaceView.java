package com.example.pureopengl;

import android.content.Context;
import android.opengl.GLSurfaceView;
import android.text.InputType;
import android.util.AttributeSet;
import android.util.Log;
import android.view.KeyEvent;
import android.view.MotionEvent;
import android.view.ViewConfiguration;
import android.view.inputmethod.BaseInputConnection;
import android.view.inputmethod.EditorInfo;
import android.view.inputmethod.InputConnection;

import javax.microedition.khronos.egl.EGLConfig;
import javax.microedition.khronos.opengles.GL10;

public class MyGLSurfaceView extends GLSurfaceView {
    private static final String TAG = "LDTTouch";
    private final float touchSlop;
    private float lastTouchX;
    private float lastTouchY;
    private float gestureStartX;
    private float gestureStartY;
    private boolean scrollGestureActive;
    private boolean nativeTextInputActive;
    private boolean nativeMultilineTextInput;

    public MyGLSurfaceView(Context context) {
        super(context);
        touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        initView();
    }

    public MyGLSurfaceView(Context context, AttributeSet attrs) {
        super(context, attrs);
        touchSlop = ViewConfiguration.get(context).getScaledTouchSlop();
        initView();
    }

    private void initView() {
        setEGLContextClientVersion(3);
        setPreserveEGLContextOnPause(true);
        setRenderer(new Renderer());
        setFocusable(true);
        setFocusableInTouchMode(true);
        requestFocus();
        RendererLib.registerInputView(this);
    }

    @Override
    protected void onAttachedToWindow() {
        super.onAttachedToWindow();
        RendererLib.registerInputView(this);
    }

    @Override
    protected void onDetachedFromWindow() {
        RendererLib.registerInputView(null);
        super.onDetachedFromWindow();
    }

    @Override
    public void onWindowFocusChanged(boolean hasWindowFocus) {
        super.onWindowFocusChanged(hasWindowFocus);
        if (!hasWindowFocus) {
            Log.d(TAG, "window focus lost -> clearPointerState");
            queueEvent(RendererLib::clearPointerState);
        }
    }

    private static String actionToString(int action) {
        switch (action) {
            case MotionEvent.ACTION_DOWN:
                return "DOWN";
            case MotionEvent.ACTION_UP:
                return "UP";
            case MotionEvent.ACTION_MOVE:
                return "MOVE";
            case MotionEvent.ACTION_CANCEL:
                return "CANCEL";
            case MotionEvent.ACTION_POINTER_DOWN:
                return "POINTER_DOWN";
            case MotionEvent.ACTION_POINTER_UP:
                return "POINTER_UP";
            default:
                return "UNKNOWN(" + action + ")";
        }
    }

    private void updateInputMethodVisibility() {
        RendererLib.syncSoftInputState(
                RendererLib.isTextInputActive(),
                RendererLib.isTextInputMultiline());
    }

    void setNativeTextInputState(boolean active, boolean multiline) {
        nativeTextInputActive = active;
        nativeMultilineTextInput = multiline;
    }

    private void queueTouchEvent(int pointerId, int action, float x, float y) {
        queueEvent(() -> RendererLib.onTouch(pointerId, action, x, y));
    }

    private void queueScrollEvent(float dx, float dy) {
        queueEvent(() -> RendererLib.onScroll(dx, dy));
    }

    private void queueKeyEvent(int keyCode, int action, int metaState, int unicodeCodePoint) {
        queueEvent(() -> RendererLib.onKey(keyCode, action, metaState, unicodeCodePoint));
    }

    @Override
    public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        final int actionIndex = event.getActionIndex();
        final float x = event.getX(actionIndex);
        final float y = event.getY(actionIndex);

        switch (action) {
            case MotionEvent.ACTION_DOWN:
                lastTouchX = x;
                lastTouchY = y;
                gestureStartX = x;
                gestureStartY = y;
                scrollGestureActive = false;
                Log.d(TAG, "java touch action=" + actionToString(action) + " pointer=" + event.getPointerId(actionIndex) + " x=" + x + " y=" + y);
                queueTouchEvent(event.getPointerId(actionIndex), action, x, y);
                break;
            case MotionEvent.ACTION_POINTER_DOWN:
            case MotionEvent.ACTION_UP:
            case MotionEvent.ACTION_POINTER_UP: {
                final int pointerId = event.getPointerId(actionIndex);
                final int forwardedAction = scrollGestureActive &&
                        (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP)
                        ? MotionEvent.ACTION_CANCEL
                        : action;
                Log.d(TAG, "java touch action=" + actionToString(action) + " forwarded=" + actionToString(forwardedAction) + " pointer=" + pointerId + " x=" + x + " y=" + y + " scrollGestureActive=" + scrollGestureActive);
                queueTouchEvent(pointerId, forwardedAction, x, y);
                if (action == MotionEvent.ACTION_UP || action == MotionEvent.ACTION_POINTER_UP) {
                    scrollGestureActive = false;
                }
                break;
            }
            case MotionEvent.ACTION_MOVE:
                if (event.getPointerCount() == 1) {
                    final float dx = x - lastTouchX;
                    final float dy = y - lastTouchY;
                    final float totalDx = x - gestureStartX;
                    final float totalDy = y - gestureStartY;
                    for (int i = 0; i < event.getPointerCount(); i++) {
                        queueTouchEvent(event.getPointerId(i), action, event.getX(i), event.getY(i));
                    }
                    if (scrollGestureActive || Math.abs(totalDx) > touchSlop || Math.abs(totalDy) > touchSlop) {
                        scrollGestureActive = true;
                        queueScrollEvent(dx / 20.0f, dy / 20.0f);
                    }
                    lastTouchX = x;
                    lastTouchY = y;
                } else {
                    for (int i = 0; i < event.getPointerCount(); i++) {
                        queueTouchEvent(event.getPointerId(i), action, event.getX(i), event.getY(i));
                    }
                }
                break;
            case MotionEvent.ACTION_CANCEL:
                Log.d(TAG, "java touch action=" + actionToString(action) + " pointer=-1 x=0.0 y=0.0");
                queueTouchEvent(-1, action, 0.0f, 0.0f);
                scrollGestureActive = false;
                gestureStartX = 0.0f;
                gestureStartY = 0.0f;
                break;
            default:
                break;
        }

        if (!hasFocus()) {
            requestFocus();
        }
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            updateInputMethodVisibility();
        }
        return true;
    }

    @Override
    public boolean onKeyPreIme(int keyCode, KeyEvent event) {
        if (keyCode == KeyEvent.KEYCODE_BACK && event.getAction() == KeyEvent.ACTION_UP) {
            if (RendererLib.handleBackPressed()) {
                RendererLib.hideSoftKeyboard(this);
                clearFocus();
                return true;
            }
        }
        return super.onKeyPreIme(keyCode, event);
    }

    @Override
    public boolean onCheckIsTextEditor() {
        return nativeTextInputActive;
    }

    @Override
    public InputConnection onCreateInputConnection(EditorInfo outAttrs) {
        if (!nativeTextInputActive) {
            return null;
        }
        outAttrs.inputType = InputType.TYPE_CLASS_TEXT;
        outAttrs.imeOptions = EditorInfo.IME_FLAG_NO_EXTRACT_UI;
        if (nativeMultilineTextInput) {
            outAttrs.inputType |= InputType.TYPE_TEXT_FLAG_MULTI_LINE;
            outAttrs.imeOptions |= EditorInfo.IME_FLAG_NO_ENTER_ACTION;
        } else {
            outAttrs.imeOptions |= EditorInfo.IME_ACTION_DONE;
        }
        return new LdtInputConnection(this, true);
    }

    @Override
    public boolean onKeyDown(int keyCode, KeyEvent event) {
        queueKeyEvent(keyCode, KeyEvent.ACTION_DOWN, event.getMetaState(), event.getUnicodeChar());
        return true;
    }

    @Override
    public boolean onKeyUp(int keyCode, KeyEvent event) {
        queueKeyEvent(keyCode, KeyEvent.ACTION_UP, event.getMetaState(), event.getUnicodeChar());
        return true;
    }

    @Override
    public boolean onKeyMultiple(int keyCode, int repeatCount, KeyEvent event) {
        if (event != null) {
            final String chars = event.getCharacters();
            if (chars != null && !chars.isEmpty()) {
                for (int i = 0; i < chars.length(); ) {
                    final int codePoint = chars.codePointAt(i);
                    queueKeyEvent(keyCode, KeyEvent.ACTION_MULTIPLE, event.getMetaState(), codePoint);
                    i += Character.charCount(codePoint);
                }
                return true;
            }
        }
        return super.onKeyMultiple(keyCode, repeatCount, event);
    }

    private static final class LdtInputConnection extends BaseInputConnection {
        private final MyGLSurfaceView targetView;

        LdtInputConnection(MyGLSurfaceView targetView, boolean fullEditor) {
            super(targetView, fullEditor);
            this.targetView = targetView;
        }

        @Override
        public boolean commitText(CharSequence text, int newCursorPosition) {
            if (text != null) {
                for (int i = 0; i < text.length(); ) {
                    final int codePoint = Character.codePointAt(text, i);
                    targetView.queueKeyEvent(
                            KeyEvent.KEYCODE_UNKNOWN,
                            KeyEvent.ACTION_MULTIPLE,
                            0,
                            codePoint);
                    i += Character.charCount(codePoint);
                }
            }
            return true;
        }

        @Override
        public boolean sendKeyEvent(KeyEvent event) {
            targetView.queueKeyEvent(
                    event.getKeyCode(),
                    event.getAction(),
                    event.getMetaState(),
                    event.getUnicodeChar());
            return true;
        }

        @Override
        public boolean deleteSurroundingText(int beforeLength, int afterLength) {
            if (beforeLength > 0) {
                targetView.queueKeyEvent(KeyEvent.KEYCODE_DEL, KeyEvent.ACTION_DOWN, 0, 0);
                targetView.queueKeyEvent(KeyEvent.KEYCODE_DEL, KeyEvent.ACTION_UP, 0, 0);
            }
            if (afterLength > 0) {
                targetView.queueKeyEvent(KeyEvent.KEYCODE_FORWARD_DEL, KeyEvent.ACTION_DOWN, 0, 0);
                targetView.queueKeyEvent(KeyEvent.KEYCODE_FORWARD_DEL, KeyEvent.ACTION_UP, 0, 0);
            }
            return true;
        }

        @Override
        public boolean performEditorAction(int actionCode) {
            if (actionCode == EditorInfo.IME_ACTION_DONE ||
                    actionCode == EditorInfo.IME_ACTION_GO ||
                    actionCode == EditorInfo.IME_ACTION_NEXT ||
                    actionCode == EditorInfo.IME_ACTION_SEARCH ||
                    actionCode == EditorInfo.IME_ACTION_SEND) {
                targetView.queueKeyEvent(KeyEvent.KEYCODE_ENTER, KeyEvent.ACTION_DOWN, 0, 0);
                targetView.queueKeyEvent(KeyEvent.KEYCODE_ENTER, KeyEvent.ACTION_UP, 0, 0);
                return true;
            }
            return super.performEditorAction(actionCode);
        }
    }

    private class Renderer implements GLSurfaceView.Renderer {
        @Override
        public void onDrawFrame(GL10 gl) {
            RendererLib.step();
        }

        @Override
        public void onSurfaceChanged(GL10 gl, int width, int height) {
            final float viewportScale = MyGLSurfaceView.this.getResources().getDisplayMetrics().density > 0.0f
                ? MyGLSurfaceView.this.getResources().getDisplayMetrics().density
                    : 1.0f;
            RendererLib.resize(width, height, viewportScale, viewportScale);
        }

        @Override
        public void onSurfaceCreated(GL10 gl, EGLConfig config) {
            RendererLib.onSurfaceCreated();
        }
    }
}
