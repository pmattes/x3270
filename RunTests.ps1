# Run integration tests on Visual Studio binaries.
$pwd=Get-Location
$env:PATH="$pwd\VisualStudio\Debug;${env:PATH}"
$env:PYTHONPATH="$pwd\Common\Test"
$files = Get-ChildItem -File -Filter '*.py' -Path s3270/Test | % { $_.FullName }
python3 -m unittest $files
