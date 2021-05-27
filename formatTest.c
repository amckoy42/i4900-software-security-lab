#include <stdio.h>

int main(){
    const int n = 128;
    char buffer[n];
    int target = 4660; //0x1234
    fgets(buffer, n, stdin);
    printf(buffer);
    if(target != 4660){
        printf("target overwritten\n");
    }

    return 0;
}