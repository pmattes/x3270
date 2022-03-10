# Start a cmd.exe window to add host entries to /etc/hosts
Start-Process cmd.exe -Wait -Verb RunAs -ArgumentList "/c chdir $pwd & python3 -m Common.Test.setupHosts & pause"
