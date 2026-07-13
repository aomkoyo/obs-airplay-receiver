@echo off
REM ============================================
REM OBS AirPlay Receiver - Uninstaller
REM Run as Administrator
REM ============================================

set "PLUGIN_DIR=%PROGRAMDATA%\obs-studio\plugins\obs-airplay-receiver"

echo.
echo OBS AirPlay Receiver - Uninstaller
echo ====================================
echo.

if not exist "%PLUGIN_DIR%" (
    echo Plugin is not installed.
    echo Directory not found: %PLUGIN_DIR%
    echo.
    pause
    exit /b 0
)

echo Removing: %PLUGIN_DIR%
rmdir /s /q "%PLUGIN_DIR%"
if errorlevel 1 (
    echo.
    echo ERROR: Failed to remove plugin directory. Please run as Administrator.
    echo Make sure OBS Studio is closed.
    pause
    exit /b 1
)

echo Removing firewall rules...
netsh advfirewall firewall delete rule name="OBS AirPlay Receiver (UDP)" >nul 2>&1
netsh advfirewall firewall delete rule name="OBS AirPlay Receiver (TCP)" >nul 2>&1

echo.
echo ============================================
echo Uninstall successful!
echo.
echo The plugin has been removed.
echo Restart OBS Studio for changes to take effect.
echo ============================================
echo.
pause
