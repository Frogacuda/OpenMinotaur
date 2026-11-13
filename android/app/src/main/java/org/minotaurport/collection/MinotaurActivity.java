package org.minotaurport.collection;

import android.content.res.Configuration;
import android.content.pm.ActivityInfo;
import android.content.pm.PackageManager;
import android.os.Build;
import android.os.Bundle;
import android.view.InputDevice;
import android.view.View;
import android.view.Window;
import android.view.WindowInsets;
import android.view.WindowInsetsController;
import android.view.WindowManager;
import org.libsdl.app.SDLActivity;

/** The app's only activity: SDL's activity, which loads libSDL3.so and libmain.so. */
public class MinotaurActivity extends SDLActivity {
    // Touchscreen devices default to Native unless launched landscape with a pad.
    // TVs, desktop-class devices and other specialized UI modes retain Horizontal.
    public boolean usesPortableDisplayDefaults() {
        int type = getResources().getConfiguration().uiMode & Configuration.UI_MODE_TYPE_MASK;
        PackageManager pm = getPackageManager();
        boolean mobile = (type == Configuration.UI_MODE_TYPE_NORMAL || type == Configuration.UI_MODE_TYPE_UNDEFINED)
            && pm.hasSystemFeature(PackageManager.FEATURE_TOUCHSCREEN)
            && !pm.hasSystemFeature("android.hardware.type.pc");
        if (!mobile) return false;
        if (getResources().getConfiguration().orientation == Configuration.ORIENTATION_LANDSCAPE) {
            for (int id : InputDevice.getDeviceIds()) {
                InputDevice device = InputDevice.getDevice(id);
                if (device != null && (device.supportsSource(InputDevice.SOURCE_GAMEPAD)
                        || device.supportsSource(InputDevice.SOURCE_JOYSTICK))) return false;
            }
        }
        return true;
    }

    @Override
    public void setOrientationBis(int w, int h, boolean resizable, String hint) {
        runOnUiThread(() -> {
            if (hint.contains("MinotaurLocked")) {
                boolean landscape = hint.contains("LandscapeLeft");
                boolean currentLandscape = getResources().getConfiguration().orientation
                    == Configuration.ORIENTATION_LANDSCAPE;
                // Keep the current normal/reversed direction when its axis fits.
                // A native game on the other axis first requests that fixed axis.
                setRequestedOrientation(landscape == currentLandscape
                    ? ActivityInfo.SCREEN_ORIENTATION_LOCKED
                    : landscape ? ActivityInfo.SCREEN_ORIENTATION_LANDSCAPE
                                : ActivityInfo.SCREEN_ORIENTATION_PORTRAIT);
            } else {
                super.setOrientationBis(w, h, resizable, hint);
            }
        });
    }

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        applyImmersiveMode();
    }

    @Override
    protected void onResume() {
        super.onResume();
        applyImmersiveMode();
    }

    @Override
    public void onWindowFocusChanged(boolean hasFocus) {
        super.onWindowFocusChanged(hasFocus);
        if (hasFocus) applyImmersiveMode();
    }

    @Override
    public void onConfigurationChanged(Configuration configuration) {
        super.onConfigurationChanged(configuration);
        applyImmersiveMode();
    }

    @Override
    public void onSystemUiVisibilityChange(int visibility) {
        // Modern transient bars auto-hide; keep SDL's legacy rehide path only
        // on releases that use the legacy visibility flags.
        if (Build.VERSION.SDK_INT < 30) super.onSystemUiVisibilityChange(visibility);
    }

    @SuppressWarnings("deprecation")
    private void applyImmersiveMode() {
        Window window = getWindow();
        window.clearFlags(WindowManager.LayoutParams.FLAG_FORCE_NOT_FULLSCREEN);
        if (Build.VERSION.SDK_INT >= 30) {
            // Fill the window instead of reserving status/navigation-bar bands.
            window.setDecorFitsSystemWindows(false);
            WindowInsetsController controller = window.getInsetsController();
            if (controller != null) {
                controller.setSystemBarsBehavior(
                    WindowInsetsController.BEHAVIOR_SHOW_TRANSIENT_BARS_BY_SWIPE);
                controller.hide(WindowInsets.Type.systemBars());
            }
        } else {
            window.addFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN);
            window.getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION |
                View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY | View.SYSTEM_UI_FLAG_LAYOUT_STABLE |
                View.SYSTEM_UI_FLAG_LAYOUT_FULLSCREEN | View.SYSTEM_UI_FLAG_LAYOUT_HIDE_NAVIGATION);
        }
    }

    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }
}
