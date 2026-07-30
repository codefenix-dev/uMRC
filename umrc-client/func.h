#pragma once


#include "../common/common.h"

char* getTimestamp();
char* getCtcpDatetime();
int strLenWithoutPipecodes(char* str);
char* stristr(const char* haystack, const char* needle);
void getSubStr(char* s, char* ss, int pos, int len);
void removeNonAlphanumeric(char* str);
void cleanUpFilename(char* str);