[CmdletBinding()]
param(
  # Compile and stop.
  [switch]$BuildOnly,
  # Open the project in the Unreal Editor instead of playing it.
  [switch]$Editor,
  # Play without compiling first.
  [switch]$SkipBuild,
  # Passed to the game, for example '-Seed=12345 -Level=6 -Bot'.
  [string]$Extra = ''
)

# Compiles MindPuckEditor, then plays the game in a window (the editor binaries with -game).
# The engine is $env:UE_ROOT, or D:\Games\UE_5.8.

$ErrorActionPreference = 'Stop'

$engine = if ([string]::IsNullOrWhiteSpace($env:UE_ROOT)) { 'D:\Games\UE_5.8' } else { $env:UE_ROOT.TrimEnd('\') }
$editorExe = Join-Path $engine 'Engine\Binaries\Win64\UnrealEditor.exe'
if (-not (Test-Path -LiteralPath $editorExe)) { throw "Unreal Engine not found at '$engine'. Set UE_ROOT to the engine folder." }

$uproject = Join-Path $PSScriptRoot 'MindPuck.uproject'
$plugin = Join-Path $PSScriptRoot '..\MindAttic.EyeTracker\unreal\MindAtticEyeTracker\MindAtticEyeTracker.uplugin'
if (-not (Test-Path -LiteralPath $plugin)) { throw "The gaze plugin is missing ($plugin). Clone MindAttic.EyeTracker next to this repo." }

if (-not $SkipBuild) {
  $buildBat = Join-Path $engine 'Engine\Build\BatchFiles\Build.bat'
  Write-Output 'Compiling MindPuckEditor Win64 Development...'
  & $buildBat MindPuckEditor Win64 Development "-project=$uproject" -waitmutex
  if ($LASTEXITCODE -ne 0) { throw "Build.bat failed (exit $LASTEXITCODE)." }
}
if ($BuildOnly) { return }

if ($Editor) {
  Start-Process -FilePath $editorExe -ArgumentList "`"$uproject`""
  return
}

$gameArgs = @("`"$uproject`"", '-game', '-windowed', '-ResX=1600', '-ResY=900', '-log') + @($Extra -split '\s+' | Where-Object { $_ })
Start-Process -FilePath $editorExe -ArgumentList $gameArgs
