package io.github.harminoff.ivan;

import android.os.Bundle;
import android.system.Os;
import android.util.Log;
import android.graphics.Rect;
import android.view.DisplayCutout;
import android.view.View;
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

    private static native void nativeSetSafeInsets(int left, int top, int right, int bottom,
                                                   int cutoutLeft, int cutoutTop,
                                                   int cutoutRight, int cutoutBottom,
                                                   float density);

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

        configureEdgeToEdgeWindow();

        getWindow().getDecorView().setOnApplyWindowInsetsListener((view, insets) -> {
            int left = 0;
            int top = 0;
            int right = 0;
            int bottom = 0;
            Rect displayCutout = new Rect();
            if (android.os.Build.VERSION.SDK_INT >= 30) {
                int barTypes = WindowInsets.Type.navigationBars();
                if (!statusBarHidden) {
                    barTypes |= WindowInsets.Type.statusBars();
                }
                android.graphics.Insets bars = insets.getInsets(barTypes);
                android.graphics.Insets cutout = insets.getInsets(WindowInsets.Type.displayCutout());
                left = statusBarHidden ? bars.left : Math.max(bars.left, cutout.left);
                top = statusBarHidden ? bars.top : Math.max(bars.top, cutout.top);
                right = statusBarHidden ? bars.right : Math.max(bars.right, cutout.right);
                bottom = statusBarHidden ? bars.bottom : Math.max(bars.bottom, cutout.bottom);
                displayCutout = findDisplayCutout(insets.getDisplayCutout());
            } else if (android.os.Build.VERSION.SDK_INT >= 28) {
                DisplayCutout cutout = insets.getDisplayCutout();
                left = statusBarHidden ? insets.getSystemWindowInsetLeft()
                        : Math.max(insets.getSystemWindowInsetLeft(),
                                   cutout != null ? cutout.getSafeInsetLeft() : 0);
                top = statusBarHidden ? 0 : Math.max(insets.getSystemWindowInsetTop(),
                                cutout != null ? cutout.getSafeInsetTop() : 0);
                right = statusBarHidden ? insets.getSystemWindowInsetRight()
                        : Math.max(insets.getSystemWindowInsetRight(),
                                   cutout != null ? cutout.getSafeInsetRight() : 0);
                bottom = statusBarHidden ? insets.getSystemWindowInsetBottom()
                        : Math.max(insets.getSystemWindowInsetBottom(),
                                   cutout != null ? cutout.getSafeInsetBottom() : 0);
                displayCutout = findDisplayCutout(cutout);
            } else {
                left = insets.getSystemWindowInsetLeft();
                top = statusBarHidden ? 0 : insets.getSystemWindowInsetTop();
                right = insets.getSystemWindowInsetRight();
                bottom = insets.getSystemWindowInsetBottom();
            }
            if (!statusBarHidden) {
                displayCutout.setEmpty();
            }
            try {
                nativeSetSafeInsets(left, top, right, bottom,
                        displayCutout.left, displayCutout.top,
                        displayCutout.right, displayCutout.bottom,
                        getResources().getDisplayMetrics().density);
            } catch (UnsatisfiedLinkError ignored) {
                Log.d(TAG, "Native layout is not ready for insets yet");
            }
            return insets;
        });
        getWindow().getDecorView().requestApplyInsets();
    }

    private void configureEdgeToEdgeWindow() {
        if (android.os.Build.VERSION.SDK_INT >= 30) {
            getWindow().setDecorFitsSystemWindows(false);
        } else {
            getWindow().getDecorView().setSystemUiVisibility(
                    View.SYSTEM_UI_FLAG_LAYOUT_STABLE
                            | View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN
                            | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
        if (android.os.Build.VERSION.SDK_INT >= 28) {
            WindowManager.LayoutParams attributes = getWindow().getAttributes();
            attributes.layoutInDisplayCutoutMode = android.os.Build.VERSION.SDK_INT >= 30
                    ? WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_ALWAYS
                    : WindowManager.LayoutParams.LAYOUT_IN_DISPLAY_CUTOUT_MODE_SHORT_EDGES;
            getWindow().setAttributes(attributes);
        }
    }

    private Rect findDisplayCutout(DisplayCutout cutout) {
        if (cutout == null) {
            return new Rect();
        }
        Rect result = new Rect();
        for (Rect bounds : cutout.getBoundingRects()) {
            if (!bounds.isEmpty()) {
                if (result.isEmpty() || bounds.width() * bounds.height()
                        > result.width() * result.height()) {
                    result.set(bounds);
                }
            }
        }
        return result;
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
        configureEdgeToEdgeWindow();
        if (android.os.Build.VERSION.SDK_INT >= 30) {
            WindowInsetsController controller = getWindow().getInsetsController();
            if (controller != null) {
                if (statusBarHidden) {
                    controller.setSystemBarsBehavior(
                            WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                    controller.hide(WindowInsets.Type.statusBars());
                } else {
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
