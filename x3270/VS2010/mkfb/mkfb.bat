cd ..\..\wc3270
type fb-common >fallbacks.txt
type fb-printSession >>fallbacks.txt
type fb-messages >>fallbacks.txt
type fb-composeMap >>fallbacks.txt
type fb-c3270 >>fallbacks.txt
%1\mkfb.exe -c fallbacks.txt >fallbacks.c

cd ..\ws3270
type fb-common >fallbacks.txt
%1\mkfb.exe -c fallbacks.txt >fallbacks.c
