#pragma once


#include "../common/common.h"

char* getTimestamp();
char* getCtcpDatetime();
int strLenWithoutPipecodes(char* str);
bool strContainsStrI(char* str, char* contains);
void getSubStr(char* s, char* ss, int pos, int len);
void removeNonAlphanumeric(char* str);
void cleanUpFilename(char* str);