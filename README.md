# Build

**Note**: This driver has been converted from WDF (Windows Driver Framework) to WDM (Windows Driver Model) to eliminate the WdfCoInstaller dependency and ensure compatibility with Windows Server 2003 and up.

## Build Process Overview

The build process is designed to resolve the circular dependency between the driver build and catalog creation:
1. **Initial Build**: The driver builds without catalog validation (CatalogFile directive is commented out in the INX template)
2. **Catalog Creation**: Scripts create the catalog file and generate a final INF with the CatalogFile directive enabled
3. **Result**: A complete, ready-to-sign driver package

## Step 1: Build the driver

```bash
msbuild kbfiltr.vcxproj /p:Configuration="Win8.1 Release" /property:Platform=x64
```

## Step 2: Create the catalog file

After building the driver, create the catalog file using one of the provided scripts. The scripts will create a complete driver package with the catalog file and a final INF file that references it:

**Batch Script:**
```bash
# For x64 Windows 8.1 (default)
create_catalog.bat x64 Win8.1

# For x86 Windows 7
create_catalog.bat x86 Win7

# For x64 Windows 8
create_catalog.bat x64 Win8
```

**PowerShell Script (recommended):**
```powershell
# For x64 Windows 8.1 (default)
.\Create-Catalog.ps1 -Architecture x64 -OSVersion Win8.1

# For x86 Windows 7
.\Create-Catalog.ps1 -Architecture x86 -OSVersion Win7

# For x64 Windows 8
.\Create-Catalog.ps1 -Architecture x64 -OSVersion Win8
```

Both scripts will:
- Create a final INF file with the CatalogFile directive enabled
- Run `inf2cat` to create the catalog file (`kbfiltr.cat`)
- Generate a complete driver package ready for signing
- Verify all files were created successfully

The complete driver package will contain:
- `kbfiltr.sys` - The driver binary
- `kbfiltr.inf` - INF file with catalog reference enabled  
- `kbfiltr.cat` - Catalog file for signing

**Architecture Changes:**
- Converted from KMDF to WDM driver type
- Removed all WDF framework dependencies
- Eliminated WdfCoInstaller requirement
- Maintains all core keyboard filtering functionality
- Raw PDO functionality temporarily disabled (can be re-implemented in WDM if needed)

## Step 3: Sign the driver and catalog

```bash
# 1. Create test signing certificate
makecert -r -pe -ss PrivateCertStore -n CN=Contoso.com(Test) ContosoTest.cer

# 2. Sign driver
signtool sign /v /s PrivateCertStore /n Contoso.com(Test) /t http://timestamp.verisign.com/scripts/timestamp.dll build/x64-Win8.1Release/kbfiltr.sys

# 3. Sign catalog file
signtool sign /v /s PrivateCertStore /n Contoso.com(Test) /t http://timestamp.verisign.com/scripts/timestamp.dll build/x64-Win8.1Release/kbfiltr.cat
```

# Install

1. On the target system, run `bcdedit /set testsigning on` and install the test cert
2. Copy `build/x64-Win8.1Release/kbfiltr.sys` to `c:/Windows/System32/drivers` on the target system
3. Create service on target system
   ```
   sc create kbfiltr binPath= C:\Windows\System32\drivers\kbfiltr.sys type= kernel
   ```
4. Install driver as an upper device filter below `kbdclass`
   ```
   REG ADD HKLM\SYSTEM\CurrentControlSet\Control\Class\{4D36E96B-E325-11CE-BFC1-08002BE10318} /v UpperFilters /t REG_MULTI_SZ /d kbfiltr\0kbdclass /f
   ```
5. Reboot target system (TODO: find a more immediate way of installing and updating)
