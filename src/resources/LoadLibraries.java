package org.lovr.app;

import android.app.NativeActivity;

public class LoadLibraries extends NativeActivity {
  static {
    System.loadLibrary("lovr");
    // System.loadLibrary("vrapi");
  }
}
