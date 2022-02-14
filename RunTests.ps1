# Run integration tests on Visual Studio binaries.
param (
    [switch] $v
)
$env:PATH="$pwd\VisualStudio\Debug;${env:PATH}"

# If there are other directories with Windows tests in them, add them to -Path with commas.
$files = Get-ChildItem -File -Filter 'test*.py' -Path s3270/Test,b3270/Test | ForEach-Object { $_.FullName }
if ($v) { $vflag = "-v"}
python3 -m unittest $vflag $files
