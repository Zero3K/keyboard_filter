param(
    [Parameter(Mandatory=$true)]
    [ValidateSet("x86", "x64")]
    [string]$Architecture,
    
    [Parameter(Mandatory=$false)]
    [ValidateSet("Win7", "Win8", "Win8.1")]
    [string]$OSVersion = "Win8.1"
)

<#
.SYNOPSIS
    Creates a catalog file for the keyboard filter driver

.DESCRIPTION
    This script creates a catalog file (.cat) for the keyboard filter driver using inf2cat.
    It should be run after building the driver but before package creation or signing.

.PARAMETER Architecture
    Target architecture: x86 or x64

.PARAMETER OSVersion
    Target OS version: Win7, Win8, or Win8.1 (default: Win8.1)

.EXAMPLE
    .\Create-Catalog.ps1 -Architecture x64 -OSVersion Win8.1
    .\Create-Catalog.ps1 -Architecture x86 -OSVersion Win7
#>

$ErrorActionPreference = "Stop"

# Set build output directory based on architecture and OS version
$BuildDir = "build\$Architecture-$OSVersion" + "Release"
$IntermediateDir = "build\intermediate\$Architecture-$OSVersion" + "Release"

Write-Host "Creating catalog file for $Architecture $OSVersion..." -ForegroundColor Green

# Check if build directory exists
if (-not (Test-Path $BuildDir)) {
    Write-Error "Build directory '$BuildDir' does not exist. Please build the driver first using: msbuild kbfiltr.vcxproj /p:Configuration='$OSVersion Release' /property:Platform=$Architecture"
    exit 1
}

# Check if driver file exists
$DriverPath = Join-Path $BuildDir "kbfiltr.sys"
if (-not (Test-Path $DriverPath)) {
    Write-Error "Driver file '$DriverPath' does not exist. Please build the driver first."
    exit 1
}

# Check if INF file exists
$InfPath = Join-Path $IntermediateDir "kbfiltr.inf"
if (-not (Test-Path $InfPath)) {
    Write-Error "INF file '$InfPath' does not exist. Please build the driver first to generate the INF from the INX template."
    exit 1
}

# Set OS version parameters for inf2cat
switch ($OSVersion) {
    "Win7" { $OSList = "7_X86,7_X64" }
    "Win8" { $OSList = "8_X86,8_X64" }
    "Win8.1" { $OSList = "6_3_X86,6_3_X64" }
    default { 
        Write-Error "Unsupported OS version '$OSVersion'. Supported versions: Win7, Win8, Win8.1"
        exit 1
    }
}

# Clean up any previous catalog files and extra directories to avoid conflicts
$CatalogPath = Join-Path $BuildDir "kbfiltr.cat"
$ExtraDir = Join-Path $BuildDir "kbfiltr"
if (Test-Path $CatalogPath) { Remove-Item $CatalogPath }
if (Test-Path $ExtraDir) { Remove-Item $ExtraDir -Recurse -Force }

# Create final INF file with CatalogFile directive enabled
$BuildInfPath = Join-Path $BuildDir "kbfiltr.inf"
Write-Host "Creating final INF file with catalog reference..." -ForegroundColor Yellow

# Copy the INF file (CatalogFile directive is already enabled in source)
Copy-Item $InfPath $BuildInfPath

# Create catalog file using inf2cat
Write-Host "Running inf2cat to create catalog file..." -ForegroundColor Yellow
$inf2catArgs = @(
    "/driver:$BuildDir",
    "/os:$OSList",
    "/verbose"
)

try {
    $process = Start-Process -FilePath "inf2cat" -ArgumentList $inf2catArgs -Wait -PassThru -NoNewWindow
    if ($process.ExitCode -ne 0) {
        throw "inf2cat exited with code $($process.ExitCode)"
    }
} catch {
    Write-Error "inf2cat failed to create catalog file: $_"
    exit 1
}

# Verify catalog file was created
$CatalogPath = Join-Path $BuildDir "kbfiltr.cat"
if (Test-Path $CatalogPath) {
    Write-Host "Success: Catalog file created at '$CatalogPath'" -ForegroundColor Green
    Write-Host "Success: Final INF file created at '$BuildInfPath'" -ForegroundColor Green
    Write-Host ""
    Write-Host "Complete driver package contents:" -ForegroundColor Cyan
    Write-Host "- Driver:  $DriverPath" -ForegroundColor White
    Write-Host "- INF:     $BuildInfPath (with catalog reference)" -ForegroundColor White  
    Write-Host "- Catalog: $CatalogPath" -ForegroundColor White
    Write-Host ""
    Write-Host "Next steps:" -ForegroundColor Cyan
    Write-Host "1. Sign the catalog file with your code signing certificate" -ForegroundColor White
    Write-Host "2. Test install the driver package" -ForegroundColor White
    Write-Host ""
    Write-Host "Example signing command:" -ForegroundColor Cyan
    Write-Host "signtool sign /v /s PrivateCertStore /n `"Contoso.com(Test)`" /t http://timestamp.verisign.com/scripts/timestamp.dll `"$CatalogPath`"" -ForegroundColor White
} else {
    Write-Error "Catalog file was not created"
    exit 1
}