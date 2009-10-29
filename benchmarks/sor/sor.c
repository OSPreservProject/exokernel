#include "stdio.h"

#define NCOL	80
#define NROW	482

/* Tolerance is 0.001.
 * Omega and stopdiff are defined by the following equations:
 *		r = 0.5 * (cos(pi / NCOL) + cos( pi / NROW))
 *  	om = 2 / (1 + sqrt(1 - r * r))
 *  	stoffdiff = 0.001/( 2 - om)
 */

#define OMEGA		1.945254
#define STOPDIFF 	0.01826619

#define ABS(x)				(((x) > 0) ? (x) : -(x))
#define NEWVALUE(m, i, j)	((m[i-1][j] + m[i+1][j] + m[i][j-1] + m[i][j+1])/ 4.0)

float matrix[NROW][NCOL];

#ifdef PRINTMATRIX
static void PrintMatrix(float m[NROW][NCOL])
{
    int i;
    int j;
    
    for (i = 1; i < NROW - 1; i++) {
	for (j = 0; j < NCOL; j++) {
	    printf("%f ", m[i][j]);
	    
	}
	printf("\n");
    }
}
#endif


static void InitMatrix(float m[NROW][NCOL])
{
    int i;
    int j;
    
    for (i = 0; i < NROW; i++) {
	for (j = 0; j < NCOL; j++) {
	    if (i == 0) m[i][j] = 4.56;
	    else if (i == NROW - 1) m[i][j] = 9.85;
	    else if (j == 0) m[i][j] = 7.32;
	    else if (j == NCOL - 1) m[i][j] = 6.88;
	    else m[i][j] = 0;
	}
    }
}


static void SOR(float m[NROW][NCOL])
{
    int i;
    int j;
    int iter;
    float newvalue;
    float diff;
    float maxdiff;
    
    iter = 0;
    do {
	iter++;
	maxdiff = 0;
	for (i = 1; i < NROW - 1; i++) {
	    for (j = 1; j < NCOL - 1; j++) {
		newvalue = NEWVALUE(m, i, j);
		diff = ABS(newvalue - m[i][j]);
		if (diff > maxdiff) maxdiff = diff;
		m[i][j] = m[i][j] + OMEGA * (newvalue - m[i][j]);
	    }
	}
    } while (maxdiff > STOPDIFF);
    printf("iter: %d\n", iter);
}


int main()
{
  InitMatrix(matrix);
  SOR(matrix);
#ifdef PRINTMATRIX
  PrintMatrix(matrix);
#endif
  return 0;
}
