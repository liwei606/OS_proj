/********************************************
* Problem3 description:
* This is a multi-thread program for matrix
* multiplication by the partition based algorithm
* with time complexity O(n3)
*********************************************/

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>

int NThread;
int* thread_used;
pthread_t* threads;

typedef int** mat;

typedef void (*matfunc)(mat, mat, mat, int);

typedef struct {
	matfunc f;
	mat a, b, c;
	int n;
} mcnf;

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

mcnf* matconf(matfunc f, mat a, mat b, mat c, int n) {
	mcnf *cf;
	cf = malloc(sizeof(mcnf));
	cf->f = f;
	cf->a = a;
	cf->b = b;
	cf->c = c;
	cf->n = n;
	return cf;
}

void* matcaller(void* data) {
	mcnf* c = data;
	(*(c->f))(c->a, c->b, c->c, c->n);
	pthread_exit(NULL);
}

void thread_join(int tid) {
	pthread_join(threads[tid], NULL);
}

void new_thread(int tid, mcnf* c) {
	pthread_create(&threads[tid], NULL, matcaller, c);
}

void multithreading(mcnf* conf[], int nconf) {
	int i, tid;
	for (i = 0; i < NThread; i++) thread_used[i] = 0;
	for (i = 0; i < nconf; i++) {
		tid = i % NThread;
		if (thread_used[tid])
			thread_join(tid);
		new_thread(tid, conf[i]);
		thread_used[tid] = 1;
	}
	for (i = 0; i < NThread; i++)
		if (thread_used[i])
			thread_join(i);
}

void matadd(mat a, mat b, mat c, int n) {
	int i, j;
	for (i = 0; i < n; i++)
		for (j = 0; j < n; j++)
			c[i][j] = a[i][j] + b[i][j];
}

void matmul(mat a, mat b, mat c, int n) {
	mcnf* conf[8];
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
	
	conf[0] = matconf(matmul_naive, a11, b11, a11b11, k);
	conf[1] = matconf(matmul_naive, a12, b21, a12b21, k);
	conf[2] = matconf(matmul_naive, a11, b12, a11b12, k);
	conf[3] = matconf(matmul_naive, a12, b22, a12b22, k);
	conf[4] = matconf(matmul_naive, a21, b11, a21b11, k);
	conf[5] = matconf(matmul_naive, a22, b21, a22b21, k);
	conf[6] = matconf(matmul_naive, a21, b12, a21b12, k);
	conf[7] = matconf(matmul_naive, a22, b22, a22b22, k);
	multithreading(conf, 8);
	
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
	
	conf[0] = matconf(matadd, a11b11, a12b21, c11, k);
	conf[1] = matconf(matadd, a11b12, a12b22, c12, k);
	conf[2] = matconf(matadd, a21b11, a22b21, c21, k);
	conf[3] = matconf(matadd, a21b12, a22b22, c22, k);
	multithreading(conf, 4);
	
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
		if (scanf("%d", &n) != 1) {
			fprintf(stderr, "Failed to read n\n");
			exit(EXIT_FAILURE);
		}
		if (freopen("data.out", "w", stdout) == 0) {
			fprintf(stderr, "Failed to open data.out\n");
			exit(EXIT_FAILURE);
		}
		NThread = 4;
	} else if (argc == 3 || argc == 4) {
		n = atoi(argv[1]);
		if (freopen(argv[2], "w", stdout) == 0) {
			fprintf(stderr, "Failed to open %s\n", argv[2]);
			exit(EXIT_FAILURE);
		}
		if (argc == 4) {
			NThread = atoi(argv[3]);
		} else {
			NThread = 4;
		}
	} else {
		fprintf(stderr, "Usage: %s [<Size> <OutputFile> [<NThread>]]\n", argv[0]);
		exit(EXIT_FAILURE);
	}
	
	thread_used = malloc(NThread * sizeof(int));
	threads = malloc(NThread * sizeof(pthread_t));
	
	a = newmat(n);
	b = newmat(n);
	c = newmat(n);
	
	if (argc == 1) {
		for (i = 0; i < n; i++)
			for (j = 0; j < n; j++)
				if (scanf("%d", &a[i][j]) != 1) {
					fprintf(stderr, "Failed to read a[%d][%d]\n", i, j);
					exit(EXIT_FAILURE);
				}
	} else {
		for (i = 0; i < n; i++)
			for (j = 0; j < n; j++)
				a[i][j] = rand() % 10;
	}
	
	matcopy(a, 0, 0, n, b);
	matmul(a, b, c, n);
	
	if (argc == 1) {
		for (i = 0; i < n; i++) {
			for (j = 0; j < n; j++)
				printf("%d ", c[i][j]);
			printf("\n");
		}
	} else {
		printf("%d\n", n);
		for (i = 0; i < n; i++)
			for (j = 0; j < n; j++)
				printf("%d %d %d\n", i, j, a[i][j]);
		for (i = 0; i < n; i++)
			for (j = 0; j < n; j++)
				printf("%d %d %d\n", i, j, b[i][j]);
		for (i = 0; i < n; i++)
			for (j = 0; j < n; j++)
				printf("%d %d %d\n", i, j, c[i][j]);
	}
	
	delmat(a, n);
	delmat(b, n);
	delmat(c, n);
	
	fclose(stdin);
	fclose(stdout);
	
	return 0;
}

