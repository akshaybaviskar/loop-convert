#include"labels.h"
#include<stdio.h>

int main()
{
		  int a[5] = {0,1,2,0,4};
		  int i,j,k,l;

		  struct s_main my_s;
		  my_s.a = &a;
		  my_s.i = &i;
		  my_s.j = &j;
		  my_s.k = &k;
		  my_s.l = &l;



		  main_f1(&my_s);

}
void main_f1_print(struct s_main_f1 *par_s)
{
		  for((*(par_s->s->j))=0;(*(par_s->s->j))<3;(*(par_s->s->j))++)
		  {
					 for((*(par_s->s->k))=0;(*(par_s->s->k))<6;(*(par_s->s->k))++)
					 {
								for((*(par_s->s->l))=0;(*(par_s->s->l))<6;(*(par_s->s->l))++)
								{
										  if((*(par_s->p)) == 1)
													 printf("%d ",(par_s->b)[(*(par_s->s->j)) * 6 * 6+(*(par_s->s->k)) * 6+(*(par_s->s->l))]);
										  else
													 (par_s->b)[(*(par_s->s->j)) * 6 * 6+(*(par_s->s->k)) * 6+(*(par_s->s->l))] = 0;
								}
								printf("\n");
					 }

					 printf("\n \n");
		  }
}

void main_f1(struct s_main *par_s)
{
		  int b[3][6][6];
		  int p;

		  struct s_main_f1 my_s;
		  my_s.s = par_s;
		  my_s.b = &b;
		  my_s.p = &p;


		  //Initialize
		  p = 0;
		  main_f1_print(&my_s);


		  for((*(par_s->i))=0;(*(par_s->i))<5;(*(par_s->i))++)
		  {
					 b[0][(par_s->a)[(*(par_s->i))]][(par_s->a)[(*(par_s->i))]] = (par_s->a)[(*(par_s->i))];
		  }

		  p = 1;	
		  main_f1_print(&my_s);

}


