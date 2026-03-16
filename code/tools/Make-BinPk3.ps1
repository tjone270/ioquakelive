param(
    [Parameter(Position=0)]
    [string]$TargetDir
)

# Strip trailing quotes/backslashes that MSBuild's "$(TargetDir)" can produce
$TargetDir = $TargetDir.Trim('"').TrimEnd('\')

# Find the baseq3 directory from the target path
if ($TargetDir -match '(?i)(.*baseq3)') {
    $baseq3 = $Matches[1]
} else {
    $baseq3 = Join-Path $TargetDir "baseq3"
}

if (-not (Test-Path $baseq3)) {
    Write-Warning "baseq3 directory not found: $baseq3"
    exit 0
}

Push-Location $baseq3

$dlls = Get-ChildItem -Filter "*.dll" -File
if ($dlls.Count -eq 0) {
    Write-Host "No DLLs found in $baseq3; skipping iobin pk3 creation"
    Pop-Location
    exit 0
}

# Detect arch from DLL names (e.g., cgamex86.dll -> x86, cgamex86_64.dll -> x86_64)
$arch = "x86"
foreach ($dll in $dlls) {
    if ($dll.Name -match 'x86_64\.dll$') {
        $arch = "x86_64"
        break
    }
}

$pk3Name = "iobin_${arch}.pk3"
$zipName = "iobin_${arch}.zip"

if (Test-Path $pk3Name) { Remove-Item $pk3Name -Force }
if (Test-Path $zipName) { Remove-Item $zipName -Force }

Compress-Archive -Path $dlls.FullName -DestinationPath $zipName
Rename-Item $zipName $pk3Name -Force

Write-Host "$pk3Name created with $($dlls.Count) DLL(s)"

# In Release builds, remove the source DLLs (only the pk3 is needed)
if ($baseq3 -match '(?i)\\Release\\') {
    foreach ($dll in $dlls) {
        Remove-Item $dll.FullName -Force
    }
    Write-Host "Release build: removed $($dlls.Count) source DLL(s)"
}

Pop-Location
