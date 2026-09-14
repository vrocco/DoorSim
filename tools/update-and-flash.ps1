$ErrorActionPreference = "Stop"

$PlatformIO = "C:\Users\vito\.platformio\penv\Scripts\platformio.exe"

if (-not (Test-Path $PlatformIO)) {
    throw "PlatformIO not found at $PlatformIO"
}

$dirty = git status --porcelain
if ($dirty) {
    throw "Working tree is not clean. Commit, stash, or remove local changes before flashing."
}

$branch = (git branch --show-current).Trim()
if ($branch -ne "main") {
    throw "Hardware checkpoints are flashed from main. Current branch: $branch"
}

Write-Host "Updating hardware-validated main..."
git pull --ff-only origin main

Write-Host "Building firmware..."
& $PlatformIO run
if ($LASTEXITCODE -ne 0) { throw "PlatformIO build failed." }

Write-Host "Uploading firmware..."
& $PlatformIO run --target upload
if ($LASTEXITCODE -ne 0) { throw "PlatformIO upload failed." }

Write-Host "Firmware uploaded successfully. Run the checkpoint hardware test plan supplied by ChatGPT."
