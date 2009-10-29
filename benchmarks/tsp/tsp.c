#include <stdio.h>
void tsp(int, int);
int present(int, int);
#define NRTOWNS 14

struct dist {
	int totown;
	int dis;
} distance[NRTOWNS][NRTOWNS] =  {
#include "g6.h"
};

int path[NRTOWNS];
int visited[NRTOWNS];
int best_path[NRTOWNS];
int min = 10000;

int main()
{
	int i;

	tsp(0,0);
	printf("shortest path length is %d\n",min);
	printf("level\tvisited\n");
	for (i=0; i < NRTOWNS; i++) {
		printf("%d\t%d\n",i,visited[i]);
	}
	printf("a best path found: ");
	for (i=0; i < NRTOWNS; i++) {
		printf("%d ", path[i]);
	}
	printf("\n");

	return 0;
}

void tsp(l,len)
{
	int e,me,i;

	if (len >= min) return;  /* pruning */
	visited[l]++;
	if (l == NRTOWNS-1) {
		min = len;
		for (i = 0; i < NRTOWNS; i++) best_path[i] = path[i];
	} else {
		me = path[l];
		for (i=0; i < NRTOWNS; i++) {
			e = distance[me][i].totown;
			if (!present(e,l)) {
				path[l+1] = e;
				tsp(l+1, len + distance[me][i].dis);
			}
		}
	}
}

int present(e,l)
{
	int i;

	for (i = 0; i <= l; i++) {
		if (path[i] == e) return 1;
	}
	return 0;
}


