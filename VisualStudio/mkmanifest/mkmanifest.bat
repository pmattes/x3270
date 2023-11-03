cd ..\..\wc3270
%1\mkmanifest.exe -a %2 -d "wc3270 terminal emulator" -e wc3270 -m ..\Common\Win32\manifest.tmpl -v ..\Common\version.txt >wc3270.exe.manifest
cd ..\ws3270
%1\mkmanifest.exe -a %2 -d "s3270 scripting terminal emulator" -e ws3270 -m ..\Common\Win32\manifest.tmpl -v ..\Common\version.txt >ws3270.exe.manifest
cd ..\wpr3287
%1\mkmanifest.exe -a %2 -d "pr3287 printer emulator" -e wpr3287 -m ..\Common\Win32\manifest.tmpl -v ..\Common\version.txt >wpr3287.exe.manifest
cd ..\wb3270
%1\mkmanifest.exe -a %2 -d "b3270 terminal emulator back-end" -e wb3270 -m ..\Common\Win32\manifest.tmpl -v ..\Common\version.txt >wb3270.exe.manifest
