#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <vector>
#include <sstream>
//#include <omp.h>
#ifdef LINUX
#include <sys/time.h>
#else
#include <time.h>
#endif

using namespace std;
typedef vector<int> doc;



#include <string>
#include <map>
#include <stdio.h>
#include <vector>
#include <unordered_map>
using namespace std;

/*
1. ����embedding���ִʵ���һ���ļ�����ŵ�hash���С�
2. ����ѵ���������Լ����Զ��ֳ���֤���������������������ݽṹ�С�
ѵ���������Լ�����֤�� vec<doc>
doc=vec<word>

*/

#define MAX_STRING 10000


map<string, int> dict; //�ֵ������е��� str->id
vector<string> vocab; //���еĴʡ��֣�����˳�������� id->str

struct embedding_t {
	int size; //����������ٸ�������value ����ı��������� size = element_size * element_num
	int element_size; //һ�������ĳ���
	int element_num; //�����ĸ���
	double *value; //���еĲ���

	void init(int element_size, int element_num) {
		this->element_size = element_size;
		this->element_num = element_num;
		size = element_size * element_num;
		value = new double[size];
	}
};

void ReadWord(char *word, FILE *fin) {
	int a = 0, ch;

	while (!feof(fin)) {
		ch = fgetc(fin);

		if (ch == 13) continue;

		if ((ch == ' ') || (ch == '\t') || (ch == '\n')) {
			if (a > 0) {
				if (ch == '\n') ungetc(ch, fin);
				break;
			}

			if (ch == '\n') {
				strcpy(word, (char *)"</s>");
				return;
			} else continue;
		}

		word[a] = ch;
		a++;

		if (a >= MAX_STRING) {
			printf("Too long word found!\n");   //truncate too long words
			a--;
		}
	}
	word[a] = 0;
}

int ReadWordIndex(FILE *fin) {
	char word[MAX_STRING];
	ReadWord(word, fin);
	if (feof(fin)) return -1;
	if (strcmp(word, "</s>") == 0)
		return -2;

	map<string, int>::iterator it = dict.find(word);
	if (it == dict.end()) {
		return -1;
	} else {
		return it->second;
	}
}

int ReadEmbedding(embedding_t &words, char *file_name) {
	FILE *f = fopen(file_name, "rb");
	if (f == NULL) {
		printf("Embedding file not found\n");
		return -1;
	}
	int wordNum, size;
	fscanf(f, "%d", &wordNum);
	fscanf(f, "%d", &size);

	words.init(size, wordNum);

	char str[MAX_STRING];
	for (int b = 0; b < wordNum; b++) {
		char ch;
		fscanf(f, "%s%c", str, &ch);
		string s = str;
		dict[s] = vocab.size();
		vocab.push_back(s);
		for (int a = 0; a < size; a++)
			fscanf(f, "%lf", &words.value[a + b * size]);
		/*
		double len = 0;
		for (int a = 0; a < size; a++)
		len += M[a + b * size] * M[a + b * size];
		len = sqrt(len);
		for (int a = 0; a < size; a++)
		M[a + b * size] /= len;
		*/
	}
	fclose(f);
	return 0;
}

/*
��ȡ�ĵ���������ѵ���������Լ���
ÿ��һƪ�ĵ������е�һ�������������Ϣ��������ԭ�ġ�ÿ���ʶ��� ��~�� ����ʽ����ʾ��
*/
void ReadDocs(const char *file_name, vector<doc> &docs, vector<int> &labels, const char *dataset) {
	FILE *fi = fopen(file_name, "rb");
	char label[MAX_STRING];
	int unknown = 0;
	int total = 0;
	while (1) {
		if (feof(fi)) break;
		doc d;
		//��һ���Ǳ�ǩ���ȶ����ǩ
		ReadWord(label, fi);
		if (feof(fi)) break;
		int lb = atoi(label);
		labels.push_back(lb);

		while (1) { //��ʼ��ȫ����Ϣ
			int word = ReadWordIndex(fi);
			if (feof(fi)) break;
			if (word == -1) {
				unknown++; //��Ƶ��
				continue;
			}
			//word_count++;
			if (word == -2) break; //һ�н�����
			d.push_back(word);
		}
		if (d.size() > 0) {
			docs.push_back(d);
			total += d.size();
		}
	}
	printf("%s data: N(docs):%d, words:%d, unknown(ignore):%d\n", dataset, (int)docs.size(), total, unknown);
}

//==========================================================


double nextDouble(double s) {
	return rand() / (RAND_MAX + 1.0) * s - s / 2;
}


const int H = 500; //���ز㣬���Ǵ������Ĵ�С
const int MAX_C = 50; //��������
const int MAX_F = 1000; //��������Ĵ�С
const char *model_name = "model_idf";

//const char *train_file = "train.txt";
const char *train_file = "E:\\ecnn����\\��������\\train.txt";
const char *valid_file = "valid.txt";
const char *test_file = "E:\\ecnn����\\��������\\test.txt";

int class_size; //������
int vector_size; //һ���ʵ�Ԫ��������С = ��������С��Լ50�� + ���������Ĵ�С��Լ10��

//===================== ����Ҫ�Ż��Ĳ��� =====================

embedding_t words; //������

double *A; //��������[������][���ز�] �ڶ����Ȩ��
double biasOutput[H]; //TODO class_size

//===================== ��֪���� =====================

//ѵ����
vector<doc> data; //ѵ�����ݣ�[������][������]
vector<vector<double> > _data; //ѵ�����ݣ�[������][������]

//int N; //ѵ������С
//int uN; //δ֪��
vector<int> b; //Ŀ�����[������] ѵ����

//��֤��
vector<doc> vdata; //�������ݣ�[������][������]
vector<vector<double> > _vdata; //�������ݣ�[������][������]
//int vN; //���Լ���С
//int uvN; //δ֪��
vector<int> vb; //Ŀ�����[������] ���Լ�

//���Լ�
vector<doc> tdata; //�������ݣ�[������][������]
vector<vector<double> > _tdata; //�������ݣ�[������][������]
//int tN; //���Լ���С
//int utN; //δ֪��
vector<int> tb; //Ŀ�����[������] ���Լ�





double time_start;
double lambda = 0;//0.01; //���������Ȩ��
double alpha = 0.01; //ѧϰ����
int iter = 0;

double getTime() {
#ifdef LINUX
	timeval tv;
	gettimeofday(&tv, 0);
	return tv.tv_sec + tv.tv_usec * 1e-6;
#else
	return clock();
#endif
}

double nextDouble() {
	return rand() / (RAND_MAX + 1.0);
}

void writeFile(const char *name, double *A, int size) {
	FILE *fout = fopen(name, "wb");
	fwrite(A, sizeof(double), size, fout);
	fclose(fout);
}


int readFile(const char *name, double *A, int size) {
	FILE *fin = fopen(name, "rb");
	if (!fin)
		return 0;
	int len = (int)fread(A, sizeof(double), size, fin);
	fclose(fin);
	return len;
}


/*
ѵ���������Լ�����֤�� vec<doc>
doc=vec<word>
word=(string)ch~ch~ch

����
embedding ÿ���ʡ��ֶ���һ����ĳ��hash���Ӧ

���� doc�����ط��ࣨ�����м�������������ݶȣ�

1. ��ȡÿ������ ѵ���������Լ�����֤��
2. ÿƪ�ĵ���һ��ƽ��ֵ������vector<double>��
3. ѵ����ʵʱ���ѵ�����
4. ��������embedding��ʼ��Ҳ����Ч��
5. ��һ��bag of words��ʵ�飬��liblinear
*/


//��һ����������������ĵ���һ��embedding��ƽ��ֵ
void ConvertData(vector<doc> &data, vector<vector<double> > &_data) {
	for (size_t i = 0; i < data.size(); i++) {
		doc &d = data[i];
		double v[H] = { 0 };
		double w = 0; //Ȩ��
		for (size_t j = 0; j < d.size(); j++) {
			double weight = 1;
			w += weight;
			for (int k = 0; k < words.element_size; k++) {
				v[k] += weight * words.value[d[j] * words.element_size + k];
			}
		}
		vector<double> vv;
		vv.reserve(words.element_size);
		for (int k = 0; k < words.element_size; k++) {
			vv.push_back(v[k] / w);
		}
		_data.push_back(vv);
	}
}

void WriteData(const char *file, vector<vector<double> > &data, vector<int> &b) {
	//char fname[100];
	//sprintf(fname, "%s_l", file);

	//	test_file = argv[4];
	FILE *fout = fopen(file, "w");
	for (int i = 0; i < data.size(); i++) {
		fprintf(fout, "%d", b[i]);
		for (int j = 0; j < data[i].size(); j++) {
			fprintf(fout, " %d:%.16lf", j + 1, data[i][j]);
		}
		fprintf(fout, "\n");
	}

}

int main(int argc, char **argv) {
	if (argc != 6) {
		printf("Useage: ./avg_embedding embedding train test train_out test_out\n");
		return 0;
	}

	printf("read embedding\n");

	train_file = argv[2];
	test_file = argv[3];

	if (ReadEmbedding(words, argv[1]) != -1) {
		printf("initialized with %s\n", argv[1]);
		/*double sum = 0;
		for (int i = 0; i < words.size; i++) {
			sum += words.value[i] * words.value[i];
		}
		sum = sqrt(sum / words.size * 12);
		for (int i = 0; i < words.size; i++) {
			words.value[i] /= sum;
		}*/

		//for (int i = 0; i < words.size; i++) {
		//	words.value[i] = (nextDouble() - 0.5);
		//}
	} else {
		printf("not initialized\n");
	}

	printf("read data\n");
	ReadDocs(train_file, data, b, "Train");
	ReadDocs(test_file, tdata, tb, "Test");

	ConvertData(data, _data);
	ConvertData(tdata, _tdata);

	WriteData(argv[4], _data, b);
	WriteData(argv[5], _tdata, tb);

	return 0;
}
