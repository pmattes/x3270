cd ..\..\wc3270
%1\mkfb.exe -c -o fallbacks.c ..\Common\fb-common ..\Common\fb-printSession ..\Common\fb-messages ..\Common\fb-composeMap ..\Common\fb-c3270

cd ..\ws3270
%1\mkfb.exe -c -o fallbacks.c ..\Common\fb-common
