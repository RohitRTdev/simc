int main() {
    unsigned int a;
    short b;
    a = 26;
    b = 5;

    a = a / b;
    a = a % b;
    a = a >> b;
    a = a << 3;
    a = a >> 1;
    b = 4 << b;
    a = 3 >> a;
    a = a << b;
    return a;
}