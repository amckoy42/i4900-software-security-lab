#include <stdio.h>

int main(){
    char buffer[32];
    printf("%p\n", buffer);
    gets(buffer);
    printf("string read: %s\n", buffer);

    return 0;
}