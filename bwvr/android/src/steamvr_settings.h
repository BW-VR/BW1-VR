// THE SETTINGS OF STEAMVR (display_hz, docs/android.md, "The display rate"). SteamVR takes
// the rate of an app from preferredRefreshRate in the section of the app, in its memory.
// Its list of XR_FB_display_refresh_rate has only one rate, and an edit of the file
// steamvr.vrsettings goes when SteamVR saves it again. So the app writes that value with
// IVRSettings_003 of vrclient.so, the client of SteamVR that the OpenXR runtime has in
// this process. No file of SteamVR is in the APK, and with another runtime this does
// nothing.
#pragma once

#include <string>

namespace steamvr_settings {

// After the start of the session. True when this process has vrclient.so and its
// settings answer. ini_key: steamvr_app_key of bwvr.ini, for example
// "steam.app.2729671027". Empty: the key comes from SteamAppId of the environment.
bool Start(const std::string& ini_key);
bool On();
const std::string& AppKey();

// preferredRefreshRate of the app, 0 with none.
float ReadRate();
// True when SteamVR took the value.
bool WriteRate(float hz);

}  // namespace steamvr_settings
