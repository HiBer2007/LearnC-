// CPP讲解用.cpp: 定义应用程序的入口点。
//

#include "CPP讲解用.h"

using namespace std;

int main()
{
	int a[10] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9 };
	int b = a[6];
	b = *(a + 6);
	return 0;
}
