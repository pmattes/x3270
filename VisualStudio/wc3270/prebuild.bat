python3 ..\..\Common\mkkeypad.py -I..\..\Common\c3270 -o compiled_keypad.h
python3 ..\..\Common\mkversion.py -o version.c wc3270 ..\..\Common\version.txt
python3 ..\..\Common\mkfb.py -c -w -o fallbacks.c ..\..\Common\fb-common ..\..\Common\fb-printSession ..\..\Common\fb-messages ..\..\Common\fb-composeMap ..\..\Common\fb-c3270
python3 ..\..\Common\Win32\mkmanifest.py -o ..\..\wc3270\wc3270.exe.manifest ..\..\Common\version.txt ..\..\Common\Win32\manifest.tmpl wc3270 "wc3270 terminal emulator"