# run-signtool.ps1 <files-to-sign>

# Any error kills the script.
$ErrorActionPreference = 'Stop'

# Find everything.
$signtool = 'C:\Program Files (x86)\Windows Kits\10\bin\10.0.26100.0\x64\signtool.exe'
$dlib = 'C:\Users\pdm\AppData\Local\Microsoft\MicrosoftTrustedSigningClientTools\Azure.CodeSigning.Dlib.dll'
$timestamp = 'http://timestamp.acs.microsoft.com'

# Put the Azure Trusted Signing metadata in a temporary file.
$tempJson = Get-ChildItem ([IO.Path]::GetTempFileName()) | Rename-Item -NewName { [IO.Path]::ChangeExtension($_, ".json") } -PassThru
$json = @'
{
  "Endpoint": "https://wcus.codesigning.azure.net",
  "CodeSigningAccountName": "x3270",
  "CertificateProfileName": "x3270",
  "ExcludeCredentials": [
    "AzureCliCredential",
    "ManagedIdentityCredential",
    "EnvironmentCredential",
    "WorkloadIdentityCredential",
    "SharedTokenCacheCredential",
    "VisualStudioCredential",
    "VisualStudioCodeCredential",
    "AzureDeveloperCliCredential",
    "InteractiveBrowserCredential"
  ]
}
'@
$json | Out-File -FilePath $tempJson -Encoding ascii

function Sign {
    param (
      [string]$FileName
    )
    $out = (& $signtool sign /v /td SHA256 /tr $timestamp /fd SHA256 /dlib $dlib /dmdf $tempjson "$FileName")
    if ((Get-AuthenticodeSignature "$FileName").Status -ne "Valid")
    {
        Write-Error "$FileName not signed"
        Write-Error "Signtool output: $out"
        exit 1
    }
}

# Sign, suppressing output.
$count = 0
foreach ($file in $args[0])
{
    $percent = ($count / $args[0].Count) * 100
    $remaining = ($args[0].Count - $count) * 5
    $count = $count + 1
    Write-Progress -Activity "Signing" -CurrentOperation "$file" -PercentComplete $percent -SecondsRemaining $remaining
    Sign("$file")
}
Write-Progress -Activity "Signing" -Completed
Remove-Item $tempJson