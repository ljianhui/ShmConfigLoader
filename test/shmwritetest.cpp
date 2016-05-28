#include <stdio.h>
#include "shmconfigloader.h"

using namespace std;

int main()
{
	ShmConfigLoader configLoader(123456, MODE_WRITE);
	int ret = configLoader.LoadConfig("./test.ini");
	if (ret != 0)
	{
		printf("Load config failed, ret = %d, errmsg[%s]\n", ret, configLoader.GetErrMsg().c_str());
		return 0;
	}

	configLoader.PrintConfig();

	string value = configLoader.GetValue("website", "baidu");
	printf("websize.baidu=%s\n", value.c_str());
	return 0;

}