
const int * d[2];

int** new_fn(int,int val) {
    return val + val;
}

int main() {
    int a, b, c;

    {
        int some;
        some = 10;
        {
            if(some) {
                int four;
                four = 5;
                {
                    return 2;
                }
                return four;
                four++;
            }
            else {
                int my;
                my = 2;
                {
                    {
                        if(1) {
                            return my+2;
                        }
                        return my+1;
                    }
                    int my;
                    return my;
                    my++;
                }
                return 5;
                my++;
            }
            return 1;
        }

        {

        }
        some = 1;
    }
    

    if(a < b) {
        a = a+b;
    }
    else if(a!=b)
        if(b == 0) { 
            a++;
            return 1;
            a--;
            int should_no_get_initialized;
            should_no_get_initialized = 3;
        }
        else if(74)a--;
        else {
            a = a + 2;
            int five;
            return five;
            five = 2;
        }

    a = (a++)+(++b);
    return a;
}
