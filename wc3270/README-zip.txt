wc3270 No-Install README File

This is the installer-free distribution of wc3270. Normally these files are
installed in "C:\Program Files\wc3270" or "C:\Program Files (x86)\wc3270" and
the suffix ".wc3270" is registered to associate wc3270 session files with the
wc3270 app.

It includes the following executable files:
    wc3270.exe       The wc3270 program, runs in a console window
    wc3270wiz.exe    The wc3270 Session Wizard, creates and edits session files
                      and desktop shortcuts
    ws3270.exe       The scripting (screen-scraping) version of wc3270
    catf.exe         Helper application to monitor trace files
    x3270if.exe      Helper application for wc3270 scripting

The documentation is here:
    html\            HTML documentation, the root is html\README.html
    LICENSE.txt      Legal necessities
    NO-INSTALL.txt   How to run wc3270 without installing it (it can be a bit
                      tricky)

Other files of interest are:
    root_certs.txt   SSL root certificates, needed to validate host
                      certificates if SSL is used

If you want to run wc3270 with SSL (SSL tunnels or TLS), you need to install a
third-party OpenSSL library. See the file html/SSL.html for details.
