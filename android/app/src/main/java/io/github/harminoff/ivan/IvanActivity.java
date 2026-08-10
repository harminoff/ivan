package io.github.harminoff.ivan;

import android.os.Bundle;
import android.system.Os;
import android.util.Log;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;

import org.libsdl.app.SDLActivity;

import java.io.File;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;

public final class IvanActivity extends SDLActivity {
    private static final String TAG = "IVAN";
    private static final String CONTENT_VERSION = "0.59-de528ac-android-2";
    private boolean statusBarHidden;

    private static native void nativeSetSafeInsets(int left, int top, int right, int bottom, float density);

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        installPackagedContent();
        File dataRoot = new File(getFilesDir(), "data/" + CONTENT_VERSION);
        File userRoot = new File(getFilesDir(), "user");
        if (!userRoot.exists() && !userRoot.mkdirs()) {
            Log.w(TAG, "Could not create user directory " + userRoot);
        }
        try {
            Os.setenv("IVAN_DATA_DIR", dataRoot.getAbsolutePath() + "/", true);
            Os.setenv("IVAN_USER_DIR", userRoot.getAbsolutePath() + "/", true);
            android.util.DisplayMetrics metrics = getResources().getDisplayMetrics();
            Os.setenv("IVAN_SCREEN_WIDTH", Integer.toString(metrics.widthPixels), true);
            Os.setenv("IVAN_SCREEN_HEIGHT", Integer.toString(metrics.heightPixels), true);
        } catch (android.system.ErrnoException error) {
            throw new IllegalStateException("Unable to configure IVAN storage paths", error);
        }

        super.onCreate(savedInstanceState);

        getWindow().getDecorView().setOnApplyWindowInsetsListener((view, insets) -> {
            int left = 0;
            int top = 0;
            int right = 0;
            int bottom = 0;
            if (android.os.Build.VERSION.SDK_INT >= 30) {
                android.graphics.Insets safe = insets.getInsets(
                        WindowInsets.Type.systemBars() | WindowInsets.Type.displayCutout());
                left = safe.left;
                top = safe.top;
                right = safe.right;
                bottom = safe.bottom;
            } else if (android.os.Build.VERSION.SDK_INT >= 28 && insets.getDisplayCutout() != null) {
                left = insets.getDisplayCutout().getSafeInsetLeft();
                top = insets.getDisplayCutout().getSafeInsetTop();
                right = insets.getDisplayCutout().getSafeInsetRight();
                bottom = insets.getDisplayCutout().getSafeInsetBottom();
            } else {
                left = insets.getSystemWindowInsetLeft();
                top = insets.getSystemWindowInsetTop();
                right = insets.getSystemWindowInsetRight();
                bottom = insets.getSystemWindowInsetBottom();
            }
            try {
                nativeSetSafeInsets(left, top, right, bottom, getResources().getDisplayMetrics().density);
            } catch (UnsatisfiedLinkError ignored) {
                Log.d(TAG, "Native layout is not ready for insets yet");
            }
            return insets;
        });
        getWindow().getDecorView().requestApplyInsets();
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL2", "main" };
    }

    public void setStatusBarHidden(boolean hidden) {
        statusBarHidden = hidden;
        runOnUiThread(this::applyStatusBarVisibility);
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) {
            applyStatusBarVisibility();
        }
    }

    private void applyStatusBarVisibility() {
        if (android.os.Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                if (statusBarHidden) {
                    controller.setSystemBarsBehavior(
                            WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                    controller.hide(WindowInsets.Type.statusBars());
                } else {
                    controller.setSystemBarsBehavior(WindowInsetsController.BEHAVIOR_DEFAULT);
                    controller.show(WindowInsets.Type.statusBars());
                }
            }
        } else if (statusBarHidden) {
            getWindow().addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        } else {
            getWindow().clearFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
        }
        getWindow().getDecorView().requestApplyInsets();
    }

    private void installPackagedContent() {
        File root = new File(getFilesDir(), "data/" + CONTENT_VERSION);
        File marker = new File(root, "content-version.txt");
        if (marker.isFile()) {
            return;
        }
        if (!root.exists() && !root.mkdirs()) {
            throw new IllegalStateException("Could not create content directory " + root);
        }
        try {
            copyAssetTree("", root);
        } catch (IOException error) {
            throw new IllegalStateException("Could not install IVAN content", error);
        }
    }

    private void copyAssetTree(String assetPath, File destination) throws IOException {
        String[] entries = getAssets().list(assetPath);
        if (entries != null && entries.length > 0) {
            if (!destination.exists() && !destination.mkdirs()) {
                throw new IOException("Could not create " + destination);
            }
            for (String entry : entries) {
                String childAsset = assetPath.isEmpty() ? entry : assetPath + "/" + entry;
                copyAssetTree(childAsset, new File(destination, entry));
            }
            return;
        }

        try (InputStream input = getAssets().open(assetPath);
             FileOutputStream output = new FileOutputStream(destination)) {
            byte[] buffer = new byte[64 * 1024];
            int count;
            while ((count = input.read(buffer)) >= 0) {
                output.write(buffer, 0, count);
            }
        }
    }
}
