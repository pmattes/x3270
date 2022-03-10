# Start a cmd.exe window running certutil to add the fake root CA.
Start-Process cmd.exe -Wait -Verb RunAs -ArgumentList "/c certutil -addstore root $pwd\Common\Test\tls\myCa.cer & pause"
