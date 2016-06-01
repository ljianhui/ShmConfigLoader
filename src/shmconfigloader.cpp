#include <stdio.h>
#include <string.h>
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
	offset_t name; 	//[]的文本的地址
	size_t kvCount;	// kv的数量, ConfigKeyValue的元素个数
	ConfigKeyValue kv[0];
}ConfigSection;

struct Config
{
	size_t shmBytes;	// 共享内存的总字节数
	size_t sectionCount;// section的个数
	ConfigSection sections[0];
};

ShmConfigLoader::ShmConfigLoader(key_t shmKey, unsigned int mode):
	m_shmId(-1), m_mode(mode), m_shmKey(shmKey), m_configPtr(NULL)
{
}

ShmConfigLoader::~ShmConfigLoader()
{
	DetShm();
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
		TrimString(buffer);
		// 忽略注释
		if (buffer[0] == '#' || strncmp(buffer, "//", 2) == 0 ||
			buffer[0] == '\n' || strcmp(buffer, "\r\n") == 0)
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
		else if (sectionCount > 0 && leftBracket == NULL && leftBracket == NULL && equal != NULL)
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
	size_t i = 0;
	for (i = strlen(str) - 1; i >= 0 && IS_SPACE_CHAR(str[i]); --i)
	{
		str[i] = '\0';
	}

	for (i = 0; str[i] != '\0' && IS_SPACE_CHAR(str[i]); ++i);
	if (i != 0)
	{
		char *pc1 = str;
		for (char *pc2 = str + i; *pc2; ++pc1, ++pc2)
		{
			*pc1 = *pc2;
		}
		*pc1 = '\0';
	}
}

ConfigSection* ShmConfigLoader::IncConfigSection(ConfigSection *section)const
{
	return (ConfigSection*)((char*)section + sizeof(ConfigSection) + sizeof(ConfigKeyValue) * section->kvCount);
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
	if (shmBytes == 0)
	{
		m_errMsg = "shm is loding";
		return ERR_SHM_LODING;
	}

	ret = DetShm();
	if (ret != 0)
	{
		return ret;
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
	m_configPtr->sectionCount = 0;
	ret = LoadToShm(conf, sectionCount, kvCount);
	if (ret != 0)
	{
		return ret;
	}

	m_configPtr->shmBytes = shmBytes;
	m_configPtr->sectionCount = sectionCount;
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

	ConfigSection *section = m_configPtr->sections;
	char *context = (char*)m_configPtr + 
		sizeof(Config) +
		sizeof(ConfigSection) * sectionCount + 
		sizeof(ConfigKeyValue) * kvCount;

	char buffer[1024] = {0};
	char *pc = '\0';
	unsigned len = 0;
	bool isFirstsection = true;
	while (fgets(buffer, sizeof(buffer)-1, fconf))
	{
		TrimString(buffer);
		if (buffer[0] == '#' || strncmp(buffer, "//", 2) == 0 ||
			buffer[0] == '\n' || strcmp(buffer, "\r\n") == 0)
		{
			continue;
		}

		if (buffer[0] == '[')
		{
			if (!isFirstsection)
			{
				section = IncConfigSection(section);
			}
			section->kvCount = 0;
			pc = strstr(buffer + 1, "]");
			if (pc == NULL)
			{
				continue;
			}

			*pc = '\0';
			TrimString(buffer+1);
			len = strlen(buffer+1);
			memcpy(context, buffer + 1, len + 1);
			section->name = context - (char*)m_configPtr;
			context += (len + 1);
			isFirstsection = false;
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
			section->kv[section->kvCount].key = context - (char*)m_configPtr;
			context += (len + 1);

			len = strlen(strValue);
			memcpy(context, strValue, len + 1);
			section->kv[section->kvCount].value = context - (char*)m_configPtr;
			context += (len + 1);

			++section->kvCount;
		}
	}
	return 0;
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
		return NULL;
	}

	char *shmPtr = (char*)m_configPtr;
	ConfigSection *section = m_configPtr->sections;
	for (size_t i = 0; i < m_configPtr->sectionCount; ++i)
	{
		if (strcmp(shmPtr + section->name, sectionName) == 0)
		{
			for (size_t j = 0; j < section->kvCount; ++j)
			{
				if (strcmp(shmPtr + section->kv[j].key, key) == 0)
				{
					return string(shmPtr + section->kv[j].value);
				}
			}
		}
		section = IncConfigSection(section);
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
			printf("%s=%s\n", shmPtr + section->kv[j].key, shmPtr + section->kv[j].value);
		}
		section = IncConfigSection(section);
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
