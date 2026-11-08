# rac1-native's tools on Windows, for setup.cmd: a private MSYS2 (its CLANG64 environment) in the repository's
# .toolchain\msys64 folder, with what the setup and the build need. The first run downloads MSYS2 (a fixed release,
# checked against its SHA-256) and the packages; later runs only install what is missing (after a git pull that
# adds a package). Nothing is installed outside .toolchain: deleting the folder removes it all.
# FFmpeg plays the game's movies: the build copies its DLLs beside the program (CMakeLists.txt), so the game also
# starts without MSYS2.
# shaderc (glslc) compiles the Vulkan renderer's shaders at build time.
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'  # (Invoke-WebRequest's progress bar makes it very slow on PowerShell 5)

$MsysRelease = '20260927'
$MsysSha256 = 'ad336cccfda47758b5e15cda993fbba421115cb0b126697daef1ee4dfe37209f'
$MsysUrl = "https://repo.msys2.org/distrib/x86_64/msys2-base-x86_64-$MsysRelease.sfx.exe"
$Packages = @('python', 'cmake', 'clang', 'ninja', 'pkgconf', 'ffmpeg', 'shaderc') | ForEach-Object { "mingw-w64-clang-x86_64-$_" }

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

# The files the packages hold as hard links to another of their files (pacman unpacks them as links): the clang package's
# clang.exe, clang-cpp.exe and the two x86_64-w64-mingw32-clang*.exe are links to clang++.exe (clang picks C, C++ or the
# preprocessor by its own file name, so a copy works the same), lld's lld.exe to ld.exe, and a few tools'. A drive that is
# not NTFS (exFAT or FAT32: an external drive, a USB stick) cannot hold hard links: pacman says "Can't create" for them
# and goes on without them, so clang.exe is missing (GitHub issues #126, #88). The ones missing are copied from their
# target. (The packages' other hard links are manual pages and time zones, which the build does not use.)
$LinkedFiles = [ordered]@{
    'clang.exe' = 'clang++.exe'; 'clang-cpp.exe' = 'clang++.exe'
    'x86_64-w64-mingw32-clang.exe' = 'clang++.exe'; 'x86_64-w64-mingw32-clang++.exe' = 'clang++.exe'
    'lld.exe' = 'ld.exe'; 'llvm-cov.exe' = 'gcov.exe'; 'unzip.exe' = 'bsdunzip.exe'
    'libdeflate-gzip.exe' = 'libdeflate-gunzip.exe'
}
function Repair-LinkedFiles {
    $bin = Join-Path $Msys 'clang64\bin'
    $copied = @()
    foreach ($name in $LinkedFiles.Keys) {
        $file = Join-Path $bin $name
        $target = Join-Path $bin $LinkedFiles[$name]
        if (-not (Test-Path $file) -and (Test-Path $target)) {
            try {
                Copy-Item $target $file
                $copied += $name
            } catch { Step "Could not copy $($LinkedFiles[$name]) to $name ($_)" }
        }
    }
    if ($copied) { Step "Copied $($copied -join ', ') (the package has them as hard links, which this drive cannot hold)" }
}

# The file system of the drive the tools are on ("NTFS", "exFAT", "FAT32", ...; "" when it cannot be known: a network
# folder)
function Get-DriveFormat {
    try { return ([IO.DriveInfo]::new([IO.Path]::GetPathRoot($ToolDir))).DriveFormat } catch { return '' }
}
$driveFormat = Get-DriveFormat
$notNtfs = $driveFormat -and $driveFormat -ne 'NTFS'
if ($notNtfs) {
    Step ("Note: $ToolDir is on a $driveFormat drive, not NTFS: exFAT and FAT32 cannot hold hard links, so the files " +
          "MSYS2's packages have as hard links are copied instead (an NTFS drive avoids this)")
}
Repair-LinkedFiles

# The compilers themselves: the clang package can be installed with clang.exe missing (an antivirus that removed
# it, an unpacking stopped halfway, a drive without hard links: above), and pacman -T then says nothing is missing;
# CMake finds clang++ but not the C compiler (GitHub issue #88). The package is installed again (without --needed)
# when clang++.exe itself is missing, or clang.exe could not be copied from it.
$compilers = @('clang.exe', 'clang++.exe') | ForEach-Object { Join-Path $Msys "clang64\bin\$_" }
if ($compilers | Where-Object { -not (Test-Path $_) }) {
    Step 'Installing the compiler again (clang.exe or clang++.exe is missing)'
    if ((Invoke-Msys 'pacman -S --noconfirm mingw-w64-clang-x86_64-clang') -ne 0) {
        Fail 'Installing the compiler failed. Check the internet connection, then run the setup again.'
    }
    Repair-LinkedFiles
    $lost = $compilers | Where-Object { -not (Test-Path $_) }
    if ($lost) {
        $drive = ''
        if ($notNtfs) {
            $drive = " The folder is on a $driveFormat drive: if this keeps happening, move the game's folder to an NTFS " +
                     'drive (the one Windows is on is NTFS).'
        }
        Fail ("$($lost -join ', ') is still missing after installing it again. An antivirus (Windows Security too) " +
              "may be removing it: look in its protection history, allow the file (or exclude the folder $ToolDir), " +
              "then run the setup again.$drive")
    }
}

# The libraries every program is linked with (the C runtime, its headers, libc++, ...): their packages can be installed
# with files missing (an antivirus that removed one, an update or an unpacking stopped halfway), and pacman -T then says
# nothing is missing: CMake's first test program does not link ("lld: error: unable to find library -lmsvcrt", GitHub
# issue #140). pacman -Qk counts each package's missing files; a package that misses one (or is not installed) is
# installed again (without --needed). (None of these packages holds a hard link: on any drive, all their files can be
# there.)
$Runtime = @('crt', 'headers', 'winpthreads', 'libc++', 'libunwind', 'compiler-rt') | ForEach-Object { "mingw-w64-clang-x86_64-$_" }
function Get-DamagedRuntime {
    $pacman = Join-Path $Msys 'usr\bin\pacman.exe'
    # (pacman's warnings go to stderr, which PowerShell 5 turns into errors that stop the script unless the preference
    # is relaxed here; LC_ALL=C: its summary lines in English, "<package>: <n> total files, <m> missing files")
    $ErrorActionPreference = 'Continue'
    $lcAll = $env:LC_ALL
    $env:LC_ALL = 'C'
    try {
        $absent = @(& $pacman -T @Runtime 2>$null)
        $report = @(& $pacman -Qk @Runtime 2>$null) -join "`n"
    } finally { $env:LC_ALL = $lcAll }
    return @($Runtime | Where-Object {
        $m = [regex]::Match($report, '(?m)^' + [regex]::Escape($_) + ': \d+ total files?, (\d+) missing files?')
        ($absent -contains $_) -or ($m.Success -and [int]$m.Groups[1].Value -gt 0)
    })
}
$damaged = Get-DamagedRuntime
if ($damaged) {
    Step "Installing again: $($damaged -join ', ') (some of their files are missing)"
    if ((Invoke-Msys "pacman -S --noconfirm $($damaged -join ' ')") -ne 0) {
        Fail 'Installing the libraries failed. Check the internet connection, then run the setup again.'
    }
    $damaged = Get-DamagedRuntime
    if ($damaged) {
        Fail ("Files of $($damaged -join ', ') are still missing after installing them again. An antivirus (Windows " +
              "Security too) may be removing them: look in its protection history, allow the files (or exclude the " +
              "folder $ToolDir), then run the setup again. If it keeps failing, delete the folder $ToolDir and run the " +
              'setup again (it downloads the tools afresh).')
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
