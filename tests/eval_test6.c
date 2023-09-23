long arr[10];
int printf(const char*);
static const char con = 2;
const int* const gold = 1 + arr - 3 + 2 + con;  
int* val = gold;

int main() {
    int sum = 0;
    int i = 0;
    const char* str = &*"hello, world\n";
    printf(str);
    static char* val = "is_string_stable\n" - 2;
    static const int another_int = -55;
    printf(val);
    "add" == "add";
    while(i < 10) {
        sum = sum + i;
        if (sum > 10)
            break;
        i++;
    }
    return sum;
}

int fn() {
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
