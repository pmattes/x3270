# Run integration tests on Visual Studio binaries.
param (
    [switch] $v,
    [switch] $nobuild
)
$env:PATH="$pwd\VisualStudio\x64\Debug;${env:PATH}"

$ErrorActionPreference = 'Stop'

if (! $nobuild) {
    cd VisualStudio
    msbuild /p:Configuration=Debug /p:Platform=x64
    if ($LASTEXITCODE -ne 0) { throw "Build failed" }
    cd ..
}

# Run the library tests.
foreach ($test in 'base64','bind_opts','json','utf8')
{
    if ($v) {
        Write-Host "${test}_test"
    }
    & .\VisualStudio\x64\Debug\${test}_test.exe
    if ($LASTEXITCODE -ne 0) { throw "Test failed" }
}

# If there are other directories with Windows tests in them, add them to -Path with commas.
$files = Get-ChildItem -File -Filter 'test*.py' -Path s3270/Test,b3270/Test,c3270/Test,wc3270/Test | ForEach-Object { $_.FullName }
if ($v) { $vflag = "-v" }
python3 -m unittest $vflag $files
if ($LASTEXITCODE -ne 0) { throw "Test failed" }
