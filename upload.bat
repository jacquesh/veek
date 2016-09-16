@echo off
REM Store the current time and date for later use
For /f "tokens=1-4 delims=/ " %%a in ("%DATE%") do (set FileDate=%%a-%%b-%%c)
For /f "tokens=1-2 delims=/:/ " %%a in ("%TIME%") do (set FileTime=%%a-%%b)

REM Compile the program (after getting the datetime so that its consistent with the compilation)
call compile.bat

REM Zip the file, upload it to the server, and then archive it
7z a veek.zip build/main.exe
scp veek.zip veek_user@ec2-54-171-205-121.eu-west-1.compute.amazonaws.com:~/bin
mv veek.zip build/veek_%FileDate%_%FileTime%.zip

