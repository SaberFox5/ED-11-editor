# aqui esta um tutorial de como compilar codigo escrito em C juntamente com a Raylib
# gcc main.c -o main -lraylib -lm -lpthread -ldl -lrt -lX11


# compilar a biblioteca libedlogic.so
gcc main.c -o ED_11 -shared -fPIC edlogic.c -o libedlogic.so

# e para correr:
LD_LIBRARY_PATH=. ./main


# caso nada funcione corre se assim:
gcc -Wall -Wextra main.c -o ed11 -lraylib -lm -lpthread -ldl -lrt -lX11 2>&1 | head -60

# caso tenha compilado da forma anterior corre se o ficheiro normalmente:
./main


## compilar para windows 64 bits
cd ~/Documentos/Testing_grounds/src
x86_64-w64-mingw32-gcc main.c -o ed11_64.exe -Iwindows-raylib/include -Lwindows-raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -mwindows -static
file ed11_64.exe

## compilar para windows 32 bits
i686-w64-mingw32-gcc main.c -o ed11_32.exe -Iwindows32-raylib/include -Lwindows32-raylib/lib -lraylib -lopengl32 -lgdi32 -lwinmm -mwindows -static