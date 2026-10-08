# These methods are looked up by name from FeLib/Source/mobileui.cpp.
# The activity itself is retained through the Android manifest.
-keepclassmembers,allowoptimization class io.github.harminoff.ivan.IvanActivity {
    public void setStatusBarHidden(boolean);
    public void vibrateFeedback(int, int);
}
