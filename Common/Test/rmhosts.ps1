# Start a cmd.exe window to remove host entries from /etc/hosts
Start-Process cmd.exe -Wait -Verb RunAs -ArgumentList "/c chdir $pwd & python3 -m Common.Test.setupHosts -reverse & pause"
