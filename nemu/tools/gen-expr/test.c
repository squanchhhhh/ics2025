#include <stdio.h>
#include <stdlib.h>

int choose(int n){
    return rand()%n;
}
int main(){
    int i = 10;
    while(i-->0){
        printf("%d\n",choose(3));
    }
    return 0;
}