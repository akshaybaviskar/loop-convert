#include"labels.h"
#include<stdio.h>

void foo()
{
		  printf("Called global foo()\n");

		  /*Label with same name as other label.*/

		  struct s_foo my_s;


		  printf("Line no 13 ");
		  foo_f1(&my_s);
}


int main()
{
		  printf("Line no 20 ");
		  foo();

		  /*Label with same name as another function.*/

		  struct s_main my_s;


		  printf("Line 33 ");
		  main_foo(&my_s);
}
void main_foo(struct s_main *par_s)
{
		  printf("called main/foo .\n");
		  struct s_main_foo my_s;
		  my_s.s = par_s;

		  printf("Line no 29  ");
		  main_foo_f1(&my_s);
}

void main_foo_f1(struct s_main_foo *par_s)
{
		  printf("called main/foo/f1 \n");
}

void foo_f1(struct s_foo *par_s)
{
		  printf("called foo/f1 \n");
}

