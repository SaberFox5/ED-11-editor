#include <stdio.h>

int main(void){
    int *block = (int)malloc(15 * 1024 * 1024);
    //obriga o OS a alocar memoria na Cache
    for (int i = 0; i < 15 * 1024 * 1024; i++){
        if (!block){
            printf("ERROR: erro ao tentar alocar a memoria a encerrar o processo\n");
            return 1;
        }
        block[i] = i;
    }
    
    return 0;
}
