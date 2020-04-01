/* Expected output:

Line no 20 Called global foo()
Line no 13 called foo/f1 
Line 33 called main/foo .
Line no 29  called main/foo/f1

This test case checks if call gets resolved to their appropriate function depending on their scope.
Case 1: Label with same name as another function.
Case 2: Label with same name as other label. 
*/


#include<stdio.h>

void foo()
{
	printf("Called global foo()\n");

	/*Label with same name as other label.*/
	f1:{
		   printf("called foo/f1 \n");
	   }

	printf("Line no 13 ");
	f1();
}


int main()
{
	printf("Line no 20 ");
	foo(); /*Gets resolved to global function.*/

	/*Label with same name as another function.*/
	foo:{
		printf("called main/foo .\n");
		f1:{
			printf("called main/foo/f1 \n");
		}
		printf("Line no 29  ");
		f1();
	    }
	
	printf("Line 33 ");
	foo();/*Gets resolved to local function.*/
}
