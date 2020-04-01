
struct s_foo{
};
struct s_foo_f1{
struct s_foo *s;
};

struct s_main{
};
struct s_main_foo{
struct s_main *s;
};
struct s_main_foo_f1{
struct s_main_foo *s;
};

void foo_f1(struct s_foo *par_s);
void main_foo(struct s_main *par_s);
void main_foo_f1(struct s_main_foo *par_s);
