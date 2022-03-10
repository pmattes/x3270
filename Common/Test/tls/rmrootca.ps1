# Start a cmd.exe window running certutil to remove the fake root CA.
Start-Process cmd.exe -Wait -Verb RunAs -ArgumentList "/c certutil -delstore root fakeca.com & pause"
