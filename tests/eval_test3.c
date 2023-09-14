
int printf(char* str, int con);
int arr[3], some(int, int);

static int (*new_fn(int* val))(const char*, int) {
    *val = *val + 1;
    return &printf;
}


unsigned long long fn(const char* str, int con) {
    void (*fn_ptr)(int, char, int*, int [3], int [2][3], int some(char, char), char val,
    short, unsigned long long), new_var(int, char), goat(int, int);
    int returns_sum(int, int);
    char test;
    test = 'm';
    int val;
    val = *str;
    test = con > val;
    test = con < val;
    val = con == val;
    val = (con != val) + test;
    return new_fn(&con)(str, con);
}
