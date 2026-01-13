/*
 * Common functions, data structures, and directives used by all uMRC programs.
 * 
 * 
 */


#if defined(WIN32) || defined(_MSC_VER)

#define PLATFORM "Windows"
#include <windows.h>
#include <conio.h>
HANDLE hCon; // needed for setting console colors and clearing screen

#else

#if defined(__APPLE__)
#define PLATFORM "macOS"
#else
#define PLATFORM "Linux"
#endif

#include <unistd.h>
#include <termios.h>
#include <sys/types.h>
#include <errno.h>

#define strcpy_s(dest, count, source)                         strncpy( (dest), (source), (count) )
#define strncpy_s(dest, max, source, count)                   strncpy( (dest), (source), (max) )
#define strcat_s(dest, max, source)                           strcat( (dest), (source))
#define _snprintf_s(buffer, sizeofBuffer, count, format, ...) snprintf((buffer), (sizeofBuffer), (format), ##__VA_ARGS__)
#define _strdup(s)                                            strdup((s))
#define _stricmp(s1, s2)                                      strcasecmp((s1), (s2))
#define _strnicmp(s1, s2, n)                                  strncasecmp((s1), (s2), (n))             
#define strtok_s(s, d, context)                               strtok_r((s), (d), (context))

#define INVALID_SOCKET   ~0
#define SOCKET_ERROR     -1

typedef int       SOCKET;    
typedef void*     HANDLE;      

static struct termios old,current;
void initTermios(int echo);
void resetTermios(void);
char _getch();
void Sleep(int ms);

#endif

#ifndef COMMON_H_   /* Include guard */
#define COMMON_H_

#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>


#define TITLE "uMRC"

// The Protocol Version should only change 
// if it changes at the MRC Server.
// see: https://bbswiki.bottomlessabyss.net/index.php?title=MRCDoc:MRC_Protocol
#define PROTOCOL_VERSION "1.3"

// When building a new executable, update the 
// following, especially the author info and
// compile date. The uMRC Version should be
// numeric and incremented when any significant 
// modification is made to the code, keeping to 
// 3 digits (e.g.: "101", "102", "103", etc...).
#define UMRC_VERSION "100"
#define YEAR_AND_AUTHOR "2025 Craig Hendricks (aka Codefenix)"
#define AUTHOR_INITIALS "cf" // alias initials
#define COMPILE_DATE "2026-01-13"

// These defaults should remain the same, and
// not be changed without a good reason.
#define CONFIG_FILE "mrc.cfg"
#define MRC_STATS_FILE "mrcstats.dat"
#define LOG_FILE "umrc.log"
#define USER_DATA_DIR "userdata"
#define DEFAULT_HOST "mrc.bottomlessabyss.net"
#define DEFAULT_PORT "5000"
#define DEFAULT_SSL_PORT "5001"
#define MSG_LEN 141
#define PACKET_LEN 513
#define DATA_LEN 12228
#define MAX_CLIENTS 50



#pragma region ARC_TYPES
#if defined(__x86_64__) || defined(_M_X64)
#define ARC "x86_64"
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#define ARC "x86"
#elif defined(__ARM_ARCH_2__)
#define ARC "ARM2"
#elif defined(__ARM_ARCH_3__) || defined(__ARM_ARCH_3M__)
#define ARC "ARM3"
#elif defined(__ARM_ARCH_4T__) || defined(__TARGET_ARM_4T)
#define ARC "ARM4T"
#elif defined(__ARM_ARCH_5_) || defined(__ARM_ARCH_5E_)
#define ARC "ARM5"
#elif defined(__ARM_ARCH_6T2_) || defined(__ARM_ARCH_6T2_)
#define ARC "ARM6T2"
#elif defined(__ARM_ARCH_6__) || defined(__ARM_ARCH_6J__) || defined(__ARM_ARCH_6K__) || defined(__ARM_ARCH_6Z__) || defined(__ARM_ARCH_6ZK__)
#define ARC  "ARM6"
#elif defined(__ARM_ARCH_7__) || defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
#define ARC  "ARM7"
#elif defined(__ARM_ARCH_7A__) || defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
#define ARC  "ARM7A"
#elif defined(__ARM_ARCH_7R__) || defined(__ARM_ARCH_7M__) || defined(__ARM_ARCH_7S__)
#define ARC  "ARM7R"
#elif defined(__ARM_ARCH_7M__)
#define ARC  "ARM7M"
#elif defined(__ARM_ARCH_7S__)
#define ARC  "ARM7S"
#elif defined(__aarch64__) || defined(_M_ARM64)
#define ARC  "ARM64"
#elif defined(mips) || defined(__mips__) || defined(__mips)
#define ARC  "MIPS"
#elif defined(__sh__)
#define ARC  "SUPERH";
#elif defined(__powerpc) || defined(__powerpc__) || defined(__powerpc64__) || defined(__POWERPC__) || defined(__ppc__) || defined(__PPC__) || defined(_ARCH_PPC)
#define ARC  "POWERPC"
#elif defined(__PPC64__) || defined(__ppc64__) || defined(_ARCH_PPC64)
#define ARC  "POWERPC64"
#elif defined(__sparc__) || defined(__sparc)
#define ARC  "SPARC"
#elif defined(__m68k__)
#define ARC  "M68K"
#else
#define ARC  "UNKNOWN"
#endif
#pragma endregion

#pragma region ANSI_CODES
#define CL "\x1b[2J\x1b[H\x1b[0m"
#define BK "\x1b[2;30m"
#define RE "\x1b[2;31m"
#define GR "\x1b[2;32m"
#define BR "\x1b[2;33m"
#define BL "\x1b[2;34m"
#define MA "\x1b[2;35m"
#define CY "\x1b[2;36m"
#define GY "\x1b[2;37m"
#define DG "\x1b[30;1m"
#define LR "\x1b[31;1m"
#define LG "\x1b[32;1m"
#define YE "\x1b[33;1m"
#define LB "\x1b[34;1m"
#define LM "\x1b[35;1m"
#define LC "\x1b[36;1m"
#define WH "\x1b[37;1m"
#define BBK "\x1b[40m"
#define BRE "\x1b[41m"
#define BGR "\x1b[42m"
#define BBR "\x1b[43m"
#define BBL "\x1b[44m"
#define BMA "\x1b[45m"
#define BCY "\x1b[46m"
#define BGY "\x1b[47m"
#pragma endregion


struct settings {
	char host[80];
	char port[6];
	bool ssl;
	char name[140];
	char soft[140];
	char web[140];
	char tel[140];
	char ssh[140];
	char sys[140];
	char dsc[140];
};

struct userdata {
    bool chatSounds;
    int userNumber;
    int chatterNameFgColor;
    int chatterNameBgColor;
    int textColor;
    char chatterName[36];
    char chatterNamePrefixFgColor;
    char chatterNamePrefixBgColor;
    char chatterNamePrefix;
    char chatterNameSuffix[33];
    char defaultRoom[30];
    char joinMessage[50];
    char exitMessage[50];
    char theme[20];
};

char* strReplace(char* orig, char* rep, char* with);
void ustr(char* s);
void lstr(char* s);
int indexOfChar(char* s, char c);
int split(const char* txt, char delim, char*** tokens);
void processPacket(char* packet, char** fromUser, char** fromSite, char** fromRoom, char** toUser, char** msgExt, char** toRoom, char** body);
void parseStats(char* stats, char** bbses, char** rooms, char** users, char** activity);
char* createPacket(char* fromUser, char* fromSite, char* fromRoom, char* toUser, char* msgExt, char* toRoom, char* body);
char* pipeToAnsi(char* str);
int countOfChars(char* str, char c);
int loadData(struct settings* data, char* filename);
size_t saveData(struct settings* data, char* filename);
int loadUser(struct userdata* data, char* filename);
size_t saveUser(struct userdata* data, char* filename);
void printPipeCodeString(char* str);
void clearScreen();


#endif
