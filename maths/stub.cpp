/*===================================================================

  
  digital liberation front 2001
  
  _______    ______      _______
 /______/\  |______|    /\______\  
|       \ \ |      |   / /       |    
|	      \| |      |  |/         |  
|_____    \ |      |_ /    ______|       
 ____|    | |      |_||    |_____          
     |____| |________||____|                
           



Code by Nicholas Chapman
nickamy@paradise.net.nz

You may use this code for any non-commercial project,
as long as you do not remove this description.

You may not use this code for any commercial project.
====================================================================*/
#include "matrix.h"
#include "matrix3.h"
#include "vec3.h"
#include <conio.h>
#include <stdio.h>
#include "maths.h"
#include "plane2.h"

void main()
{
	Matrix3 m1(Vec3(10, 6, 3), Vec3(-98645, 5, 5), Vec3(3, 7, 10));
	//Matrix3 m1(NICK_PI/4, 0, 0);

	//Vec3 veca(1, 0, 0);

	//Vec3 transformed = m1 * veca;

	

	m1.print();

	Matrix3 m2 = Matrix3::identity();

	printf("\n");

	m2.print();

	Matrix3 m3 = m1 * m1;

	printf("\n");

	m3.print();

	printf("\n");

	//transformed.print();

	//Matrix3::identity.print();


	Plane(Vec3(0,0,0), Vec3(1, 0, 0));


	getch();





}