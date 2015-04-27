cd ..\..\wc3270
%1\mkfb.exe -c -o fallbacks.c fb-common fb-printSession fb-messages fb-composeMap fb-c3270

cd ..\ws3270
%1\mkfb.exe -c -o fallbacks.c fb-common
