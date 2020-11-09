%~d0
cd %~dp0

if not exist %UserProfile%\Documents\Arduino\libraries\CapstoneCustomLib mkdir %UserProfile%\Documents\Arduino\libraries\CapstoneCustomLib
xcopy /F /s /Y "lib/capstone_custom_lib.h" %UserProfile%\Documents\Arduino\libraries\CapstoneCustomLib\custom_capstone_lib.h*
xcopy /F /s /Y "lib/capstone_default_settings.h" %UserProfile%\Documents\Arduino\libraries\CapstoneCustomLib\capstone_default_settings.h*

::pause 