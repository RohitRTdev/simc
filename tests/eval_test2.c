
int a = 2, b = 3;
int c = 5;


int fn() {

    a=a+1;
    b=b+5;
    return c=a=b+c+3;
}

int main() {
    a = a + 1 + b + c;

    return a;
}
