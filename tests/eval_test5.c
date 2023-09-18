int main() {
    int a;
    short b;
    a = 26;
    b = 5;

    a = a / b;
    a = a % b;
    a = a % 3;
    a = 10 / b;
    a = a & b;
    a = a ^ b;
    a = 3 & b;
    b = a | 10;
    a = b | a;

    return a;
}