unsigned char volatile good_old_function(int param1, void (param2)(), int **ptr_param) {

    long int f;
    unsigned char m;
    short j;
    f = 66000;
    j = f;
    m = f * j + 1;
    f = m * j;

    return m;
}