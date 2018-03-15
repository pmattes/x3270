cd ..\..\wc3270
%1\mkmanifest.exe -a %2 -d "wc3270 terminal emulator" -e wc3270 -m ..\Common\Win32\manifest.tmpl -v ..\Common\version.txt >wc3270.exe.manifest
cd ..\ws3270
%1\mkmanifest.exe -a %2 -d "ws3270 scripting terminal emulator" -e ws3270 -m ..\Common\Win32\manifest.tmpl -v ..\Common\version.txt >ws3270.exe.manifest
cd ..\wpr3287
%1\mkmanifest.exe -a %2 -d "wpr3287 printer emulator" -e wpr3287 -m ..\Common\Win32\manifest.tmpl -v ..\Common\version.txt >wpr3287.exe.manifest
