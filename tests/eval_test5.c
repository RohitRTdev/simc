void test() {

}

int printf(const char*, int);

int main() {
    unsigned int a;
    short b;

    char arr[4];
    int con;
    con = 0;
    arr[con] = '%';
    arr[con+1] = 'd';
    arr[con+2] = '\n';
    arr[con+3] = '\0';    
    a = 1;
    b = 25;


    if(a) {
        while(a < b)
            printf(arr, a++);
            printf(arr, b+1);
    }
    return a;
}