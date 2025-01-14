python3 ..\..\Common\mkversion.py -o version.c s3270 ..\..\Common\version.txt
python3 ..\..\Common\mkfb.py -c -w -o fallbacks.c ..\..\Common\fb-common ..\..\Common\fb-printSession
python3 ..\..\Common\Win32\mkmanifest.py -o ..\..\ws3270\ws3270.exe.manifest ..\..\Common\version.txt ..\..\Common\Win32\manifest.tmpl s3270 "s3270 scripting terminal emulator"