[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repositoryRoot = [IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$runtimeRoot = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'Binaries/v143/x64'))
$release = Join-Path $runtimeRoot 'Release'
$packages = [IO.Path]::GetFullPath((Join-Path $repositoryRoot 'Binaries/Packages'))
$stagingRoot = Join-Path $packages '.BabyDoll_Staging'
$staging = Join-Path $stagingRoot 'BabyDoll_Latest'
$latest = Join-Path $packages 'BabyDoll_Latest'

function Assert-GeneratedChild([string]$Target, [string]$Parent) {
    $resolved = [IO.Path]::GetFullPath($Target)
    $expectedParent = [IO.Path]::GetFullPath($Parent)
    if (-not [string]::Equals([IO.Path]::GetDirectoryName($resolved), $expectedParent, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Refusing to modify a path outside the generated output directory: $resolved"
    }
    if (Test-Path -LiteralPath $resolved) {
        if ((Get-Item -LiteralPath $resolved -Force).Attributes -band [IO.FileAttributes]::ReparsePoint) {
            throw "Refusing to modify a linked output path: $resolved"
        }
    }
    return $resolved
}

function Remove-GeneratedChild([string]$Target, [string]$Parent) {
    $checked = Assert-GeneratedChild $Target $Parent
    if (Test-Path -LiteralPath $checked) { Remove-Item -LiteralPath $checked -Recurse -Force }
}

# Validate absolute output roots before any recursive deletion or move.
foreach ($directory in @($runtimeRoot, $packages)) {
    if (-not $directory.StartsWith($repositoryRoot + [IO.Path]::DirectorySeparatorChar, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Invalid build output root: $directory"
    }
    New-Item -ItemType Directory -Force -Path $directory | Out-Null
    if ((Get-Item -LiteralPath $directory).Attributes -band [IO.FileAttributes]::ReparsePoint) { throw "Linked build output root: $directory" }
}
Assert-GeneratedChild $release $runtimeRoot | Out-Null
Assert-GeneratedChild $latest $packages | Out-Null
Assert-GeneratedChild $stagingRoot $packages | Out-Null
Assert-GeneratedChild $staging $stagingRoot | Out-Null

$runningBuilds = Get-Process -Name SeonEngine -ErrorAction SilentlyContinue | Where-Object {
    $_.Path -and ($_.Path.StartsWith($runtimeRoot + '\', [StringComparison]::OrdinalIgnoreCase) -or
        $_.Path.StartsWith($packages + '\', [StringComparison]::OrdinalIgnoreCase))
}
if ($runningBuilds) { throw 'Close the running BabyDoll prototype before updating the latest build.' }

# Reuse the canonical Release output; do not create per-change runtime directories.
$oldCompilerOptions = $env:_CL_
try {
    $env:_CL_ = (($oldCompilerOptions + ' /Z7').Trim())
    & (Join-Path $PSScriptRoot 'BuildBabyDoll.ps1') -Configuration Release -NoDebugSymbols
}
finally { $env:_CL_ = $oldCompilerOptions }
$executable = Join-Path $release 'SeonEngine.exe'
& $executable --babydoll-test
if ($LASTEXITCODE -ne 0) { throw 'Release verification failed; previous packages were retained.' }

Remove-GeneratedChild $stagingRoot $packages
& (Join-Path $PSScriptRoot 'PackageBabyDoll.ps1') -RuntimeDirectory $release -OutputDirectory $staging
& (Join-Path $staging 'Binaries/v143/x64/Release/SeonEngine.exe') --babydoll-test
if ($LASTEXITCODE -ne 0) { throw 'Package verification failed; previous packages were retained.' }

# Carry forward the newest local checkpoint before removing previous playable folders.
# The distributable ZIP stays clean; only the local playable folder receives the save.
$saveCandidates = @((Join-Path $repositoryRoot 'Binaries/BabyDollSave/checkpoint.json'))
Get-ChildItem -LiteralPath $packages -Directory | Where-Object { $_.Name -like 'BabyDoll_*' } | ForEach-Object {
    $saveCandidates += Join-Path $_.FullName 'Binaries/BabyDollSave/checkpoint.json'
}
$newestSave = $saveCandidates | Where-Object { Test-Path -LiteralPath $_ -PathType Leaf } |
    ForEach-Object { Get-Item -LiteralPath $_ } | Sort-Object LastWriteTimeUtc -Descending | Select-Object -First 1
if ($newestSave) {
    $saveDirectory = Join-Path $staging 'Binaries/BabyDollSave'
    New-Item -ItemType Directory -Force -Path $saveDirectory | Out-Null
    Copy-Item -LiteralPath $newestSave.FullName -Destination (Join-Path $saveDirectory 'checkpoint.json')
}

# Only generated BabyDoll packages and compiled configuration folders are removed.
Get-ChildItem -LiteralPath $packages -Force | Where-Object { $_.Name -like 'BabyDoll_*' } | ForEach-Object {
    Remove-GeneratedChild $_.FullName $packages
}
Assert-GeneratedChild $stagingRoot $packages | Out-Null
Assert-GeneratedChild $staging $stagingRoot | Out-Null
Assert-GeneratedChild ($staging + '.zip') $stagingRoot | Out-Null
Assert-GeneratedChild $latest $packages | Out-Null
Assert-GeneratedChild ($latest + '.zip') $packages | Out-Null
Move-Item -LiteralPath $staging -Destination $latest
Move-Item -LiteralPath ($staging + '.zip') -Destination ($latest + '.zip')
Remove-GeneratedChild $stagingRoot $packages
Get-ChildItem -LiteralPath $runtimeRoot -Directory | Where-Object { $_.Name -ne 'Release' } | ForEach-Object {
    Remove-GeneratedChild $_.FullName $runtimeRoot
}
Get-ChildItem -LiteralPath $release -File | Where-Object {
    $_.Extension -in @('.pdb', '.ilk', '.exp', '.lib') -or ($_.Extension -eq '.exe' -and $_.Name -ne 'SeonEngine.exe')
} | ForEach-Object { Remove-GeneratedChild $_.FullName $release }
$helperRoot = Join-Path $repositoryRoot 'Intermediate/BabyDoll'
Get-ChildItem -LiteralPath $helperRoot -Filter 'build-*.cmd' -File | Where-Object { $_.Name -ne 'build-local.cmd' } | ForEach-Object {
    Remove-GeneratedChild $_.FullName $helperRoot
}
Write-Output "Latest playable build: $latest\Play.cmd"
Write-Output "Latest archive: $latest.zip"
