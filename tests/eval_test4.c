
const int * d[2];

int** new_fn(int,int val) {
    return val + val;
}

int main() {
    int a, b, c;

    a = 11;
    b = 23;
    c = 35;
    b = (a,b,c);
    a = new_fn(a, (1, b));

    if(a < b) {
        a = a+b;
    }
    else if(a!=b)
        if(b == 0)a++;
        else if(0)a--;
        else {
            a = a + 2;
            int five;
            five = 2;
        }


    return a;
}
