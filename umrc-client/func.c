/*

Functions used by main.c

Most of these functions involve string manipulation or other
routines which are better kept separate from the chat specific
routines in main.c

*/

#include "func.h"

/**
 *  Returns a time string (HH:MM) to place in front of all chat messages.
 */
char* getTimestamp() {
    char timeStamp[10] = "";
    time_t rawtime;
    time(&rawtime);
#if defined(WIN32) || defined(_MSC_VER)  
    struct tm timeinfo;
    localtime_s(&timeinfo, &rawtime);
#else
    struct tm *timeinfo; 
    timeinfo = localtime(&rawtime);
#endif
    _snprintf_s(timeStamp, sizeof(timeStamp), -1, "%02d:%02d",
#if defined(WIN32) || defined(_MSC_VER)  
        timeinfo.tm_hour,
        timeinfo.tm_min);
#else
        timeinfo->tm_hour,
        timeinfo->tm_min);
#endif
    return _strdup(timeStamp);
}

/**
 *  Returns a date/time string for CTCP responses, formatted as MM/DD/YY HH:MM.
 */
char* getCtcpDatetime() {
    char dtStr[30] = "";
    time_t rawtime;
    time(&rawtime);
#if defined(WIN32) || defined(_MSC_VER)  
    struct tm timeinfo;
    localtime_s(&timeinfo, &rawtime);
#else
    struct tm* timeinfo;
    timeinfo = localtime(&rawtime);
#endif
    _snprintf_s(dtStr, sizeof(dtStr), -1, "%02d/%02d/%02d %02d:%02d",
#if defined(WIN32) || defined(_MSC_VER)  
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_year - 100, // + 1900,
        timeinfo.tm_hour,
        timeinfo.tm_min);
#else
        timeinfo->tm_mon + 1,
        timeinfo->tm_mday,
        timeinfo->tm_year - 100, // + 1900,
        timeinfo->tm_hour,
        timeinfo->tm_min);
#endif
    return _strdup(dtStr);
}


/**
 *  Returns the length of a string without pipe color codes.
 */
int strLenWithoutPipecodes(char* str) {
    int len = 0;
    for (int i = 0; i < (int)strlen(str); i++) {
        if (str[i] == '|' && i < ((int)strlen(str) - 2)) { // check the next 2 characters for digits
            if (isdigit(str[i + 1]) && isdigit(str[i + 2])) {
                i = i + 2; // skip if it's a pipe code
            }
            else {
                len = len + 1;
            }
        }
        else {
            len = len + 1;
        }
    }
    return len;
}

/**
 * Checks if a string contains another string. Case-insensitive.
 */
bool strContainsStrI(char* str, char* contains) {
    char s[512] = "";
    char c[512] = "";
    strcpy_s(s, sizeof(s), str);
    strcpy_s(c, sizeof(c), contains);
    lstr(c);
    lstr(s);
    return strstr(s, c) != NULL;
}

/**
 * Gets a substring from a string. Result stored in: ss
 */
void getSubStr(char* s, char* ss, int pos, int len) {
    int i = 0;
    s += pos; // Move pointer to starting position
    while (len--) {
        *ss++ = *s++;
    }
    *ss = '\0'; // Null terminate
}

/**
 * Removes characters from a string that aren't digits or letters
 */
void removeNonAlphanumeric(char* str) {
    int readPos = 0, writePos = 0;
    while (str[readPos] != '\0') {
        if (isalnum(str[readPos])) {
            str[writePos++] = str[readPos];
        }
        readPos++;
    }
    str[writePos] = '\0';
}

/**
 * Removes illegal characters/strings from strings
 * to be used as filenames.
 */
void cleanUpFilename(char* str) {
    int readPos = 0, writePos = 0;
    while (str[readPos] != '\0') {
        if (str[readPos] == '<' || // .... Disallowed characters
            str[readPos] == '>' ||
            str[readPos] == ':' ||
            str[readPos] == '"' ||
            str[readPos] == '/' ||
            str[readPos] == '\\' ||
            str[readPos] == '|' ||
            str[readPos] == '?' ||
            str[readPos] == '|' ||
            str[readPos] == '*') {
            str[writePos++] = '_';
        }
        else {
            str[writePos++] = str[readPos];
        }
        readPos++;
    }
    str[writePos] = '\0';

    if (_stricmp(str, "CON") == 0 || // .... Disallowed filenames
        _stricmp(str, "PRN") == 0 ||
        _stricmp(str, "AUX") == 0 ||
        _stricmp(str, "NUL") == 0 ||
        (_strnicmp(str, "COM", 3) == 0 && isdigit(str[3])) ||
        (_strnicmp(str, "LPT", 3) == 0 && isdigit(str[3]))) {
        strcat_s(str, 36, "_"); // append a character to make it allowed
    }
}