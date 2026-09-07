@echo off
:start
echo ============================================================================ >> umrc.log
echo %date%%time% Starting umrc-bridge.exe >> umrc.log
echo ============================================================================ >> umrc.log
umrc-bridge.exe
:end
goto :start
