python3 ..\..\Common\mkversion.py -o version.c b3270 ..\..\Common\version.txt
python3 ..\..\Common\mkfb.py -c -w -o fallbacks.c ..\..\Common\fb-common ..\..\Common\fb-printSession
python3 ..\..\Common\Win32\mkmanifest.py -o ..\..\wb3270\wb3270.exe.manifest ..\..\Common\version.txt ..\..\Common\Win32\manifest.tmpl b3270 "b3270 terminal emulator back end"