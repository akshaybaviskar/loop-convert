struct cases_struct2{
		  int r;
};
struct cases_f1_struct1{
		  struct cases_struct2 x;             // x shall resolve to struct2 in cases(). (Expected output 4)
		  int y;
};
struct cases_f1_f2_node{
		  struct cases_f1_f2_node* node;  // struct member refering to same struct shall also get renamed. (Expected Output 4)
		  int data;
};
struct cases_f1_f3_node{
		  struct cases_f1_f3_node* node;
		  int my_data;
};

struct s_cases{
		  struct struct1* i;
		  struct cases_struct2* struct2_var;
};
struct s_cases_f1{
		  struct s_cases *s;
		  struct struct1* jay;
		  struct cases_f1_struct1* viru;
};
struct s_cases_f1_f2{
		  struct s_cases_f1 *s;
		  struct cases_f1_f2_node* ll;
};
struct s_cases_f1_f3{
		  struct s_cases_f1 *s;
		  struct cases_f1_f3_node* ll;
};

void cases_f1(struct s_cases *par_s);
void cases_f1_f2(struct s_cases_f1 *par_s);
void cases_f1_f3(struct s_cases_f1 *par_s);
