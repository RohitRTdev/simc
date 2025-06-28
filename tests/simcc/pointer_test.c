//Signature (Need this type of signature since we don't support variable arg functions)
int printf(char* fstr, char* ustr, int d);

int main()
{
    char* s[5];
    s[0] = "ice",s[1] = "cube",s[2] = "break";
    s[3] = "got", s[4] = "join";

    void** ptr[4];
    ptr[0] = s+3;
    ptr[1] = s+2;
    ptr[2] = s;
    ptr[3] = s+1;

    char*** p = ptr;

    printf("%s %d\n", *--*++p+2, 3);

    return 0;
}
