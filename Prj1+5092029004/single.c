/********************************************
* Problem3 description:
* This is a single-thread program for matrix
* multiplication by the partition based algorithm
* with time complexity O(n3)
*********************************************/

#include <stdio.h>
#include <stdlib.h>

typedef int** mat;

mat newmat(int n) {
	mat m;
	int i;
	m = malloc(n * sizeof(int*));
	for (i = 0; i < n; i++) m[i] = malloc(n * sizeof(int));
	return m;
}

void delmat(mat m, int n) {
	int i;
	for (i = 0; i < n; i++) free(m[i]);
	free(m);
}

void matcopy(mat src, int r, int c, int n, mat dst) {
	int i, j;
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			dst[i][j] = src[r + i][c + j];
}

void matrcopy(mat src, mat dst, int r, int c, int n) {
	int i, j;
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			dst[r + i][c + j] = src[i][j];
}

void matmul_naive(mat a, mat b, mat c, int n) {
	int i, j, k;
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++) {
			c[i][j] = 0;
			for (k = 0; k < n; k++)
				c[i][j] += a[i][k] * b[k][j];
		}
}

void matadd(mat a, mat b, mat c, int n) {
	int i, j;
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			c[i][j] = a[i][j] + b[i][j];
}

void matmul(mat a, mat b, mat c, int n) {
	mat a11, a12, a21, a22;
	mat b11, b12, b21, b22;
	mat a11b11, a12b21;
	mat a11b12, a12b22;
	mat a21b11, a22b21;
	mat a21b12, a22b22;
	mat c11, c12, c21, c22;
	int k = n / 2;
	
	a11 = newmat(k);
	a12 = newmat(k);
	a21 = newmat(k);
	a22 = newmat(k);
	b11 = newmat(k);
	b12 = newmat(k);
	b21 = newmat(k);
	b22 = newmat(k);
	
	matcopy(a, 0, 0, k, a11);
	matcopy(a, 0, k, k, a12);
	matcopy(a, k, 0, k, a21);
	matcopy(a, k, k, k, a22);
	matcopy(b, 0, 0, k, b11);
	matcopy(b, 0, k, k, b12);
	matcopy(b, k, 0, k, b21);
	matcopy(b, k, k, k, b22);
	
	a11b11 = newmat(k);
	a12b21 = newmat(k);
	a11b12 = newmat(k);
	a12b22 = newmat(k);
	a21b11 = newmat(k);
	a22b21 = newmat(k);
	a21b12 = newmat(k);
	a22b22 = newmat(k);
	
	matmul_naive(a11, b11, a11b11, k);
	matmul_naive(a12, b21, a12b21, k);
	matmul_naive(a11, b12, a11b12, k);
	matmul_naive(a12, b22, a12b22, k);
	matmul_naive(a21, b11, a21b11, k);
	matmul_naive(a22, b21, a22b21, k);
	matmul_naive(a21, b12, a21b12, k);
	matmul_naive(a22, b22, a22b22, k);
	
	delmat(a11, k);
	delmat(a12, k);
	delmat(a21, k);
	delmat(a22, k);
	delmat(b11, k);
	delmat(b12, k);
	delmat(b21, k);
	delmat(b22, k);
	
	c11 = newmat(k);
	c12 = newmat(k);
	c21 = newmat(k);
	c22 = newmat(k);
	
	matadd(a11b11, a12b21, c11, k);
	matadd(a11b12, a12b22, c12, k);
	matadd(a21b11, a22b21, c21, k);
	matadd(a21b12, a22b22, c22, k);
	
	delmat(a11b11, k);
	delmat(a12b21, k);
	delmat(a11b12, k);
	delmat(a12b22, k);
	delmat(a21b11, k);
	delmat(a22b21, k);
	delmat(a21b12, k);
	delmat(a22b22, k);
	
	matrcopy(c11, c, 0, 0, k);
	matrcopy(c12, c, 0, k, k);
	matrcopy(c21, c, k, 0, k);
	matrcopy(c22, c, k, k, k);
	
	delmat(c11, k);
	delmat(c12, k);
	delmat(c21, k);
	delmat(c22, k);
}

int main(int argc, char** argv) {
	int n, i, j;
	mat a, b, c;
	
	if (argc == 1) {
		if (freopen("data.in", "r", stdin) == 0) {
			fprintf(stderr, "Failed to open data.in\n");
			exit(EXIT_FAILURE);
		}
		if (freopen("data.out", "w", stdout) == 0) {
			fprintf(stderr, "Failed to open data.out\n");
			exit(EXIT_FAILURE);
		}
	}
	
	if (scanf("%d", &n) != 1) {
		fprintf(stderr, "Failed to read n\n");
		exit(EXIT_FAILURE);
	}
	
	a = newmat(n);
	b = newmat(n);
	c = newmat(n);
	
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			if (scanf("%d", &a[i][j]) != 1) {
				fprintf(stderr, "Failed to read a[%d][%d]\n", i, j);
				exit(EXIT_FAILURE);
			}
	
	matcopy(a, 0, 0, n, b);
	matmul(a, b, c, n);
	
	for (i = 0; i < n; i++) {
		for (j = 0; j < n; j++)
			printf("%d ", c[i][j]);
		printf("\n");
	}
	
	delmat(a, n);
	delmat(b, n);
	delmat(c, n);
	
	if (argc == 1) {
		fclose(stdin);
		fclose(stdout);
	}
	
	return 0;
}

