
int a = 2, b = 10;
int c = 5;


int fn() {
    b+1;
    a=a-1;
    b=5-b;
    return (c=a+1)=b;
}

int main1() {
    a = a + 1 + b + c;

    return a;
}
