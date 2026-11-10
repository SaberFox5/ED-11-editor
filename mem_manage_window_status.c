#include <stdio.h>
#include <stdlib.h>
#include <raylib.h>
#include <threads.h>

static void show_err(){
    size_t n = 15;
    float midl = GetScreenWidth() / 2;
    float midly = GetScreenHeight() / 2;
    char *ptr = (int *)malloc(n);
    for (int i = 0; i < n; i++){
        ptr[i] = i + 1;
        if (ptr == NULL){
            printf("Unsuficient memory closing window");
            struct timespec time;
            time.tv_sec = 5;
            time.tv_nsec = 0;
            thrd_sleep(&time, NULL);
            CloseWindow();
            return 1;
        }
    }


    char state[] = "stable";
    if (strlen(state, "stable")){
        printf("[OK]stable editor");
        Setstate("Stable");
        return 0;
    }
    

    if (IsKeyPressed(KEY_LEFT_CONTROL) || IsKeyPressed(KEY_RIGHT_CONTROL)){
        DrawText("Status: %s", state, midl, midly, DARKGRAY);
        printf("Displaying window status");
    }
}

static int main(){
    show_err();
    if (!show_err){
        printf("failed to load function");
        return -1; //significa que algo correu mal na funcao show_err
    }
    return 0;
}