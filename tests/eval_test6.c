long arr[10];
int printf(const char*);
static const char* con = "fails" == "fails";
const char* ptr = "";

int test_this(int a, int b);

int test_this(int a, int b) {
    int flag = 0;
    if (flag)
                return 2;
            else if(2)
                return 4;
            else if(5)
                return 2;

    return 1;
}

int return_void() {
    return 2;
}
char val = 'a';

int main() {
    int sum = 1, i = 0;
    const char* str = &*"";
    printf(str);
    ptr++;
    static char* val = "is_string_stable\n";
    static const int another_int = -55;
    printf(val);
    "add" == "add";
    return_void() && val;
    while(i < 10) {
        sum = sum + i;
        if (sum > 10)
            continue;
        i++;
    }
    return sum;
}

int fn() {
    

    int (*(*s[2]))(int, const char(char [2], register char));
    
    (*s[0][1])(0, fn);
    int sum = 0;
    int i = 0;
    static const int another_int;
    another_int + 1;
    while (i < 10) {
        i = i + 1;
        if (i % 2)
            continue;
        sum = sum + i;
    }
    return sum;
}

int another() {
    int sum = 0;
    int i = 0;
    while (i < 10) {
        if ((sum / 2) * 2 != sum)
            continue;
        sum = sum + i;
        i = i + 1;
    }
    return sum;
}
