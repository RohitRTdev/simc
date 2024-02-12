//This is a comment
/*This is a block comment*/ Hey there
#define \
GOOD  /*Some messed up comment

*/
ANOTHER

#define ANOTHERGOOD #GOOD good
#define ANOTHERGOODTHIRD \
3
# define ANOTHER (1+ANOTHER ##GOOD# 1);
#define REPLACE_THIS ANOTHER
int square(int num) {
    return nu\
m * num;
}

int main() {
    REPLACE\
_THIS;
}

#define function_macro(some, SEE, IF) some MACRO function_macro12(1,1,1) arg are\
 REPLACE_THIS. Let's SEE IF it works
#define SEVEN __VA_ARGS__

function_macro(23+2,
    function_macro(2,2,2) ## 3,a_23 * 5);

#define var_macro(a, b, ...) #a b+2 __VA_ARGS__ #__VA_ARGS__

var_macro(2,5+/\
/This is a comment
/*Block comment */

    SEVEN, three, there, 12##2, hehe)
#define blah a# #gogo
/Block comment
*/blah

#define NEW_MACRO var##_macro(2,5+3,works, ifits, there)

#define john(a) #a
#define SOME(a,b) great john
#define GORY(a,b) a+b>>SOME(1,1)
#define VALUE GORY(12,3)(11)
VALUE


#define double(x) (2*(x))
#define call_with_1(x) x(1)
call_with_1(double)

