@echo off
set CFLAGS=/MT /DNOMINMAX /Ox /DNDEBUG /openmp /W4 /Zi /EHsc /nologo -I./include -I./src
set LDFLAGS=/LIBPATH:.\lib
if "%1"=="-s" (
  echo use static lib
  set CFLAGS=%CFLAGS% /DSG_DONT_EXPORT
) else if "%1"=="-d" (
  echo use dynamic lib
) else (
  echo "mk (-s|-d) <source file>"
  goto exit
)
set SRC=%2
set EXE=%SRC:.cpp=.exe%
set EXE=%EXE:.c=.exe%
set EXE=%EXE:test\=bin\%
set EXE=%EXE:sample\=bin\%
echo cl %CFLAGS% %2 /Fe:%EXE% /link %LDFLAGS%
cl %CFLAGS% %2 /Fe:%EXE% /link %LDFLAGS%

:exit
