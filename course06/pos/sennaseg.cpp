#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <sstream>
#include <omp.h>
#ifdef LINUX
#include <sys/time.h>
#else
#include <time.h>
#endif

using namespace std;

const int H = 100; //���ز�
const int MAX_C = 50; //��������
const int MAX_F = 1000; //��������Ĵ�С
const char *model_name = "model_300_nosuff_noinit";

const char *train_file = "pos_train.txt";
const char *valid_file = "pos_valid.txt";
const char *test_file = "pos_test.txt";
//const char *dict_file = "dict.txt";

int class_size; //������
int input_size; //��������������С input_size = window_size*vector_size
int window_size; //���ڴ�С
int vector_size; //һ���ʵ�Ԫ��������С = ��������С��Լ50�� + ���������Ĵ�С��Լ10��

//===================== ����Ҫ�Ż��Ĳ��� =====================
struct embedding_t{
	int size; //����������ٸ�������value ����ı��������� size = element_size * element_num
	int element_size; //һ�������ĳ���
	int element_num; //�����ĸ���
	double *value; //���еĲ���

	void init(int element_size, int element_num){
		this->element_size = element_size;
		this->element_num = element_num;
		size = element_size * element_num;
		value = new double[size];
	}
};

embedding_t words; //������

double *A; //��������[������][���ز�] �ڶ����Ȩ��
double *B; //��������[���ز�][������] ��һ���Ȩ��
double *gA, *gB;

//===================== ��֪���� =====================
struct data_t{
	int word; //�ʵı��
	//char *ch; //ʵ�ʵĴʣ��������
};
//ѵ����
data_t *data; //ѵ�����ݣ�[������][������]
int N; //ѵ������С
int uN; //δ֪��
int *b; //Ŀ�����[������] ѵ����

//��֤��
data_t *vdata; //�������ݣ�[������][������]
int vN; //���Լ���С
int uvN; //δ֪��
int *vb; //Ŀ�����[������] ���Լ�

//���Լ�
data_t *tdata; //�������ݣ�[������][������]
int tN; //���Լ���С
int utN; //δ֪��
int *tb; //Ŀ�����[������] ���Լ�


#include "fileutil.hpp"


double time_start;
double lambda = 0;//0.01; //���������Ȩ��
double alpha = 0.01; //ѧϰ����
int iter = 0;

const int thread_num = 4;
const int patch_size = thread_num;

double getTime(){
#ifdef LINUX
	timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec * 1e-6;
#else
	return clock();
#endif
}

double nextDouble(){
	return rand() / (RAND_MAX + 1.0);
}

void softmax(double hoSums[], double result[], int n){
	double max = hoSums[0];
	for (int i = 0; i < n; ++i)
		if (hoSums[i] > max) max = hoSums[i];
	double scale = 0.0;
	for (int i = 0; i < n; ++i)
		scale += exp(hoSums[i] - max);
	for (int i = 0; i < n; ++i)
		result[i] = exp(hoSums[i] - max) / scale;
}

double sigmoid(double x){
	return 1 / (1 + exp(-x));
}

double hardtanh(double x){
	if(x > 1)
		return 1;
	if(x < -1)
		return -1;
	return x;
}

//b = Ax
void fastmult(double *A, double *x, double *b, int xlen, int blen){
	double val1, val2, val3, val4;
	double val5, val6, val7, val8;
	int i;
	for (i=0; i<blen/8*8; i+=8) {
		val1=0;
		val2=0;
		val3=0;
		val4=0;

		val5=0;
		val6=0;
		val7=0;
		val8=0;

		for (int j=0; j<xlen; j++) {
			val1 += x[j] * A[j+(i+0)*xlen];
			val2 += x[j] * A[j+(i+1)*xlen];
			val3 += x[j] * A[j+(i+2)*xlen];
			val4 += x[j] * A[j+(i+3)*xlen];

			val5 += x[j] * A[j+(i+4)*xlen];
			val6 += x[j] * A[j+(i+5)*xlen];
			val7 += x[j] * A[j+(i+6)*xlen];
			val8 += x[j] * A[j+(i+7)*xlen];
		}
		b[i+0] += val1;
		b[i+1] += val2;
		b[i+2] += val3;
		b[i+3] += val4;

		b[i+4] += val5;
		b[i+5] += val6;
		b[i+6] += val7;
		b[i+7] += val8;
	}

	for (; i<blen; i++) {
		for (int j=0; j<xlen; j++) {
			b[i] += x[j] * A[j+i*xlen];
		}
	}
}

double checkCase(data_t *id, int ans, int &correct, int &output, double *p=NULL, bool gd = false){
	double x[MAX_F];
	for(int i = 0, j = 0; i < window_size; i++){
		int offset = id[i].word * words.element_size;
		for(int k = 0; k < words.element_size; k++,j++){
			x[j] = words.value[offset + k];
		}
	}

	double h[H] = {0};
	fastmult(B, x, h, input_size, H);
	for(int i = 0; i < H; i++){
		//h[i] = sigmoid(h[i]);
		h[i] = tanh(h[i]);
		//h[i] = hardtanh(h[i]+biasH[i]);
		//if(h[i] > 1) h[i] = 1;
		//if(h[i] < -1) h[i] = -1;
	}
	//for(int i = 0, k=0; i < H; i++){
	//	for(int j = 0; j < input_size; j++,k++){
	//		//h[i] += x[j] * B[i*input_size+j];
	//		h[i] += x[j] * B[k];
	//	}
	//	h[i] = sigmoid(h[i]);
	//}

	double r[MAX_C] = {0};
	for(int i = 0; i < class_size; i++){
		//r[i] = biasOutput[i];
		for(int j = 0; j < H; j++){
			r[i] += h[j] * A[i*H+j];
		}
	}
	double y[MAX_C];
	softmax(r, y, class_size);

	if(gd){ //�޸Ĳ���
		/*for(int i = 0; i < class_size; i++){
			if(i == ans){
				biasOutput[i] += alpha*(1-y[i]);
			}else{
				biasOutput[i] += alpha*(0-y[i]);
			}
		}*/

		double dh[H] = {0};
		for(int j = 0; j < H; j++){
			dh[j] = A[ans*H+j];
			for(int i = 0; i < class_size; i++){
				dh[j] -= y[i]*A[i*H+j];
			}
			//dh[j] *= h[j]*(1-h[j]);
			dh[j] *= 1-h[j]*h[j];
			/*if(h[j] > 1 || h[j] < -1)
				dh[j] = 0;
			biasH[j] += alpha * dh[j];*/
		}

		//#pragma omp critical
		{
			for(int i = 0; i < class_size; i++){
				double v = (i==ans?1:0) - y[i];
				for(int j = 0; j < H; j++){
					int t = i*H+j;
					A[t] += alpha/sqrt(H) * (v * h[j] - lambda * A[t]);
					//gA[i*H+j] += v * h[j];
				}
			}

			double dx[MAX_F] = {0};

			//fastmult(B, dh, dx, input_size, H);
			for(int i = 0; i < H; i++){
				for(int j = 0; j < input_size; j++){
					dx[j] += dh[i] * B[i*input_size+j];
				}
			}

			for(int i = 0; i < H; i++){
				for(int j = 0; j < input_size; j++){
					int t = i*input_size+j;
					B[t] += alpha/sqrt(input_size) * (x[j] * dh[i] - lambda * B[t]);
					//gB[i*input_size+j] += -x[j] * dh[i];
				}
			}


			for(int i = 0, j = 0; i < window_size; i++){
				int offset = id[i].word * words.element_size;
				for(int k = 0; k < words.element_size; k++,j++){
					int t = offset + k;
					words.value[t] += alpha * (dx[j] - lambda * words.value[t]);
				}
			}

		}
	}

	output = 0;
	double maxi = 0;
	bool ok = true;
	for(int i = 0; i < class_size; i++){
		if(i != ans && y[i] >= y[ans])
			ok = false;
		if(y[i] > maxi){
			maxi = y[i];
			output = i;
		}
		if(p)
			p[i] = -log(y[i]);
	}

	if(ok)
		correct++;
	return log(y[ans]); //������Ȼ
}


void writeFile(const char *name, double *A, int size){
	FILE *fout = fopen(name, "wb");
	fwrite(A, sizeof(double), size, fout);
	fclose(fout);
}

double checkSet(const char *dataset, data_t *data, int *b, int N, char *fname = NULL){
	int hw = (window_size-1)/2;
	double ret = 0;
	int wordCorrect = 0; //ֱ�ӵĴ�׼ȷ��

	int ans[2000];
	char *chs[2000];
	int index = 0;

	for(int s = 0; s < N; s++){
		int tc = 0;
		int output;
		ret += checkCase(data+s*window_size, b[s], tc, output);
		wordCorrect += tc;
	}
	printf("%s:%lf(%.2lf%%), ", dataset, -ret/N, 100.*wordCorrect/N);
	return -ret/N;
}

//�����ȷ�ʺ���Ȼ
//����ֵ����Ȼ
double check(){
	double ret = 0;
	//char fname[100];

	double ps = 0;
	int pnum = 0;
	for(int i = 0; i < class_size*H; i++,pnum++){
		ps += A[i]*A[i];
	}
	for(int i = 0; i < H*input_size; i++,pnum++){
		ps += B[i]*B[i];
	}
	for(int i = 0; i < words.size; i++,pnum++){
		ps += words.value[i]*words.value[i];
	}
/*
	sprintf(fname, "%s_A", model_name);
	writeFile(fname, A, class_size*H);
	sprintf(fname, "%s_B", model_name);
	writeFile(fname, B, H*input_size);
	sprintf(fname, "%s_w", model_name);
	writeFile(fname, words.value, words.size);
*/

	printf("para: %lf, ", ps/pnum/2);

	ret = checkSet("train", data, b, N);
	checkSet("valid", vdata, vb, vN);
	//sprintf(fname, "%s_%d_output", model_name, iter);
	checkSet("test", tdata, tb, tN);

	printf("time:%.1lf\n", getTime()-time_start);
	fflush(stdout);

	double fret = ret + ps/pnum*lambda/2;
	return fret;
}

int readFile(const char *name, double *A, int size){
	FILE *fin = fopen(name, "rb");
	if(!fin)
		return 0;
	int len = (int)fread(A, sizeof(double), size, fin);
	fclose(fin);
	return len;
}

int main(int argc, char **argv){
	if(argc < 2){
		printf("Useage: ./senna_tag embedding\n");
		return 0;
	}

	printf("read embedding\n");

	window_size = 5;
	class_size = 45;


	init(argv[1]);

	vector_size = words.element_size;

	//words.init(vector_size, chk.size());

	printf("read data\n");
	readAllData(train_file, "Train", window_size, data, b, N, uN);
	readAllData(valid_file, "Valid", window_size, vdata, vb, vN, uvN);
	readAllData(test_file, "Test", window_size, tdata, tb, tN, utN);

	input_size = window_size * vector_size;

	printf("init. input(features):%d, hidden:%d, output(classes):%d, alpha:%lf, lambda:%.16lf\n", input_size, H, class_size, alpha, lambda);
	printf("window_size:%d, vector_size:%d, vocab_size:%d, lineMax:%d\n", window_size, vector_size, words.element_num, lineMax);

	A = new double[class_size*H];
	gA = new double[class_size*H];
	B = new double[H*input_size];
	gB = new double[H*input_size];

	for(int i = 0; i < class_size * H; i++){
		A[i] = (nextDouble()-0.5) / sqrt(H);
	}
	for(int i = 0; i < H * input_size; i++){
		B[i] = (nextDouble()-0.5) /sqrt(input_size);
	}
	for(int i = 0; i < words.size; i++){
	//	words.value[i] = (nextDouble()-0.5);
	}
	
	
	//if(readFile(argv[1], words.value, words.size)){
	//	printf("initialized with %s\n", argv[1]);
	{	double sum = 0;
		for(int i = 0; i < words.size; i++){
			sum += words.value[i]*words.value[i];
		}
		sum = sqrt(sum/words.size*12);
		for(int i = 0; i < words.size; i++){
			words.value[i] /= sum;
		}
		/*if(argc > 3){
			double v = atof(argv[3]);
			printf("x%lf %s\n", v);
			for(int i = 0; i < words.size; i++){
				words.value[i] *= v;
			}
		}*/
	}/*else{
		printf("not initialized\n");
	}*/

	for(int i = 0; i < words.element_num; i++){
		for(int j = 0; j < words.element_size; j++){
			//words.value[i * words.element_size + j] = senna_raw_words[i].vec[j] / sqrt(12);
		}
	}
	

	time_start = getTime();

	int *order = new int[N];
	for(int i = 0; i < N; i++){
		order[i] = i;
	}

	//double lastLH = 1e100;
	while(iter < 30){
		//������ȷ��
		printf("%citer: %d, ", 13, iter);
		//double LH = check();
		check();
		iter++;
		/*if(LH > lastLH){
			alpha = 0.0001;
		}
		lastLH = LH;*/


		double lastTime = getTime();
		//memset(gA, 0, sizeof(double)*class_size*H);
		//memset(gB, 0, sizeof(double)*H*input_size);

		for(int i = 0; i < N; i++){
			swap(order[i], order[rand()%N]);
		}
		double tlambda = lambda;
		for(int i = 0; i < N; i++){
			lambda = 0;
			if(i % 10 == 0)
				lambda = tlambda;
			int s = order[i];
			data_t *x = data+s*window_size;
			int ans = b[s];

			int tmp, output;
			checkCase(x, ans, tmp, output, NULL, true);

			if ((i%1000)==0){
				//printf("%cIter: %3d\t   Progress: %.2f%%   Words/sec: %.1f ", 13, iter, 100.*i/N, i/(getTime()-lastTime));
			}
		}
		lambda = tlambda;
	}
	return 0;
}