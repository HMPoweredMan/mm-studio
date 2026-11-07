$ErrorActionPreference='Stop'
$root=Split-Path -Parent $PSScriptRoot
$sdk=Join-Path $root 'tools/vst3sdk'
$revision='3cdf9ca5d1f5b1b21e0a86832aa4abe55607bd96'
if (Test-Path -LiteralPath (Join-Path $sdk 'public.sdk/source/vst/vstaudioeffect.h')) { return }
if (Test-Path -LiteralPath $sdk) { throw 'Incomplete VST3 SDK directory; move it aside before retrying.' }
git init $sdk
if ($LASTEXITCODE) { throw 'Cannot initialize VST3 SDK checkout.' }
git -C $sdk remote add origin https://github.com/steinbergmedia/vst3sdk.git
git -C $sdk fetch --depth 1 origin $revision
if ($LASTEXITCODE) { throw 'Cannot fetch pinned VST3 SDK.' }
git -C $sdk checkout --detach FETCH_HEAD
if ($LASTEXITCODE) { throw 'Cannot check out pinned VST3 SDK.' }
git -C $sdk submodule update --init --recursive --depth 1
if ($LASTEXITCODE) { throw 'Cannot initialize VST3 SDK submodules.' }
