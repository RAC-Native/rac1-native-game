# rac1-native's tools on Windows, for setup.cmd: a private MSYS2 (its CLANG64 environment) in the repository's
# .toolchain\msys64 folder, with what the setup and the build need. The first run downloads MSYS2 (a fixed release,
# checked against its SHA-256) and the packages; later runs only install what is missing (after a git pull that
# adds a package). Nothing is installed outside .toolchain: deleting the folder removes it all.
# FFmpeg plays the game's movies: the build copies its DLLs beside the program (CMakeLists.txt), so the game also
# starts without MSYS2.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'  # (Invoke-WebRequest's progress bar makes it very slow on PowerShell 5)

$MsysRelease = '20260927'
$MsysSha256 = 'ad336cccfda47758b5e15cda993fbba421115cb0b126697daef1ee4dfe37209f'
$MsysUrl = "https://repo.msys2.org/distrib/x86_64/msys2-base-x86_64-$MsysRelease.sfx.exe"
$Packages = @('python', 'cmake', 'clang', 'ninja', 'pkgconf', 'ffmpeg') | ForEach-Object { "mingw-w64-clang-x86_64-$_" }

$Root = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)
$ToolDir = Join-Path $Root '.toolchain'
$Msys = Join-Path $ToolDir 'msys64'
$Bash = Join-Path $Msys 'usr\bin\bash.exe'

function Step($msg) { Write-Host "  $msg" }
function Fail($msg) { Write-Host "  ERROR  $msg" -ForegroundColor Red; exit 1 }

# A command in MSYS2's shell (CLANG64), from the repository's folder
function Invoke-Msys([string]$cmd) {
    $env:MSYSTEM = 'CLANG64'
    $env:CHERE_INVOKING = '1'
    $env:MSYS2_PATH_TYPE = 'minimal'  # (not Windows' PATH: another MSYS2, Git's tools or a Python there stay out)
    & $Bash -lc $cmd | Out-Host
    return $LASTEXITCODE
}

if (-not (Test-Path $Bash)) {
    Write-Host ''
    Write-Host 'Preparing the tools that build the game (once: about 1 GB, a few minutes)'
    New-Item -ItemType Directory -Force $ToolDir | Out-Null
    $sfx = Join-Path $ToolDir "msys2-base-$MsysRelease.sfx.exe"
    if (-not (Test-Path $sfx) -or (Get-FileHash $sfx -Algorithm SHA256).Hash -ne $MsysSha256) {
        Step "Downloading MSYS2 ($MsysUrl)"
        $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
        try {
            if ($curl) {
                & $curl.Source -L --fail --retry 3 --progress-bar -o $sfx $MsysUrl
                if ($LASTEXITCODE -ne 0) { throw "curl exited with $LASTEXITCODE" }
            } else {
                [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
                Invoke-WebRequest -UseBasicParsing -Uri $MsysUrl -OutFile $sfx
            }
        } catch {
            Fail "The download failed ($_). Check the internet connection, then run the setup again."
        }
        if ((Get-FileHash $sfx -Algorithm SHA256).Hash -ne $MsysSha256) {
            Remove-Item $sfx
            Fail 'The downloaded MSYS2 is not the expected file (its SHA-256 differs). Run the setup again.'
        }
    }
    Step 'Unpacking MSYS2'
    $unpack = Start-Process -FilePath $sfx -ArgumentList '-y', "-o`"$ToolDir`"" -Wait -PassThru -WindowStyle Hidden
    if ($unpack.ExitCode -ne 0 -or -not (Test-Path $Bash)) { Fail "Unpacking MSYS2 failed (exit code $($unpack.ExitCode))." }
    Remove-Item $sfx
}

# One setup at a time: two pacman in the same MSYS2 break each other's work (and its keyring)
$running = Get-Process pacman, gpg -ErrorAction SilentlyContinue |
    Where-Object { $_.Path -and $_.Path.StartsWith($Msys, [StringComparison]::OrdinalIgnoreCase) }
if ($running) { Fail 'Another setup is running (or its window is still open). Close it, then run the setup again.' }
# (a run that was stopped, its window closed, can leave pacman's lock behind)
$lock = Join-Path $Msys 'var\lib\pacman\db.lck'
if (Test-Path $lock) { Remove-Item $lock }

# Only the repositories the build uses: clang64 and msys (MSYS2's own tools). The others (clangarm64, mingw64,
# ucrt64, ...) would only be downloaded for nothing.
$conf = Join-Path $Msys 'etc\pacman.conf'
$text = [IO.File]::ReadAllText($conf)
$trimmed = $text -replace '(?m)^\[(clangarm64|clang32|mingw32|mingw64|ucrt64)\]\r?\nInclude = [^\r\n]*\r?\n', ''
if ($trimmed -ne $text) { [IO.File]::WriteAllText($conf, $trimmed, (New-Object Text.UTF8Encoding $false)) }

# The keys pacman checks the packages with, made by keyring.sh (not by MSYS2's first start: see there), before the
# first start and after each update (an update can bring new keys)
function Update-Keyring {
    $script = (Join-Path $PSScriptRoot 'keyring.sh') -replace '\\', '/'
    & $Bash --noprofile --norc $script | Out-Host
    if ($LASTEXITCODE -ne 0) { Fail 'Preparing the keys that check the downloads failed.' }
}
if (-not (Test-Path (Join-Path $Msys 'etc\pacman.d\gnupg\pubring.kbx'))) { Update-Keyring }

# The first update, after the unpacking (until it succeeds once)
$updated = Join-Path $Msys '.rac1-updated'
if (-not (Test-Path $updated)) {
    Step 'Updating MSYS2'
    # (the first update may update pacman and MSYS2's core, and stop: the second one does the rest)
    Invoke-Msys 'pacman -Syuu --noconfirm' | Out-Null
    Update-Keyring
    if ((Invoke-Msys 'pacman -Syuu --noconfirm') -ne 0) { Fail 'Updating MSYS2 failed. Check the internet connection, then run the setup again.' }
    Update-Keyring
    New-Item -ItemType File $updated | Out-Null
}

# The packages: installed when missing (pacman -T prints the missing ones; offline once everything is there)
$missing = & (Join-Path $Msys 'usr\bin\pacman.exe') -T @Packages
if ($missing) {
    Step "Installing: $($missing -join ', ')"
    if ((Invoke-Msys "pacman -Syu --needed --noconfirm $($Packages -join ' ')") -ne 0) {
        Fail 'Installing the tools failed. Check the internet connection, then run the setup again.'
    }
    Update-Keyring
}

# The compilers themselves: the clang package can be installed with clang.exe missing (an antivirus that removed
# it, an unpacking stopped halfway), and pacman -T then says nothing is missing; CMake finds clang++ but not the C
# compiler (GitHub issue #88). The package is installed again (without --needed) when one of them is missing.
$compilers = @('clang.exe', 'clang++.exe') | ForEach-Object { Join-Path $Msys "clang64\bin\$_" }
if ($compilers | Where-Object { -not (Test-Path $_) }) {
    Step 'Installing the compiler again (clang.exe or clang++.exe is missing)'
    if ((Invoke-Msys 'pacman -S --noconfirm mingw-w64-clang-x86_64-clang') -ne 0) {
        Fail 'Installing the compiler failed. Check the internet connection, then run the setup again.'
    }
    $lost = $compilers | Where-Object { -not (Test-Path $_) }
    if ($lost) {
        Fail ("$($lost -join ', ') is still missing after installing it again. An antivirus (Windows Security too) " +
              "may be removing it: look in its protection history, allow the file (or exclude the folder $ToolDir), " +
              'then run the setup again.')
    }
}

# The certificates CLANG64's programs check HTTPS with (CMake downloading GLFW, Python's update check): its
# ca-certificates package makes them with CLANG64's p11-kit, which breaks in a folder with a space in its path (it
# runs its trust tool by a path cut at the space) and leaves the files empty: "error adding trust anchors" at the
# build's first download (GitHub issues #79, #69, #63). MSYS2's own p11-kit has no such problem: the files are made
# with it when they are missing or empty (also repairing a setup made before this check).
$ssl = Join-Path $Msys 'clang64\etc\ssl'
$bundles = @((Join-Path $ssl 'cert.pem'), (Join-Path $ssl 'certs\ca-bundle.crt'),
             (Join-Path $Msys 'clang64\etc\pki\ca-trust\extracted\pem\tls-ca-bundle.pem'))
if ($bundles | Where-Object { -not (Test-Path $_) -or (Get-Item $_).Length -lt 1024 }) {
    Step 'Preparing the certificates for the downloads'
    $out = '/clang64/etc/pki/ca-trust/extracted'
    $cmd = "mkdir -p $out/pem $out/openssl /clang64/etc/ssl/certs && " +
           "/usr/bin/p11-kit extract --format=pem-bundle --filter=ca-anchors --overwrite --comment --purpose server-auth $out/pem/tls-ca-bundle.pem && " +
           "/usr/bin/p11-kit extract --format=openssl-bundle --filter=certificates --overwrite --comment $out/openssl/ca-bundle.trust.crt && " +
           "cp -f $out/pem/tls-ca-bundle.pem /clang64/etc/ssl/certs/ca-bundle.crt && " +
           "cp -f $out/pem/tls-ca-bundle.pem /clang64/etc/ssl/cert.pem && " +
           "cp -f $out/openssl/ca-bundle.trust.crt /clang64/etc/ssl/certs/ca-bundle.trust.crt"
    Invoke-Msys $cmd | Out-Null
    if ($bundles | Where-Object { -not (Test-Path $_) -or (Get-Item $_).Length -lt 1024 }) {
        # (MSYS2's own bundle, which pacman downloads with, as the last resort)
        $own = Join-Path $Msys 'usr\ssl\certs\ca-bundle.crt'
        if ((Test-Path $own) -and (Get-Item $own).Length -ge 1024) {
            foreach ($b in $bundles) {
                New-Item -ItemType Directory -Force (Split-Path -Parent $b) | Out-Null
                Copy-Item -Force $own $b
            }
        } else {
            Fail 'The certificates for the downloads could not be made. Delete the .toolchain folder, then run the setup again.'
        }
    }
}
exit 0
