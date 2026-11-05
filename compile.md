# aqui esta um tutorial de como compilar codigo escrito em C juntamente com a Raylib
# gcc main.c -o main -lraylib -lm -lpthread -ldl -lrt -lX11


# compilar a biblioteca libedlogic.so
gcc main.c -o ED_11 -shared -fPIC edlogic.c -o libedlogic.so

# e para correr:
LD_LIBRARY_PATH=. ./main