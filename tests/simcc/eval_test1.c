
int a = 2, b = 3;
int c = 5;


int fn() {
    return b=a=b+c+3;
}


int main() {
    return a+2+b+(c+(a+b+(a+b+(a+b)+3)))+b;
}