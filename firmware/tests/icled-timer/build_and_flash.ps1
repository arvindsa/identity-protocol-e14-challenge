Set-Location $PSScriptRoot

# Load machine-specific SDK paths (copy ../../sdk.local.ps1.example to ../../sdk.local.ps1)
$ConfigFile = Join-Path $PSScriptRoot "..\..\sdk.local.ps1"
if (-not (Test-Path $ConfigFile)) {
    Write-Error "sdk.local.ps1 not found. Copy firmware/sdk.local.ps1.example to firmware/sdk.local.ps1 and fill in your LPSDK_ROOT."
    exit 1
}
. $ConfigFile

$MsysBase     = "$LPSDK_ROOT\Toolchain\msys\1.0"
$Make         = "$MsysBase\bin\make.exe"
$OpenOCD      = "$LPSDK_ROOT\Toolchain\bin\openocd.exe"
$LpsdkTcl     = ($LPSDK_ROOT + "\Toolchain").Replace('\', '/')
$OcdCfg       = "..\..\openocd.cfg"
$Elf          = ($PSScriptRoot -replace '\\', '/') + "/build/icled_timer_test.elf"

$env:PATH = "$MsysBase\bin;$LPSDK_ROOT\Toolchain\bin;$env:PATH"

Write-Host "==> Cleaning..."
& $Make clean "MAKE=$Make"

Write-Host "==> Building..."
& $Make "MAKE=$Make"
if ($LASTEXITCODE -ne 0) { Write-Error "Build failed"; exit 1 }

if (-not (Test-Path ($Elf -replace '/', '\'))) {
    Write-Error "ELF not found after build: $Elf"
    exit 1
}

Write-Host "==> Flashing $Elf ..."
& $OpenOCD -c "set LPSDK_TOOLCHAIN $LpsdkTcl" -f $OcdCfg `
    -c "program $Elf verify reset exit"
