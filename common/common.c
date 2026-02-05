/*
 * Common function definitions used by all uMRC programs.
 * 
 * 
 */


#include "common.h"

void writeToLog(char* msg, char* source, char* user) {
    char tm_str[32] = "";
    time_t rawtime;
    time(&rawtime);
#if defined(WIN32) || defined(_MSC_VER)  
    struct tm timeinfo;
    localtime_s(&timeinfo, &rawtime);
#else
    struct tm* timeinfo;
    timeinfo = localtime(&rawtime);
#endif

    _snprintf_s(tm_str, 32, -1, "%d/%02d/%02d %02d:%02d:%02d",
#if defined(WIN32) || defined(_MSC_VER)  
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);
#else
        timeinfo->tm_year + 1900,
        timeinfo->tm_mon + 1,
        timeinfo->tm_mday,
        timeinfo->tm_hour,
        timeinfo->tm_min,
        timeinfo->tm_sec);
#endif
    FILE* logfile;
#if defined(WIN32) || defined(_MSC_VER)  
    fopen_s(&logfile, LOG_FILE, "a");
#else
    logfile = fopen(LOG_FILE, "a");
#endif
    if (logfile != NULL) {
        if (strlen(user) > 0) {
            fprintf(logfile, "[%s] source=%s user=%s %s\n", tm_str, source, user, strReplace(strReplace(msg, "\r", ""), "\n", ""));
        }
        else {
            fprintf(logfile, "[%s] source=%s %s\n", tm_str, source, strReplace(strReplace(msg, "\r", ""), "\n", ""));
        }
        fclose(logfile);
    }
}

char* strReplace(char* orig, char* rep, char* with) {
	// set the length of this the largest string length that will be converted
	char newstr[512] = ""; // not ideal, but meh...

	for (int i = 0; i < (int)strlen(orig); i++) {
		if (strncmp(orig + i, rep, strlen(rep)) == 0) {
			strcat_s(newstr, sizeof(newstr), with);
			i = i + (int)strlen(rep) - 1;
		}
		else {
			char append[] = { orig[i],'\0' };
			strcat_s(newstr, sizeof(newstr), append);
		}
	}
	return _strdup(newstr);
}

void ustr(char* s) {
	for (int i = 0; s[i] != '\0'; i++) {
		s[i] = toupper(s[i]);
	}
}

void lstr(char* s) {
	for (int i = 0; s[i] != '\0'; i++) {
		s[i] = tolower(s[i]);
	}
}

int indexOfChar(char* s, char c) {
	int ret = -1;
	for (int i = 0; s[i]!='\0'; i++) {
		if (s[i] == c) {
			ret = i;
			break;
		}
	}
	return ret;
}

/**
 *
 * Splits a string into an array. Used for parsing inbound packets and stats,
 * among other things.
 * Contains a couple compiler warnings. I tried to fix them, but then the function
 * did not work correctly. Will try to fix them later.
 *
 */
int split(const char* txt, char delim, char*** tokens)
{
	int* tklen, * t, count = 1;
	char** arr, * p = (char*)txt;

	while (*p != '\0') if (*p++ == delim) count += 1;
	t = tklen = calloc(count, sizeof(int));
	for (p = (char*)txt; *p != '\0'; p++) *p == delim ? *t++ : (*t)++;
	*tokens = arr = malloc(count * sizeof(char*));
	t = tklen;
	p = *arr++ = calloc(*(t++) + 1, sizeof(char*)); // TODO: Dereferencing NULL pointer 'arr' and 't'. 
	while (*txt != '\0')
	{
		if (*txt == delim)
		{
			p = *arr++ = calloc(*(t++) + 1, sizeof(char*));
			txt++;
		}
		else *p++ = *txt++; // TODO: Derefferecing NULL pointer 'p'
	}
	free(tklen);
	return count;
}

void processPacket(char* packet, char** fromUser, char** fromSite, char** fromRoom, char** toUser, char** msgExt, char** toRoom, char** body)
{
	char** field;
	int fieldCount = split(packet, '~', &field);
	if (fieldCount >= 7) {
		*fromUser = field[0];
		*fromSite = field[1];
		*fromRoom = field[2];
		*toUser = field[3];
		*msgExt = field[4];
		*toRoom = field[5];
		*body = field[6];
	}
}

void parseStats(char* stats, char** bbses, char** rooms, char** users, char** activity) {

	char** stat;
	int statCount = split(stats, ' ', &stat);
	if (statCount >= 4) {
		*bbses = stat[0];
		*rooms = stat[1];
		*users = stat[2];
		*activity = stat[3];
	}
}

char* createPacket(char* fromUser, char* fromSite, char* fromRoom, char* toUser, char* msgExt, char* toRoom, char* body) {
	char packet[PACKET_LEN];
	_snprintf_s(packet, PACKET_LEN, -1, "%s~%s~%s~%s~%s~%s~%s~\n", fromUser, fromSite, fromRoom, toUser, msgExt, toRoom, body);
	return _strdup(packet);
}

char* pipeToAnsi(char* str) {

    if (strstr(str, "|00") != NULL) {
        str = strReplace(str, "|00", BK);
    }
    if (strstr(str, "|01") != NULL) {
        str = strReplace(str, "|01", BL);
    }
    if (strstr(str, "|02") != NULL) {
        str = strReplace(str, "|02", GR);
    }
    if (strstr(str, "|03") != NULL) {
        str = strReplace(str, "|03", CY);
    }
    if (strstr(str, "|04") != NULL) {
        str = strReplace(str, "|04", RE);
    }
    if (strstr(str, "|05") != NULL) {
        str = strReplace(str, "|05", MA);
    }
    if (strstr(str, "|06") != NULL) {
        str = strReplace(str, "|06", BR);
    }
    if (strstr(str, "|07") != NULL) {
        str = strReplace(str, "|07", GY);
    }

    // bright colors

    if (strstr(str, "|08") != NULL) {
        str = strReplace(str, "|08", DG);
    }
    if (strstr(str, "|09") != NULL) {
        str = strReplace(str, "|09", LB);
    }
    if (strstr(str, "|10") != NULL) {
        str = strReplace(str, "|10", LG);
    }
    if (strstr(str, "|11") != NULL) {
        str = strReplace(str, "|11", LC);
    }
    if (strstr(str, "|12") != NULL) {
        str = strReplace(str, "|12", LR);
    }
    if (strstr(str, "|13") != NULL) {
        str = strReplace(str, "|13", LM);
    }
    if (strstr(str, "|14") != NULL) {
        str = strReplace(str, "|14", YE);
    }
    if (strstr(str, "|15") != NULL) {
        str = strReplace(str, "|15", WH);
    }

    // background colors

    if (strstr(str, "|16") != NULL) {
        str = strReplace(str, "|16", BBK);
    }
    if (strstr(str, "|17") != NULL) {
        str = strReplace(str, "|17", BBL);
    }
    if (strstr(str, "|18") != NULL) {
        str = strReplace(str, "|18", BGR);
    }
    if (strstr(str, "|19") != NULL) {
        str = strReplace(str, "|19", BCY);
    }
    if (strstr(str, "|20") != NULL) {
        str = strReplace(str, "|20", BRE);
    }
    if (strstr(str, "|21") != NULL) {
        str = strReplace(str, "|21", BMA);
    }
    if (strstr(str, "|22") != NULL) {
        str = strReplace(str, "|22", BBR);
    }
    if (strstr(str, "|23") != NULL) {
        str = strReplace(str, "|23", BGY);
    }

    // flashing colors, or "bright backgrounds" (aka iCE colors) 
    // depending on terminal mode...
    // these are annoying, so just treat them as normal colors.
    if (strstr(str, "|24") != NULL) {
        str = strReplace(str, "|24", DG); // except for this one.. we don't want black foregrounds on black backgrounds...
    }
    if (strstr(str, "|25") != NULL) {
        str = strReplace(str, "|25", BL);
    }
    if (strstr(str, "|26") != NULL) {
        str = strReplace(str, "|26", GR);
    }
    if (strstr(str, "|27") != NULL) {
        str = strReplace(str, "|27", CY);
    }
    if (strstr(str, "|28") != NULL) {
        str = strReplace(str, "|28", RE);
    }
    if (strstr(str, "|29") != NULL) {
        str = strReplace(str, "|29", MA);
    }
    if (strstr(str, "|30") != NULL) {
        str = strReplace(str, "|30", BR);
    }
    if (strstr(str, "|31") != NULL) {
        str = strReplace(str, "|31", GY);
    }

    return str;
}

int countOfChars(char* str, char c) {
    int count = 0;
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == c) {
            count = count + 1;
        }
    }
    return count;
}

int loadData(struct settings* data, char* filename) {
    FILE* infile;
#if defined(WIN32) || defined(_MSC_VER)
    fopen_s(&infile, filename, "rb");
#else
    infile = fopen(filename, "rb");
#endif
    if (infile == NULL) {
        return 1;
    }
    fread(data, sizeof(struct settings), 1, infile);
    fclose(infile);
    return 0;
}

size_t saveData(struct settings* data, char* filename) {
    FILE* outfile;
#if defined(WIN32) || defined(_MSC_VER)
    fopen_s(&outfile, filename, "wb");
#else
    outfile = fopen(filename, "wb");
#endif

    if (outfile == NULL) {
        return 1;
    }
    size_t flag = 0;
    flag = fwrite(data, sizeof(struct settings), 1, outfile);
    fclose(outfile);
    return flag;
}

int loadUser(struct userdata* data, char* filename) {
    FILE* infile;
#if defined(WIN32) || defined(_MSC_VER)
    fopen_s(&infile, filename, "rb");
#else
    infile=fopen(filename, "rb");
#endif
    if (infile == NULL) {
        return 1;
    }
    fread(data, sizeof(struct userdata), 1, infile);
    fclose(infile);
    return 0;
}

size_t saveUser(struct userdata* data, char* filename) {
    FILE* outfile;
#if defined(WIN32) || defined(_MSC_VER)
    fopen_s(&outfile, filename, "wb");
#else
    outfile=fopen(filename, "wb");
#endif

    if (outfile == NULL) {
        return 1;
    }
    size_t flag = 0;
    flag = fwrite(data, sizeof(struct userdata), 1, outfile);
    fclose(outfile);
    return flag;
}


#if defined(WIN32) || defined(_MSC_VER)
/**
 *
 * This function displays a string containing pipe color codes on the console.
 *
 * On Windows 10 and earlier, we have to iterate through the string, find any
 * pipe character followed by 2 digits, and use those digits to set the
 * background and foreground colors with the SetConsoleTextAttribute function.
 *
 */
void printPipeCodeString(char* str) {

    int fg = 7;
    int bg = 0;

    for (int i = 0; i < (int)strlen(str); i++) {

        if (str[i] == '|' && i < ((int)strlen(str) - 2)) { // check the next 2 characters for digits
            if (isdigit(str[i + 1]) && isdigit(str[i + 2])) {
                // it's a pipe code...

                int pcci = 0;
                char pcc[3] = "";
                char cmd[10] = "";
                strncpy_s(pcc, sizeof(pcc), str + i + 1, 2);
                pcci = atoi(pcc);

                if (pcci >= 0 && pcci <= 15) {
                    fg = pcci;
                }
                else if (pcci > 15 && pcci < 24) {
                    bg = pcci;
                }

                SetConsoleTextAttribute(hCon, ((bg & 0x0F) << 4) + (fg & 0x0F));
                i = i + 2;
            }
            else {
                printf("%c", str[i]);
            }
        }
        else
        {
            printf("%c", str[i]);
        }
    }
}
#else

/**
 *
 * On Linux, we simply replace the pipe color codes with the equivalent
 * ANSI color codes. ANSI works fine on Windows 11 as well, but not 
 * Windows 10 or earlier.
 *
 */
void printPipeCodeString(char* str) {
    printf("%s", pipeToAnsi(str));
}

#endif

#if defined(WIN32) || defined(_MSC_VER)

void clearScreen()
{
    printPipeCodeString("|07|16");

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    DWORD                      count;
    DWORD                      cellCount;
    COORD                      homeCoords = { 0, 0 };

    /* Get the number of cells in the current buffer */
    if (!GetConsoleScreenBufferInfo(hCon, &csbi)) return;
    cellCount = csbi.dwSize.X * csbi.dwSize.Y;

    /* Fill the entire buffer with spaces */
    if (!FillConsoleOutputCharacter(
        hCon,
        (TCHAR)' ',
        cellCount,
        homeCoords,
        &count
    )) return;

    /* Fill the entire buffer with the current colors and attributes */
    if (!FillConsoleOutputAttribute(
        hCon,
        csbi.wAttributes,
        cellCount,
        homeCoords,
        &count
    )) return;

    /* Move the cursor home */
    SetConsoleCursorPosition(hCon, homeCoords);
}

#else

void clearScreen()
{
    printf("%s", CL);
}

void initTermios(int echo) {
    tcgetattr(0, &old);
    current=old;
    current.c_lflag &= ~ICANON;
    if (echo) {
        current.c_lflag |= ECHO;
    } else {
        current.c_lflag &= ~ECHO;
    }
    tcsetattr(0, TCSANOW,&current);
}

void resetTermios(void) {
    tcsetattr(0,TCSANOW, &old);
}

char _getch() {
    char ch;
    initTermios(0);
    ch=getchar();
    resetTermios();
    return ch;
}

void Sleep(int ms) {
    usleep(ms * 1000);
}

#endif
