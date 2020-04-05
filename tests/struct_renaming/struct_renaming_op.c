#include"labels.h"
/*
	This test case checks when structures are declared inside function/labels.

	Case 1: Structure declared in function and it's structure variable used in a nested function.
	Case 2: Structure declared in function has same name as structure in other function/ global structure. (Structure Renaming) 

	Expected output:
	1. Structure declarations are hoisted outside so as to enable access to nested functions.
	2. Structures are renamed to avoid compilation error when same name structures are declared in other nested functions.
	3. Structure variable declarations are resolved and rewritten to match corresponding structure.
	4. Structure variable declarations inside the structures are renamed.
 */

#include<stdio.h>

struct struct1{
		  int a;
		  int b[5];
};

//Case 1: Structure declared in function and it's structure variable used in a nested function.
//Case 2: Structure declared in function has same name as structure in other function/ global structure. (Structure Renaming) 
void cases()
{
		  struct struct1 i; // refers to global structure



		  struct cases_struct2 struct2_var; // refers to struct2 in cases
		  struct2_var.r = 8;


		  struct s_cases my_s;
		  my_s.i = &i;
		  my_s.struct2_var = &struct2_var;

}


void cases_f1_f2(struct s_cases_f1 *par_s)
{


		  struct cases_f1_f2_node ll;
		  ll.data = 8;
}

void cases_f1_f3(struct s_cases_f1 *par_s)
{


		  struct cases_f1_f3_node ll; // this shall resolve to node declaration in case/f1/f3
		  ll.my_data = 4; // otherwise this will throw compiler error.
}

void cases_f1(struct s_cases *par_s)
{
		  /*This represents that structure variable with same type in same function resolves to two different 
			 declarations based on scope. 
			 jay and viru both are of type struct1, but jay resolves to global struct1 
			 and viru resolves to struct1 inside f1.			
			*/
		  struct struct1 jay;  

		  struct cases_f1_struct1 viru;

		  jay.b[2] = 4;
		  viru.x.r = 5; 		// This line will throw compiler error, if structures are not resolved properly.

		  struct s_cases_f1 my_s;
		  my_s.s = par_s;
		  my_s.jay = &jay;
		  my_s.viru = &viru;



}
int main()
{
		  cases();
}
