@REM "nixieclock.c"をコンパイルするためのバッチファイルです。
@REM 本バッチはGDLから使用されるものではありません。
@REM GDLを使用しないで単体でコンパイルする際に利用してください。
@SET PATH=C:\PROGRA~1\BestTech\GCCDEV~1\GCC\H8\bin;c:\PROGRA~1\BestTech\GCCDEV~1\GCC\H8\libexec\gcc\H8300-~1\45-GNU~1.02;c:\PROGRA~1\BestTech\GCCDEV~1\GCC\H8\H8300-~1\bin
h8300-elf-gcc -I C:\PROGRA~1\BestTech\GCCDEV~1\TARGET\3694F -L C:\PROGRA~1\BestTech\GCCDEV~1\TARGET\3694F -mh -mn -nostartfiles -fsigned-char -Wall -Wno-main -pipe -O0 -TC:\PROGRA~1\BestTech\GCCDEV~1\TARGET\3694F\h8rom.x -Xlinker -Map -Xlinker "nixieclock.map" C:\PROGRA~1\BestTech\GCCDEV~1\TARGET\3694F\h8crt0.s "nixieclock.c" -ladd3694 -lm -o "nixieclock.exe" 
h8300-elf-objcopy -O srec  "nixieclock.exe" "nixieclock.mot"
PAUSE
