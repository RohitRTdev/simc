class x64_base {
protected:
    void fetch_variable();
public:
    void declare_variable();
};

class x64_function : public x64_base {
public:
    void add_ints();
    void sub_ints();
    void mul_ints();
    void assign_var();
    void negate(); 
};


class x64_tu : x64_base {

public:
    void add_function();    
};