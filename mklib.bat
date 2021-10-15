@echo off
set CFLAGS=/MT /DNOMINMAX /Ox /DNDEBUG /D_CRT_SECURE_NO_WARNINGS /openmp /W4 /Zi /EHsc /nologo -I./include -I./src -I./src/x64/
if "%1"=="dll" (
  echo make dynamic library DLL
  set CFLAGS=%CFLAGS% /DSG_NO_AUTOLINK
) else (
  echo make static library LIB
)

echo cl /c %CFLAGS% src\main.cpp /Foobj\main.obj
     cl /c %CFLAGS% src\main.cpp /Foobj\main.obj

if "%1"=="dll" (
  echo link /nologo /DLL /OUT:bin\simdgen.dll obj\main.obj %LDFLAGS% /implib:lib\simdgen.lib
       link /nologo /DLL /OUT:bin\simdgen.dll obj\main.obj %LDFLAGS% /implib:lib\simdgen.lib
) else (
  echo lib /nologo /OUT:lib\simdgen.lib /nodefaultlib obj\main.obj
       lib /nologo /OUT:lib\simdgen.lib /nodefaultlib obj\main.obj
)
