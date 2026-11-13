# =====================================================================
#  Smart Estate - Windows installer
#  Run it by double-clicking INSTALL-WINDOWS.bat in the project folder.
#
#  What it does (it asks before each big step):
#   1. Copies the sketch to  Documents\Arduino\smarthome  (backs up your old one)
#   2. Saves your Wi-Fi name, password and Home ID in secrets.h
#   3. Installs the ESP32 boards and the libraries (arduino-cli)
#   4. Compiles and uploads to the ESP32 on the COM port you choose
#   5. Copies the speaker clips to your micro-SD card
#   6. Opens the web app
# =====================================================================
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # makes downloads much faster

$Root       = Split-Path -Parent (Split-Path -Parent $PSScriptRoot)   # project folder
$SketchSrc  = Join-Path $Root 'firmware\smarthome\smarthome.ino'
$Docs       = [Environment]::GetFolderPath('MyDocuments')
$Sketchbook = Join-Path $Docs 'Arduino'
$SketchDir  = Join-Path $Sketchbook 'smarthome'
$ToolsDir   = Join-Path $env:LOCALAPPDATA 'SmartEstate'
$Cli        = Join-Path $ToolsDir 'arduino-cli.exe'
$Esp32Url   = 'https://espressif.github.io/arduino-esp32/package_esp32_index.json'
$Fqbn       = 'esp32:esp32:esp32'

function Say($text, $color = 'Gray') { Write-Host $text -ForegroundColor $color }
function Step($n, $text) { Write-Host ''; Write-Host "=== $n. $text ===" -ForegroundColor Cyan }
function Ask-YesNo($question, $default = 'y') {
    $hint = if ($default -eq 'y') { '[Y/n]' } else { '[y/N]' }
    $a = Read-Host "$question $hint"
    if ([string]::IsNullOrWhiteSpace($a)) { $a = $default }
    return $a.Trim().ToLower().StartsWith('y')
}
function CEscape($s) { return ($s -replace '\\', '\\' -replace '"', '\"') }
function Run-Cli { & $Cli @args; if ($LASTEXITCODE -ne 0) { throw "arduino-cli $($args -join ' ') failed" } }

Say ''
Say '  SMART ESTATE - Windows installer' 'Green'
Say "  Project folder: $Root"
if (-not (Test-Path $SketchSrc)) {
    Say "Can't find firmware\smarthome\smarthome.ino. Run INSTALL-WINDOWS.bat from inside the unzipped project folder." 'Red'
    Read-Host 'Press Enter to close'; exit 1
}

# ---------------------------------------------------------------------
Step 1 'Copy the sketch to Documents\Arduino\smarthome'
New-Item -ItemType Directory -Force -Path $SketchDir | Out-Null
$target = Join-Path $SketchDir 'smarthome.ino'
if (Test-Path $target) {
    $backup = Join-Path $SketchDir ("smarthome.ino.backup-" + (Get-Date -Format 'yyyyMMdd-HHmmss'))
    Copy-Item $target $backup
    Say "Your old smarthome.ino was saved as $(Split-Path -Leaf $backup)" 'Yellow'
}
Copy-Item $SketchSrc $target -Force
Say "Copied to $target" 'Green'

# ---------------------------------------------------------------------
Step 2 'Your Wi-Fi and Home ID (saved in secrets.h, never on GitHub)'
$secrets = Join-Path $SketchDir 'secrets.h'
$doSecrets = $true
if (Test-Path $secrets) { $doSecrets = Ask-YesNo 'secrets.h already exists. Type new Wi-Fi details?' 'n' }
if ($doSecrets) {
    Say 'The ESP32 only works on 2.4 GHz Wi-Fi (not 5 GHz).'
    $ssid = Read-Host 'Wi-Fi name (SSID)'
    $sec  = Read-Host 'Wi-Fi password' -AsSecureString
    $pass = [Runtime.InteropServices.Marshal]::PtrToStringAuto([Runtime.InteropServices.Marshal]::SecureStringToBSTR($sec))
    $suggest = 'estate-' + -join ((48..57) + (97..122) | Get-Random -Count 8 | ForEach-Object { [char]$_ })
    $homeId = Read-Host "Home ID for online control (Enter = $suggest)"
    if ([string]::IsNullOrWhiteSpace($homeId)) { $homeId = $suggest }
    $content = @"
// Your private settings - made by the Smart Estate installer.
#pragma once
#define WIFI_SSID      "$(CEscape $ssid)"
#define WIFI_PASSWORD  "$(CEscape $pass)"
#define HOME_ID        "$(CEscape $homeId)"
"@
    Set-Content -Path $secrets -Value $content -Encoding ASCII
    Say "Saved $secrets" 'Green'
    Say "Write this down - type it in the app (gear icon > Online): HOME ID = $homeId" 'Yellow'
}

# ---------------------------------------------------------------------
Step 3 'Install the ESP32 boards and libraries'
if (Ask-YesNo 'Download and install them now? (about 500 MB, needs internet)') {
    New-Item -ItemType Directory -Force -Path $ToolsDir | Out-Null
    if (-not (Test-Path $Cli)) {
        Say 'Downloading arduino-cli ...'
        $zip = Join-Path $ToolsDir 'arduino-cli.zip'
        Invoke-WebRequest 'https://downloads.arduino.cc/arduino-cli/arduino-cli_latest_Windows_64bit.zip' -OutFile $zip
        Expand-Archive $zip -DestinationPath $ToolsDir -Force
        Remove-Item $zip
    }
    Run-Cli config init --overwrite
    Run-Cli config set board_manager.additional_urls $Esp32Url
    Run-Cli config set directories.user $Sketchbook
    Say 'Installing ESP32 boards (this takes a few minutes) ...'
    Run-Cli core update-index
    Run-Cli core install esp32:esp32
    Say 'Installing libraries ...'
    Run-Cli lib update-index
    Run-Cli lib install ArduinoJson Espalexa PubSubClient "Adafruit NeoPixel"
    Say 'Boards and libraries are installed. The Arduino IDE will see them too.' 'Green'
}

# ---------------------------------------------------------------------
Step 4 'Compile and upload to the ESP32'
if ((Test-Path $Cli) -and (Ask-YesNo 'Plug the ESP32 into USB now. Compile and upload?')) {
    Say 'Compiling (first time takes 1-3 minutes) ...'
    Run-Cli compile --fqbn $Fqbn $SketchDir
    Say ''
    Say 'Serial ports on this PC:'
    & $Cli board list
    $port = Read-Host 'Type the ESP32 port, e.g. COM5'
    if ($port) {
        Say 'Uploading ... (if it hangs on "Connecting....", hold the BOOT button on the ESP32)' 'Yellow'
        Run-Cli upload -p $port --fqbn $Fqbn $SketchDir
        Say 'Uploaded!' 'Green'
        if (Ask-YesNo 'Open the Serial Monitor to see the IP address? (Ctrl+C to stop)') {
            & $Cli monitor -p $port --config baudrate=115200
        }
    }
} elseif (-not (Test-Path $Cli)) {
    Say 'Skipped: no arduino-cli. Open Documents\Arduino\smarthome\smarthome.ino in the Arduino IDE and press Upload.'
}

# ---------------------------------------------------------------------
Step 5 'Speaker clips to the micro-SD card'
if (Ask-YesNo 'Is the micro-SD card for the DFPlayer in the PC now?' 'n') {
    $drive = Read-Host 'Drive letter of the SD card, e.g. E'
    $drive = $drive.Trim().TrimEnd(':')
    if ($drive -match '^[A-Za-z]$' -and (Test-Path "${drive}:\")) {
        Copy-Item (Join-Path $Root 'sdcard\mp3') "${drive}:\" -Recurse -Force
        Say "Copied the mp3 folder to ${drive}:\mp3" 'Green'
    } else {
        Say "Drive ${drive}: not found - copy the sdcard\mp3 folder by hand." 'Red'
    }
}

# ---------------------------------------------------------------------
Step 6 'Open the web app'
if (Ask-YesNo 'Open the app in demo mode now?') {
    Start-Process (Join-Path $Root 'webapp\index.html')
}

Say ''
Say 'All done!' 'Green'
Say "  Sketch:   $SketchDir"
Say '  App:      http://smartestate.local  (on the same Wi-Fi as the ESP32)'
Say '  Guide:    docs\INSTALL.md and docs\RUN_THE_APP.md'
Read-Host 'Press Enter to close'
