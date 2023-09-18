int main() {
    int a;
    short b;
    a = 26;
    b = 5;
    int d[4];
    int c;
    c = 0;
    d[c] = 88;
    d[c+3] = 3;

    a = (a + 1) + (2 / a);
    b = 3 % (a + 1);

    return d[b];
}