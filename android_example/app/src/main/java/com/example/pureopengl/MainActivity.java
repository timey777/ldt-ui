package com.example.pureopengl;

import android.app.Activity;
import android.os.Bundle;

public class MainActivity extends Activity {
    private MyGLSurfaceView mView;

    @Override protected void onCreate(Bundle icicle) {
        super.onCreate(Bundle.EMPTY);
        RendererLib.applyWindowDisplayMode(this, false, true);

        RendererLib.initializeContext(this);
        RendererLib.configurePlatform(getAssets(), getFilesDir().getAbsolutePath());
        RendererLib.setDslSourceFromAsset("main.ldt");
        mView = new MyGLSurfaceView(this);
        setContentView(mView);
    }

    @Override protected void onPause() {
        super.onPause();
        RendererLib.onPause();
        mView.onPause();
    }

    @Override protected void onResume() {
        super.onResume();
        RendererLib.onResume();
        mView.onResume();
    }
}
