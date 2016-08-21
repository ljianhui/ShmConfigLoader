#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <string>
#include "shmconfigloader.h"

#define IS_SPACE_CHAR(c) ((c) == ' '||(c) == '\n'||(c) == '\t'||(c) == '\r')

using std::string;

typedef size_t offset_t;

typedef struct ConfigKeyValue
{
	offset_t key;	// key的文本的地址
	offset_t value;	// value的文本的地址
}ConfigKeyValue;

typedef struct ConfigSection
{
	offset_t name; 	// []的文本的地址
	offset_t kvs;	// Key-values 数组偏移地址
	size_t kvCount;	// kv的数量, ConfigKeyValue的元素个数
}ConfigSection;

struct Config
{
	size_t shmBytes;	// 共享内存的总字节数
	size_t sectionCount;// section的个数
	ConfigSection sections[0];
};

static int ShmDataCompare(const ConfigKeyValue &x, const ConfigKeyValue &y, const void *baseAddr)
{
	return strcmp((const char*)baseAddr + x.key, (const char*)baseAddr + y.key);
}

static int ShmDataCompare(const ConfigKeyValue &x, const char *y, const void *baseAddr)
{
	return strcmp((const char*)baseAddr + x.key, y);
}

static int ShmDataCompare(const ConfigSection &x, const ConfigSection &y, const void *baseAddr)
{
	return strcmp((const char*)baseAddr + x.name, (const char*)baseAddr + y.name);
}

static int ShmDataCompare(const ConfigSection &x, const char *y, const void *baseAddr)
{
	return strcmp((const char*)baseAddr + x.name, y);
}

ShmConfigLoader::ShmConfigLoader(key_t shmKey, unsigned int mode):
	m_shmId(-1), m_mode(mode), m_shmKey(shmKey), m_configPtr(NULL)
{
}

ShmConfigLoader::~ShmConfigLoader()
{
	DetShm();
}

bool ShmConfigLoader::PreHandleInput(char *line)
{
	TrimString(line);
	// 注释行或空行，无需要进一步处理
	if (line[0] == '\0' || line[0] == '#' || strncmp(line, "//", 2) == 0) 
	{
		return false;
	}

	// 行内注释，删除注释
	char *comment = strstr(line, "//");
	if (comment != NULL)
	{
		*comment = '\0';
		TrimString(line);
	}

	comment = strstr(line, "#");
	if (comment != NULL)
	{
		*comment = '\0';
		TrimString(line);
	}

	return true;
}

int ShmConfigLoader::AnalyseConfig(const char *conf, 
	size_t &sectionCount, size_t &kvCount, size_t &bytes)
{
	FILE *fconf = fopen(conf, "r");
	if (fconf == NULL)
	{
		m_errMsg = string("open config file[") + conf + string("] failed");
		return ERR_OPEN_CONFIG;
	}

	sectionCount = 0;
	kvCount = 0;
	bytes = 0;
	char buffer[1024] = {0};
	while (fgets(buffer, sizeof(buffer) - 1, fconf))
	{
		if (PreHandleInput(buffer) == false) 
		{
			continue;
		}

		char *equal = strstr(buffer, "=");
		char *leftBracket = strstr(buffer, "[");
		char *rightBracket = strstr(buffer, "]");
		if (leftBracket == buffer && rightBracket != NULL && equal == NULL)
		{
			++sectionCount;
			// [xxx] => xxx\0
			bytes += (strlen(buffer) - 1);
		}
		else if (sectionCount > 0 && leftBracket == NULL && rightBracket == NULL && equal != NULL)
		{
			++kvCount;
			// xxx=yyy => xxx\0yyy\0
			bytes += (strlen(buffer) + 1);
		}
		else
		{
			fclose(fconf);
			m_errMsg = string("config file[") + conf + string("] format error");
			return ERR_CONFIG_FORMAT;
		}
	}

	fclose(fconf);
	return 0;
}

void ShmConfigLoader::TrimString(char *str)
{
	char *pc = NULL;
	for (pc = str + strlen(str) - 1; pc >= str && IS_SPACE_CHAR(*pc); --pc)
	{
		*pc = '\0';
	}

	for (pc = str; *pc != '\0' && IS_SPACE_CHAR(*pc); ++pc);
	if (pc != str)
	{
		char *dst = NULL;
		for (dst = str; *pc; ++pc, ++dst)
		{
			*dst = *pc;
		}
		*dst = '\0';
	}
}

int ShmConfigLoader::LoadConfig(const char *conf)
{
	if (m_mode == MODE_READ)
	{
		return AttachConfigShm();
	}
	else if (m_mode == MODE_WRITE)
	{
		return CreateConfigShm(conf);
	}
	m_errMsg = "the mode to load config file is invalid";
	return ERR_INVALID_MODE;
}

int ShmConfigLoader::AttachConfigShm()
{
	int ret = GetShm(sizeof(Config));
	if (ret != 0)
	{
		return ret;
	}

	size_t shmBytes = m_configPtr->shmBytes;
	ret = DetShm();
	if (ret != 0)
	{
		return ret;
	}

	if (shmBytes == 0)
	{
		m_errMsg = "shm is loding";
		return ERR_SHM_LODING;
	}

	return GetShm(shmBytes);
}

int ShmConfigLoader::CreateConfigShm(const char *conf)
{
	if (conf == NULL)
	{
		m_errMsg = "config file is NULL";
		return ERR_OPEN_CONFIG;
	}
	
	size_t sectionCount = 0;
	size_t kvCount = 0;
	size_t bytes = 0;
	int ret = AnalyseConfig(conf, sectionCount, kvCount, bytes);
	if (ret != 0)
	{
		return ret;
	}

	size_t shmBytes = 
		sizeof(Config) + 
		sizeof(ConfigSection) * sectionCount + 
		sizeof(ConfigKeyValue) * kvCount + 
		bytes;
	ret = GetShm(shmBytes);
	if (ret != 0)
	{
		return ret;
	}

	m_configPtr->shmBytes = 0;
	m_configPtr->sectionCount = sectionCount;
	ret = LoadToShm(conf, sectionCount, kvCount);
	if (ret != 0)
	{
		return ret;
	}

	SortConfig();

	m_configPtr->shmBytes = shmBytes;
	return 0;
}

int ShmConfigLoader::LoadToShm(const char *conf, size_t sectionCount, size_t kvCount)
{
	FILE *fconf = fopen(conf, "r");
	if (fconf == NULL)
	{
		m_errMsg = string("open config file[") + conf + string("] failed");
		return ERR_OPEN_CONFIG;
	}

	char *shmPtr = (char*)m_configPtr;
	ConfigSection *section = m_configPtr->sections;
	ConfigKeyValue *keyValue = (ConfigKeyValue*)(shmPtr + 
		sizeof(Config) + 
		sizeof(ConfigSection) * sectionCount);
	char *context = shmPtr +
		sizeof(Config) +
		sizeof(ConfigSection) * sectionCount + 
		sizeof(ConfigKeyValue) * kvCount;

	char buffer[1024] = {0};
	char *pc = '\0';
	unsigned len = 0;
	bool isFirstSection = true;
	while (fgets(buffer, sizeof(buffer)-1, fconf))
	{
		if (PreHandleInput(buffer) == false)
		{
			continue;
		}

		if (buffer[0] == '[')
		{
			pc = strstr(buffer + 1, "]");
			if (pc == NULL)
			{
				continue;
			}

			if (!isFirstSection)
			{
				++section;
			}
			section->kvs = (char*)keyValue - shmPtr;
			section->kvCount = 0;

			*pc = '\0';
			TrimString(buffer+1);
			len = strlen(buffer+1);
			memcpy(context, buffer + 1, len + 1);
			section->name = context - shmPtr;
			context += (len + 1);
			isFirstSection = false;
		}
		else
		{
			pc = strstr(buffer, "=");
			if (pc == NULL)
			{
				continue;
			}

			*pc = '\0';
			char *strKey = buffer;
			char *strValue = pc + 1;
			TrimString(strKey);
			TrimString(strValue);

			len = strlen(strKey);
			memcpy(context, strKey, len + 1);
			keyValue->key = context - shmPtr;
			context += (len + 1);

			len = strlen(strValue);
			memcpy(context, strValue, len + 1);
			keyValue->value = context - shmPtr;
			context += (len + 1);

			++section->kvCount;
			++keyValue;
		}
	}
	fclose(fconf);
	return 0;
}

void ShmConfigLoader::SortConfig()
{
	if (m_configPtr == NULL)
	{
		return;
	}

	srand(time(NULL));

	QuickSort(m_configPtr->sections, 0, (int)(m_configPtr->sectionCount - 1));

	char *shmPtr = (char*)m_configPtr;
	ConfigSection *section = m_configPtr->sections;
	for (size_t i = 0; i < m_configPtr->sectionCount; ++i)
	{
		QuickSort((ConfigKeyValue*)(shmPtr + section->kvs), 0, (int)(section->kvCount - 1));	
		++section;
	}
}

template<typename T>
void ShmConfigLoader::QuickSort(T *elems, int begin, int end)
{
	if (elems == NULL || begin >= end)
	{
		return;
	}

	//随机选取一个元素作为枢纽，并与最后一个元素交换
	int ic = rand()%(end - begin + 1) + begin;
	Swap(elems[ic], elems[end]);

	const void *shmPtr = (const void*)m_configPtr;
	const T &centre = elems[end];
	int i = begin;
	int j = end - 1;
	while(true)
	{
		//从前向后扫描，找到第一个小于枢纽的值，
		//在到达数组末尾前，必定结束循环,因为最后一个值为centre
		while (ShmDataCompare(elems[i], centre, shmPtr) < 0)
			++i;
		//从后向前扫描，此时要检查下标，防止数组越界
		while(j >= begin && ShmDataCompare(elems[j], centre, shmPtr) > 0)
			--j;
		//如果没有完成一趟交换，则交换
		if(i < j)
			Swap(elems[i++], elems[j--]);
		else
			break;
	}
	//把枢纽放在正确的位置
	Swap(elems[i], elems[end]);
	QuickSort(elems, begin, i - 1);
	QuickSort(elems, i + 1, end);
}

template<typename T>
void ShmConfigLoader::Swap(T &x, T &y)
{
	T tmp(x);
	x = y;
	y = tmp;
}

template<typename T>
int ShmConfigLoader::BinSearch(const T *elems, size_t size, const char *key)const
{
	const char *shmPtr = (const char*)m_configPtr;
	int begin = 0;
	int end = (int)size - 1;
	while (begin <= end)
	{
		int middle = begin + (end - begin) / 2;
		int ret = ShmDataCompare(elems[middle], key, shmPtr);
		if (ret == 0)
		{
			return middle;
		}
		else if (ret < 0)
		{
			begin = middle + 1;
		}
		else
		{
			end = middle - 1;
		}

	}
	return -1;
}

int ShmConfigLoader::GetShm(size_t size)
{	
	m_shmId = shmget(m_shmKey, size, m_mode);
	if (m_shmId == -1)
	{
		char errMsg[256] = {0};
		snprintf(errMsg, sizeof(errMsg), 
			"shmget failed, shmKey=%d, size=%u, mode=%o", 
			(int)m_shmKey, (unsigned)size, m_mode);
		m_errMsg = errMsg;
		return ERR_SHMGET;
	}

	void *shmPtr = shmat(m_shmId, NULL, 0);
	if (shmPtr == (void*)-1)
	{
		m_errMsg = "shmat failed";
		return ERR_SHMAT;
	}
	m_configPtr = (Config*)shmPtr;
	return 0;
}

int ShmConfigLoader::DetShm()
{
	if (m_configPtr != NULL)
	{
		if (shmdt((const void*)m_configPtr) == -1)
		{
			m_errMsg = "shmdt failed";
			return ERR_SHMDT;
		}
		m_configPtr = NULL;
	}
	return 0;
}

string ShmConfigLoader::GetValue(const char *sectionName, const char *key)const
{
	if (m_configPtr == NULL || m_shmId == 0 || sectionName == NULL || key == NULL)
	{
		return string();
	}

	char *shmPtr = (char*)m_configPtr;
	int i = BinSearch(m_configPtr->sections, m_configPtr->sectionCount, sectionName);
	if (i >= 0)
	{
		ConfigSection *section = &m_configPtr->sections[i];
		int j = BinSearch((ConfigKeyValue*)(shmPtr + section->kvs), section->kvCount, key);
		if (j >= 0)
		{
			ConfigKeyValue *keyValue = ((ConfigKeyValue*)(shmPtr + section->kvs)) + j;
			return string(shmPtr + keyValue->value);
		}
	}
	return string();
}

void ShmConfigLoader::PrintConfig()const
{
	if (m_configPtr == NULL || m_shmId == 0)
	{
		return;
	}

	char *shmPtr = (char*)m_configPtr;
	ConfigSection *section = m_configPtr->sections;
	for (size_t i = 0; i < m_configPtr->sectionCount; ++i)
	{
		printf("[%s]\n", shmPtr + section->name);
		for (size_t j = 0; j < section->kvCount; ++j)
		{
			ConfigKeyValue *keyValue = ((ConfigKeyValue*)(shmPtr + section->kvs)) + j;
			printf("%s=%s\n", shmPtr + keyValue->key, shmPtr + keyValue->value);
		}
		++section;
	}
}

int ShmConfigLoader::FreeShm()
{
	int ret = DetShm();
	if (ret != 0)
	{
		return ret;
	}

	if (m_shmId != 0)
	{
		if (shmctl(m_shmId, IPC_RMID, 0) == -1)
		{
			m_errMsg = "shmctl remove id failed";
			return ERR_SHMCTL_RMID;
		}
		m_shmId = 0;
	}
	return 0;
}
