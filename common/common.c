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
            fprintf(logfile, "[%s] source=%s user=%s %s\n", tm_str, source, user, msg);
        }
        else {
            fprintf(logfile, "[%s] source=%s %s\n", tm_str, source, msg);
        }
        fclose(logfile);
    }
}

/**
 *  Removes pipe color code sequences (0-24) from a string.
 */
void stripPipeCodes(char* str) {
    int len = 0;
    for (int i = 0; i < (int)strlen(str); i++) {
        if (str[i] == '|' && i < ((int)strlen(str) - 2)) { // check the next 2 characters for digits
            if (isdigit(str[i + 1]) && isdigit(str[i + 2])) {
                i = i + 2; // skip if it's a pipe code
            }
            else {
                str[len] = str[i];
                len = len + 1;
            }
        }
        else {
            str[len] = str[i];
            len = len + 1;
        }
    }
    str[len] = '\0';
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

void removeChar(char* str, char c) {
    int readPos = 0, writePos = 0;
    while (str[readPos] != '\0') {
        if (str[readPos] != c) {
            str[writePos++] = str[readPos];
        }
        readPos++;
    }
    str[writePos] = '\0';
}

void replaceChar(char* str, char old, char new) {
    for (int i = 0; str[i] != '\0'; i++) {
        if (str[i] == old) {
            str[i] = new;
        }
    }
}

void removeSubstr(char* str, const char* sub) {
    char* match = strstr(str, sub); // Find first occurrence
    while (match != NULL) {
        size_t sub_len = strlen(sub);
        // Shift trailing characters (including '\0') to fill the gap
        memmove(match, match + sub_len, strlen(match + sub_len) + 1);
        match = strstr(str, sub);
    }
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

void freeSplitResult(char** arr, int count) {
    if (arr == NULL) return;
    for (int i = 0; i < count; i++) {
        free(arr[i]);
    }
    free(arr);
}

/**
 *
 * Splits a string into an array of `count` NUL-terminated tokens delimited
 * by `delim`. Used for parsing inbound packets and stats, among other
 * things.
 *
 * On allocation failure, returns 0 and sets *tokens = NULL. Every existing
 * call site already only reads up to the returned count (e.g.
 * `for (i = 0; i < split(...); i++)` or `if (split(...) >= N)`), so a
 * caller that doesn't explicitly check the return value simply won't touch
 * *tokens -- it will not dereference NULL, unlike the previous version.
 *
 * This also fixes a pre-existing bug where per-token buffers were sized
 * with `sizeof(char*)` (8 bytes per character on 64-bit) instead of
 * `sizeof(char)` -- harmless (over-allocation) but wasteful.
 */
int split(const char* txt, char delim, char*** tokens)
{
	*tokens = NULL;
	if (txt == NULL) {
		return 0;
	}

	// First pass: count fields and the length of each one.
	int count = 1;
	for (const char* p = txt; *p != '\0'; p++) {
		if (*p == delim) {
			count += 1;
		}
	}

	int* tklen = calloc((size_t)count, sizeof(int));
	if (tklen == NULL) {
		return 0;
	}

	int idx = 0;
	for (const char* p = txt; *p != '\0'; p++) {
		if (*p == delim) {
			idx++;
		}
		else {
			tklen[idx]++;
		}
	}

	char** arr = malloc((size_t)count * sizeof(char*));
	if (arr == NULL) {
		free(tklen);
		return 0;
	}

	// Allocate each token's buffer up front, so we can safely unwind if
	// any single allocation fails partway through.
	for (int i = 0; i < count; i++) {
		arr[i] = calloc((size_t)tklen[i] + 1, sizeof(char));
		if (arr[i] == NULL) {
			for (int j = 0; j < i; j++) {
				free(arr[j]);
			}
			free(arr);
			free(tklen);
			return 0;
		}
	}
	free(tklen);

	// Second pass: copy characters into their token buffers.
	idx = 0;
	char* dst = arr[0];
	for (const char* p = txt; *p != '\0'; p++) {
		if (*p == delim) {
			idx++;
			dst = arr[idx];
		}
		else {
			*dst++ = *p;
		}
	}

	*tokens = arr;
	return count;
}

void processPacket(char* packet, char** fromUser, char** fromSite, char** fromRoom, char** toUser, char** msgExt, char** toRoom, char** body) {
    static char empty[] = "";
    *fromUser = *fromSite = *fromRoom = *toUser = *msgExt = *toRoom = *body = empty;
	char** field;
	int fieldCount = split(packet, '~', &field);
	if (fieldCount >= 7) {
        *fromUser = _strdup(field[0]); // _strdup ?
        *fromSite = _strdup(field[1]);
        *fromRoom = _strdup(field[2]);
        *toUser = _strdup(field[3]);
        *msgExt = _strdup(field[4]);
        *toRoom = _strdup(field[5]);
        *body = _strdup(field[6]);
	}
    freeSplitResult(field, fieldCount);
}

void parseStats(char* stats, char** bbses, char** rooms, char** users, char** activity) {
    static char empty[] = "";
    *bbses = *rooms = *users = *activity = empty;
    char** stat;
	int statCount = split(stats, ' ', &stat);
	if (statCount >= 4) {
        *bbses = _strdup(stat[0]); // _strdup ?
        *rooms = _strdup(stat[1]);
        *users = _strdup(stat[2]);
        *activity = _strdup(stat[3]);
	}
    freeSplitResult(stat, statCount);
}

char* createPacket(char* fromUser, char* fromSite, char* fromRoom, char* toUser, char* msgExt, char* toRoom, char* body) {
	char packet[PACKET_LEN];
	_snprintf_s(packet, PACKET_LEN, -1, "%s~%s~%s~%s~%s~%s~%s~\n", fromUser, fromSite, fromRoom, toUser, msgExt, toRoom, body);
	return _strdup(packet);
}

/**
 * Converts pipe/MCI color codes ("|00".."|31") in str to their ANSI escape
 * equivalents.
 *
 * IMPORTANT: the caller owns the returned string and must free() it -- every
 * call site previously did `od_disp_emu(pipeToAnsi(x), TRUE)` without
 * capturing or freeing the result, and internally this function replaced
 * `str` with a freshly-strdup'd string (via strReplace()) on every matching
 * code without ever freeing the previous value, so a single line of text
 * with several distinct color codes could leak multiple intermediate
 * allocations *in addition to* the final, never-freed return value. Called
 * once per visible line on every scrollback keypress, that added up fast.
 * Use the dispEmuPipe() wrapper in main.c for the common
 * "convert, display, discard" pattern instead of calling this directly.
 *
 * `str`'s original value (the caller's pointer, which may be a string
 * literal) is never freed -- only allocations this function makes itself
 * are released before being replaced.
 */
char* pipeToAnsi(char* str) {
    char* result = str;   // caller's original pointer -- never free this one
    bool ownsResult = false;

#define PIPE_REPLACE(code, ansi) \
    if (strstr(result, code) != NULL) { \
        char* next = strReplace(result, code, ansi); \
        if (ownsResult) { free(result); } \
        result = next; \
        ownsResult = true; \
    }

    PIPE_REPLACE("|00", BK);
    PIPE_REPLACE("|01", BL);
    PIPE_REPLACE("|02", GR);
    PIPE_REPLACE("|03", CY);
    PIPE_REPLACE("|04", RE);
    PIPE_REPLACE("|05", MA);
    PIPE_REPLACE("|06", BR);
    PIPE_REPLACE("|07", GY);

    // bright colors

    PIPE_REPLACE("|08", DG);
    PIPE_REPLACE("|09", LB);
    PIPE_REPLACE("|10", LG);
    PIPE_REPLACE("|11", LC);
    PIPE_REPLACE("|12", LR);
    PIPE_REPLACE("|13", LM);
    PIPE_REPLACE("|14", YE);
    PIPE_REPLACE("|15", WH);

    // background colors

    PIPE_REPLACE("|16", BBK);
    PIPE_REPLACE("|17", BBL);
    PIPE_REPLACE("|18", BGR);
    PIPE_REPLACE("|19", BCY);
    PIPE_REPLACE("|20", BRE);
    PIPE_REPLACE("|21", BMA);
    PIPE_REPLACE("|22", BBR);
    PIPE_REPLACE("|23", BGY);

    // flashing colors, or "bright backgrounds" (aka iCE colors) 
    // depending on terminal mode...
    // these are annoying, so just treat them as normal colors.
    PIPE_REPLACE("|24", DG); // except for this one.. we don't want black foregrounds on black backgrounds...
    PIPE_REPLACE("|25", BL);
    PIPE_REPLACE("|26", GR);
    PIPE_REPLACE("|27", CY);
    PIPE_REPLACE("|28", RE);
    PIPE_REPLACE("|29", MA);
    PIPE_REPLACE("|30", BR);
    PIPE_REPLACE("|31", GY);

#undef PIPE_REPLACE

    // Always hand back a string the caller can unconditionally free(), even
    // when no codes were present and nothing was allocated above.
    if (!ownsResult) {
        result = _strdup(result);
    }
    return result;
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

/**
 * Bounded, always-NUL-terminating replacement for the Windows secure-CRT
 * strcpy_s() on POSIX platforms. Unlike strncpy(), this guarantees dest is
 * NUL-terminated even when source is longer than destsize (it truncates
 * rather than leaving dest unterminated, which previously allowed
 * out-of-bounds reads by every downstream strlen()/strcat()/printf("%s")
 * call on a truncated buffer).
 */
int safe_strcpy_s(char* dest, size_t destsize, const char* source) {
    if (dest == NULL || destsize == 0) {
        return -1;
    }
    if (source == NULL) {
        dest[0] = '\0';
        return -1;
    }
    size_t srclen = strlen(source);
    size_t copylen = (srclen >= destsize) ? destsize - 1 : srclen;
    memmove(dest, source, copylen);
    dest[copylen] = '\0';
    return 0;
}

/**
 * Bounded, always-NUL-terminating replacement for strncpy_s(). Copies at
 * most `count` bytes from source, but never more than destsize-1, and
 * always NUL-terminates dest.
 */
int safe_strncpy_s(char* dest, size_t destsize, const char* source, size_t count) {
    if (dest == NULL || destsize == 0) {
        return -1;
    }
    if (source == NULL) {
        dest[0] = '\0';
        return -1;
    }
    size_t srclen = strlen(source);
    size_t copylen = (count < srclen) ? count : srclen;
    if (copylen >= destsize) {
        copylen = destsize - 1;
    }
    memmove(dest, source, copylen);
    dest[copylen] = '\0';
    return 0;
}

/**
 * Bounds-checked replacement for strcat_s(). The previous POSIX shim mapped
 * this directly to plain strcat(), which ignored the size bound entirely --
 * this was a genuine unbounded stack buffer overflow anywhere strcat_s()
 * was used with network-derived data (word wrapping, log-string building,
 * CTCP reply formatting, etc). This version truncates safely and always
 * NUL-terminates dest, and refuses to touch dest if it isn't already
 * properly terminated within destsize (which indicates dest is already in
 * a corrupted/unsafe state).
 */
int safe_strcat_s(char* dest, size_t destsize, const char* source) {
    if (dest == NULL || destsize == 0 || source == NULL) {
        return -1;
    }
    size_t destlen = strnlen(dest, destsize);
    if (destlen >= destsize) {
        // dest has no NUL within destsize bytes -- refuse to append blindly.
        return -1;
    }
    size_t remaining = destsize - destlen - 1;
    size_t srclen = strlen(source);
    size_t copylen = (srclen < remaining) ? srclen : remaining;
    memmove(dest + destlen, source, copylen);
    dest[destlen + copylen] = '\0';
    return 0;
}

int _snprintf_s(char* buffer, size_t sizeOfBuffer, size_t count, const char* format, ...) {
    int retval;
    va_list ap;

    if ((count != -1) && (count < sizeOfBuffer)) {
        sizeOfBuffer = count;
    }

    va_start(ap, format);
    retval = vsnprintf(buffer, sizeOfBuffer, format, ap);
    va_end(ap);

    if ((0 <= retval) && (sizeOfBuffer <= (size_t)retval)) {
        retval = -1;
    }

    return retval;
}

#endif
