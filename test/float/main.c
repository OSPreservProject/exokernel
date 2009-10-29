#include <math.h>
#include <stdio.h>

float m1[2][2] = {{2.0, 3.0},
		  {4.0, -1.0}};
float m2[2][2] = {{-2.0, -5.0},
		  {9.0, 6.0}};
float a_known[2][2] = {{23.0, 8.0},
		       {-17.0, -26.0}};
float a[2][2];

int main () {
  int x, y, z;
#define ITERS 1000000
  int iters = ITERS;

  while (iters--) {
    for (x = 0; x < 2; x++) {
      for (y = 0; y < 2; y++) {
	a[x][y] = 0;
      }
    }
    
    for (x = 0; x < 2; x++) {
      for (y = 0; y < 2; y++) {
	for (z = 0; z < 2; z++) {
	  a[x][y] += (m1[x][z] * m2[z][y]);
	}
      }
    }

    for (x = 0; x < 2; x++) {
      for (y = 0; y < 2; y++) {
	if (a[x][y] != a_known[x][y]) {
	  printf ("mismatch at a[%d][%d] = %f\n", x, y, a[x][y]);
	  return -1;
	}
      }
    }
  }

  printf("Ok\n");
  return 0;
}
    
