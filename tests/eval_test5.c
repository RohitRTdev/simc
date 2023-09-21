int printf(const char*, int);

const int arr[4];
const int d = 2;
const char val = 2;
int where = 4;
int* global = ((1 && (val - 2)) || arr) + &where;

int main() {
    unsigned int a;
    short b;

    a = ++global;
    char arr[4];
    const int con = -1;
    arr[0] = '%';
    arr[1] = 'd';
    arr[2] = '\n';
    arr[3] = '\0';    
    b = 25;

    int d = 1/0;
    int *ptr = &con;
    ptr = ptr - con;

    if(a) {
        while(a < b)
            printf(arr, a++);
            printf(arr, 1+1);
    }
    return a;
}