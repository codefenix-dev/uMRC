/*

 uMRC Client

 This is an OpenDoors door that interfaces with the uMRC Bridge and lets users
 chat with other users in MRC.

 



 */


#if defined(WIN32) || defined(_MSC_VER)

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include <shellapi.h>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#pragma comment (lib, "shell32.lib")

CRITICAL_SECTION gChattersLock;
#else

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <dirent.h>

static pthread_mutex_t gChattersLock = PTHREAD_MUTEX_INITIALIZER;
#endif

#include <sys/stat.h>
#include <sys/timeb.h>

#include "OpenDoor.h"
#include "../common/common.h"
#include "func.h"

#define PROGRAM "umrc-client"
#define DIVIDER "`bright blue`___________________________________________________________________________\r\n``"
#define DEFAULT_ROOM "lobby"
#define DEFAULT_JOIN_MSG "|07- |11%s |03has arrived."
#define DEFAULT_EXIT_MSG "|07- |12%s |04has left chat."
#define CTCP_ROOM "ctcp_echo_channel"
#define CHAT_CURSOR "`flashing %s`\262`bright black`\372``"

bool gRoomTopicChanged = false;
bool gLatencyChanged = false;
bool gUserCountChanged = false;
bool gMentionCountChanged = false;

bool gIsInChat = false;
bool isChatPaused = false;

int gMentionCount = 0;
int gChatterCount = 0;
int gTwitCount = 0;
char gDisplayChatterName[80] = "";
char gRoom[30] = "";
char gTopic[55] = "";
char gLatency[6] = "";
char gFromSite[140] = "";
char gLastDirectMsgFrom[36] = "";
char gUserDataFile[260] = "";
char gTwitFile[260] = "";
char gUserIP[30] = "";

#define TOPIC_FG_DEFAULT_1 "bright white"
#define TOPIC_BG_DEFAULT_1 "black"
#define TOPIC_FG_DEFAULT_2 "white"
#define TOPIC_BG_DEFAULT_2 "black"
#define TOPIC_FG_DEFAULT_3 "bright black"
#define TOPIC_BG_DEFAULT_3 "black"
#define USERCOUNT_FG_DEFAULT "bright white"
#define USERCOUNT_BG_DEFAULT "black"
#define LATENCY_FG_DEFAULT "bright white"
#define LATENCY_BG_DEFAULT "black"
#define MENTIONS_FG_DEFAULT "bright white"
#define MENTIONS_BG_DEFAULT "black"
#define BUFFER_FG_DEFAULT_1 "bright white"
#define BUFFER_BG_DEFAULT_1 "black"
#define BUFFER_FG_DEFAULT_2 "white"
#define BUFFER_BG_DEFAULT_2 "black"

char gTopicFg1[20] = TOPIC_FG_DEFAULT_1;
char gTopicBg1[20] = TOPIC_BG_DEFAULT_1;
char gTopicFg2[20] = TOPIC_FG_DEFAULT_2;
char gTopicBg2[20] = TOPIC_BG_DEFAULT_2;
char gTopicFg3[20] = TOPIC_FG_DEFAULT_3;
char gTopicBg3[20] = TOPIC_BG_DEFAULT_3;
char gUserCountFg[20] = USERCOUNT_FG_DEFAULT;
char gUserCountBg[20] = USERCOUNT_BG_DEFAULT;
char gLatencyFg[20] = LATENCY_FG_DEFAULT;
char gLatencyBg[20] = LATENCY_BG_DEFAULT;
char gMentionsFg[20] = MENTIONS_FG_DEFAULT;
char gMentionsBg[20] = MENTIONS_BG_DEFAULT;
char gBufferFg1[20] = BUFFER_FG_DEFAULT_1;
char gBufferBg1[20] = BUFFER_BG_DEFAULT_1;
char gBufferFg2[20] = BUFFER_FG_DEFAULT_2;
char gBufferBg2[20] = BUFFER_BG_DEFAULT_2;

char* gScrollBack;
char* gMentions;

char** gTwits;
char** gChattersInRoom;

// dummy theme in case the theme file doesn't load properly
char gStatusThemeLine1[400] = "\325\315\315[                                                                 /help ]\315\315\270";
char gStatusThemeLine2[400] = "\324\315\315[ users:     ]\315\315[ latency:      ]\315\315\315[ mentions:     ]\315[ buffer:         ]\315\315\276";

struct messageQueue {
    char message[512];
    bool isMention;
};
struct messageQueue mq;

time_t gLastActTm;

#define DEFAULT_BRACKETS_COUNT 20
const char* DEFAULT_BRACKETS[DEFAULT_BRACKETS_COUNT] = {
    "<>",
    "{}",
    "[]",
    "()",
    "@@",
    "!!",
    ":)",
    "//",
    "==",
    "--",
    "++",
    "%%",
    "^^",
    "##",
    "$$",
    "**",
    "..",
    "][",
    ">>",
    "&}"
};

const char* CURSOR_COLORS[16] = {
    "black",
    "blue",
    "green",
    "cyan",
    "red",
    "magenta",
    "brown",
    "grey",
    "bright black",
    "bright blue",
    "bright green",
    "bright cyan",
    "bright red",
    "bright magenta",
    "bright yellow",
    "bright white"
};

const char* ACTIVITY[4] = {
    "`bright black`NUL``",
    "`bright yellow`LOW``",
    "`bright green`MED``",
    "`bright red`HI``"
};

struct settings cfg;
struct userdata user;

SOCKET mrcSock = INVALID_SOCKET;


/**
 * Converts pipe/MCI codes in str to ANSI and displays it via od_disp_emu(),
 * then frees the converted string. pipeToAnsi() always returns a heap
 * string the caller must free -- use this instead of calling
 * od_disp_emu(pipeToAnsi(x), y) directly, which discarded the pointer and
 * leaked it (and pipeToAnsi()'s internal intermediate allocations) on every
 * single call.
 */
void dispEmuPipe(char* str, BOOL immediate) {
    char* ansi = pipeToAnsi(str);
    od_disp_emu(ansi, immediate);
    free(ansi);
}

/**
 *  Pauses and waits for any key input from the user.
 */
void doPause() {
    od_printf("\r\n `bright white`[`cyan`Press any key to continue`bright white`]`white` ");
    od_get_key(TRUE);
}

/**
 * This is basically a re-implementation of od_input_str
 * which handles both backspace and DEL, and clearly shows 
 * where text input ends.
 */
void getInputString(char* input, INT nMaxLength, unsigned char chMin, unsigned char chMax) {

    bool updateInput = false;
    char key = ' ';
    tODInputEvent InputEvent;

    for (int i = 0; i < nMaxLength; i++) {
        dispEmuPipe("|17 ", TRUE);
    }
    for (int i = 0; i < nMaxLength; i++) {
        od_putch('\b');
    }

    for (int i = 0; i < (int)strlen(input); i++) {
        od_putch(input[i]);
    }

    while (true) {
        Sleep(0);
        updateInput = false;

        if (od_get_input(&InputEvent, 1, GETIN_NORMAL) == FALSE) {
            od_sleep(0);
            continue;
        }
        if (InputEvent.EventType == EVENT_EXTENDED_KEY) {
            switch (InputEvent.chKeyPress)
            {
            case OD_KEY_DELETE:
                // Treat the same as Backspace
                if (strlen(input) > 0) {
                    input[strlen(input) - 1] = '\0';
                    updateInput = true;
                    key = 8;
                }
                break;
            }
        }
        else if (InputEvent.EventType == EVENT_CHARACTER) {
            key = InputEvent.chKeyPress;

            if (key == 13 || key == 10) {
                break;
            }
            else if (key == 8) { // backspace
                if (strlen(input) > 0) {
                    input[strlen(input) - 1] = '\0';
                    updateInput = true;
                }
            }
            else if (key >= chMin && key <= chMax) { // allowed characters
                // Add the keystroke to the input string...            
                if ((int)strlen(input) < nMaxLength-1) {
                    char tmpipt[80] = "";
                    _snprintf_s(tmpipt, sizeof(tmpipt), -1, "%s%c", input, key);
                    strcpy_s(input, nMaxLength, tmpipt);
                    updateInput = true;
                }
            }
        }

        if (updateInput) {
            if (key == 8) {              // Type the backspace...
                od_disp_str("\b \b");
            }
            else {            // Any other character simply gets displayed as typed...
                od_putch(key);
            }
        }
    }
    dispEmuPipe("|16", TRUE);
}

/**
 *  Forms a randomized default display name for the chatter.
 */
void defaultDisplayName() {
    bool buildSuffix = true;
    int suffixColor = -1;
    int suffixLen = 0;
    char brackets[3] = "";
    char defaultSuf[30] = "|08|16.";
    srand(clock());
    user.chatterNameFgColor = (rand() % 11) + 2;
    strncpy_s(brackets, sizeof(brackets), DEFAULT_BRACKETS[rand() % DEFAULT_BRACKETS_COUNT], 3);
    do { 
        user.chatterNamePrefixFgColor = (rand() % 11) + 2;
    } while (user.chatterNamePrefixFgColor == user.chatterNameFgColor);
    suffixColor = user.chatterNamePrefixFgColor;
    user.chatterNamePrefix = brackets[0];    
    _snprintf_s(user.chatterNameSuffix, sizeof(user.chatterNameSuffix), -1, "|%02d%c|16", user.chatterNamePrefixFgColor, brackets[1]);
}

void showPipeColors(int lo, int hi) {
    for (int i = lo; i <= hi; i++) {
        char c[15] = "";
        _snprintf_s(c, sizeof(c), -1, i < 17 ? "|%02d%02d " : "|00|%02d%02d ", i, i);
        dispEmuPipe(c, TRUE);
    }
}

/**
 *  Prompts the user to select a color within a range. 
 */
int colorPrompt(int lo, int hi) {
    bool validEntry = false;
    int pickedColor = -1;

    showPipeColors(lo, hi);
    while (!validEntry) {
        od_printf("``\r\n\r\nPick a color (%d-%d): ", lo, hi);        
        char ci[3]="";
        getInputString(ci, 3, '0', '9');
        pickedColor = atoi(ci);
        validEntry = (pickedColor >= lo && pickedColor <= hi);
    }
    return pickedColor;
}

/**
 *  Lets a user customize their chatter display name.
 */
int editDisplayName(char* quitToWhere) {

    bool doneEditing = false;
    int changeCount = 0;
    while (!doneEditing) {

        char prefixprev[10] = "";
        char nameprev[50] = "";
        _snprintf_s(prefixprev, sizeof(prefixprev), -1, "|%02d|%02d%c", user.chatterNamePrefixFgColor, user.chatterNamePrefixBgColor, user.chatterNamePrefix);
        _snprintf_s(nameprev, sizeof(nameprev), -1, "|%02d%s", user.chatterNameFgColor, user.chatterName);
        _snprintf_s(gDisplayChatterName, sizeof(gDisplayChatterName), -1, "|%02d|%02d%c|%02d|%02d%s%s", user.chatterNamePrefixFgColor, user.chatterNamePrefixBgColor, user.chatterNamePrefix, user.chatterNameFgColor, user.chatterNameBgColor, user.chatterName, user.chatterNameSuffix);

        od_clr_scr();

        od_printf("`bright white`Display Name (aka Nick) Settings for `bright cyan`%s``\r\n", user.chatterName);
        od_printf(DIVIDER);
        od_printf("``\r\n");

        od_printf("``Preview: ");
        dispEmuPipe(gDisplayChatterName, TRUE);
        od_printf("\r\n");

        od_printf("\r\n `bright magenta`P`bright white`) `white`Edit prefix `bright black`:           ``");
        dispEmuPipe(prefixprev, TRUE);
        od_printf("\r\n `bright magenta`C`bright white`) `white`Edit name color `bright black`:       ``");
        dispEmuPipe(nameprev, TRUE);
        od_printf("\r\n `bright magenta`S`bright white`) `white`Edit suffix `bright black`:           ``");
        dispEmuPipe(user.chatterNameSuffix, TRUE);

        od_printf("``\r\n\r\n");
        od_printf(" `cyan`R`bright white`) `white`Randomize!");
        od_printf("``\r\n\r\n");
        od_printf(" `bright green`Q`bright white`) `white`Quit to %s", quitToWhere);
        od_printf("``\r\n\r\n> ");

        switch (od_get_answer("PCSRQ")) {
        case 'P':
            od_printf("\r\n``Type a single, symbol character to use as your prefix: ``\r\n > ");
            user.chatterNamePrefix = od_get_answer("!@#$%^&*()_+-=/*-+[]\\;',./{}|:<>?");
            od_printf("\r\n``Choose a `bright white`foreground color`` for this prefix: ``\r\n");
            user.chatterNamePrefixFgColor = colorPrompt(1, 15);
            od_printf("\r\n``Choose a `bright white`background color`` for this prefix: ``\r\n");
            user.chatterNamePrefixBgColor = colorPrompt(16, 23);
            changeCount = changeCount + 1;
            break;

        case 'C':
            od_printf("\r\n``Choose a `bright white`foreground color`` for your username: ``\r\n");
            user.chatterNameFgColor = colorPrompt(1, 15);
            od_printf("\r\n``Choose a `bright white`background color`` for your username: ``\r\n");
            user.chatterNameBgColor = colorPrompt(16, 23);
            changeCount = changeCount + 1;
            break;

        case 'S':
            od_printf("`bright white`Suffix``\r\n\r\n");
            od_printf("Enter a suffix to add to your username:\r\n\r\n `bright black`(pipe colors allowed)\r\n\r\n");
            showPipeColors(1, 23);
            od_printf("\r\n\r\n`bright white`> ");
            getInputString(user.chatterNameSuffix, 30, 32, 127);
            changeCount = changeCount + 1;
            break;

        case 'R':
            defaultDisplayName();
            changeCount = changeCount + 1;
            break;

        case 'Q':
            doneEditing = true;
            break;
        }
    }
    return changeCount;
}

void pickTheme(char* pickedTheme) {
    bool validEntry = false;
    int pickedOption = -1;
    int count = 0;
    char themeFiles[1000] = "";
#if defined(WIN32) || defined(_MSC_VER) 
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;

    if ((hFind = FindFirstFile("themes\\*.ans", &fdFile)) == INVALID_HANDLE_VALUE) {
        od_printf("Path not found: [%s]\r\n", "themes");
        doPause();
        return;
    }

    do {
        if (count == 0) {
            _snprintf_s(themeFiles, sizeof(themeFiles), -1, "%s", fdFile.cFileName);
        }
        else {
            _snprintf_s(themeFiles, sizeof(themeFiles), -1, "%s|%s", themeFiles, fdFile.cFileName);
        }
        count = count + 1;
    } while (FindNextFile(hFind, &fdFile));
    FindClose(hFind);
#else
    DIR *d;
    struct dirent *dir;
    d = opendir("themes");
    if (d) {
        while ((dir=readdir(d)) != NULL) {
            if (strstr(dir->d_name, ".ans")==NULL) {
                continue;
            }
            if (count == 0) {
                strcpy_s(themeFiles, sizeof(themeFiles), dir->d_name);
            }
            else {
                strcat_s(themeFiles, sizeof(themeFiles), "|");
                strcat_s(themeFiles, sizeof(themeFiles), dir->d_name);
            }
            count = count + 1;
        }
    } else {
        od_printf("Path not found: [%s]\r\n", "themes");
        doPause();
        return;
	}
#endif

    char** themeList;
    count = split(themeFiles, '|', &themeList);

    for (int i = 0; i < count; i++) {
        od_printf("``%d`bright black`: `bright white`%s``\r\n", i + 1, themeList[i]);
    }
    while (!validEntry) {
        od_printf("``\r\n\r\nPick a theme (%d-%d): ", 1, count);
        char ci[3]="";
        getInputString(ci, 3, '0', '9');
        pickedOption = atoi(ci);
        validEntry = (pickedOption >= 1 && pickedOption <= count);
    }
    strcpy_s(pickedTheme, 20,  themeList[pickedOption - 1]);
}

void loadTheme() {
    char themePath[30] = "";
    FILE* extFile;
    _snprintf_s(themePath, sizeof(themePath), -1, "themes%c%s", PATH_SEP, user.theme);
#if defined(WIN32) || defined(_MSC_VER)
    fopen_s(&extFile, themePath, "r");
#else
    extFile = fopen(themePath, "r");
#endif
    int lineCount = 0;
    if (extFile != NULL) {
        // Reinitialize all colors to default values
        strcpy_s(gTopicFg1, sizeof(gTopicFg1), TOPIC_FG_DEFAULT_1);
        strcpy_s(gTopicBg1, sizeof(gTopicBg1), TOPIC_BG_DEFAULT_1);
        strcpy_s(gTopicFg2, sizeof(gTopicFg2), TOPIC_FG_DEFAULT_2);
        strcpy_s(gTopicBg2, sizeof(gTopicBg2), TOPIC_BG_DEFAULT_2);
        strcpy_s(gTopicFg3, sizeof(gTopicFg3), TOPIC_FG_DEFAULT_3);
        strcpy_s(gTopicBg3, sizeof(gTopicBg3), TOPIC_BG_DEFAULT_3);
        strcpy_s(gUserCountFg, sizeof(gUserCountFg), USERCOUNT_FG_DEFAULT);
        strcpy_s(gUserCountBg, sizeof(gUserCountBg), USERCOUNT_BG_DEFAULT);
        strcpy_s(gLatencyFg, sizeof(gLatencyFg), LATENCY_FG_DEFAULT);
        strcpy_s(gLatencyBg, sizeof(gLatencyBg), LATENCY_BG_DEFAULT);
        strcpy_s(gMentionsFg, sizeof(gMentionsFg), MENTIONS_FG_DEFAULT);
        strcpy_s(gMentionsBg, sizeof(gMentionsBg), MENTIONS_BG_DEFAULT);
        strcpy_s(gBufferFg1, sizeof(gBufferFg1), BUFFER_FG_DEFAULT_1);
        strcpy_s(gBufferBg1, sizeof(gBufferBg1), BUFFER_BG_DEFAULT_1);
        strcpy_s(gBufferFg2, sizeof(gBufferFg2), BUFFER_FG_DEFAULT_2);
        strcpy_s(gBufferBg2, sizeof(gBufferBg2), BUFFER_BG_DEFAULT_2);

        char line[400] = "";
        while (fgets(line, sizeof(line), extFile)) {

            if (lineCount == 0) {
                // strip out the CRs and LFs... in case someone uses a file with just one or the other...
                strcpy_s(gStatusThemeLine1, sizeof(gStatusThemeLine1), strReplace(strReplace(line, "\r", ""), "\n", ""));
            }
            else if (lineCount == 1) {
                strcpy_s(gStatusThemeLine2, sizeof(gStatusThemeLine2), strReplace(strReplace(line, "\r", ""), "\n", ""));
            }
            else if (lineCount > 1) {

                if (strlen(line) == 0) {
                    continue;
                }
                if (line[0] == ';') {
                    continue;
                }
                char colorname[20] = "";
                char colorvalue[20] = "";
                int tokencnt = 0;
                int spcidx = indexOfChar(line, ' ');
                char* token;
                char* context = NULL;
                token = strtok_s(line, " ", &context);
                while (token != NULL) {
                    if (tokencnt == 0) {
                        strcpy_s(colorname, sizeof(colorname), token);
                    }
                    else if (tokencnt >= 1) {
                        removeNonAlphanumeric(token);
                        if (strlen(token) + strlen(colorvalue) < sizeof(colorvalue)) {
                            strcat_s(colorvalue, sizeof(colorvalue), token);
                        }
                    }
                    tokencnt = tokencnt + 1;
                    token = strtok_s(NULL, " ", &context);
                    if (token != NULL && strlen(colorvalue) > 0) {
                        strcat_s(colorvalue, sizeof(colorvalue), " ");
                    }
                }

                if (_stricmp(colorname, "TopicFg1") == 0) {
                    strcpy_s(gTopicFg1, sizeof(gTopicFg1), colorvalue);
                }
                else if (_stricmp(colorname, "TopicBg1") == 0) {
                    strcpy_s(gTopicBg1, sizeof(gTopicBg1), colorvalue);
                }
                else if (_stricmp(colorname, "TopicFg2") == 0) {
                    strcpy_s(gTopicFg2, sizeof(gTopicFg2), colorvalue);
                }
                else if (_stricmp(colorname, "TopicBg2") == 0) {
                    strcpy_s(gTopicBg2, sizeof(gTopicBg2), colorvalue);
                }
                else if (_stricmp(colorname, "TopicFg3") == 0) {
                    strcpy_s(gTopicFg3, sizeof(gTopicFg3), colorvalue);
                }
                else if (_stricmp(colorname, "TopicBg3") == 0) {
                    strcpy_s(gTopicBg3, sizeof(gTopicBg3), colorvalue);
                }
                else if (_stricmp(colorname, "UserCountFg") == 0) {
                    strcpy_s(gUserCountFg, sizeof(gUserCountFg), colorvalue);
                }
                else if (_stricmp(colorname, "UserCountBg") == 0) {
                    strcpy_s(gUserCountBg, sizeof(gUserCountBg), colorvalue);
                }
                else if (_stricmp(colorname, "LatencyFg") == 0) {
                    strcpy_s(gLatencyFg, sizeof(gLatencyFg), colorvalue);
                }
                else if (_stricmp(colorname, "LatencyBg") == 0) {
                    strcpy_s(gLatencyBg, sizeof(gLatencyBg), colorvalue);
                }
                else if (_stricmp(colorname, "MentionsFg") == 0) {
                    strcpy_s(gMentionsFg, sizeof(gMentionsFg), colorvalue);
                }
                else if (_stricmp(colorname, "MentionsBg") == 0) {
                    strcpy_s(gMentionsBg, sizeof(gMentionsBg), colorvalue);
                }
                else if (_stricmp(colorname, "BufferFg1") == 0) {
                    strcpy_s(gBufferFg1, sizeof(gBufferFg1), colorvalue);
                }
                else if (_stricmp(colorname, "BufferBg1") == 0) {
                    strcpy_s(gBufferBg1, sizeof(gBufferBg1), colorvalue);
                }
                else if (_stricmp(colorname, "BufferFg2") == 0) {
                    strcpy_s(gBufferFg2, sizeof(gBufferFg2), colorvalue);
                }
                else if (_stricmp(colorname, "BufferBg2") == 0) {
                    strcpy_s(gBufferBg2, sizeof(gBufferBg2), colorvalue);
                }
            }
            lineCount = lineCount + 1;
        }
        fclose(extFile);
    }
}

/**
 *  Updates the Room and Topic strings in the top line of the status bar
 */
void updateRoomTopic() {
    if (isChatPaused) {
        return;
    }
    char displayableTopic[65] = "";
    strcpy_s(displayableTopic, 65 - (strlen(gRoom) + 2), gTopic);
    od_set_cursor(od_control.user_screen_length - 2, 6); 
    od_printf("`%s %s`#`%s %s`%s`%s %s`:`%s %s`%s ", 
        gTopicFg2, gTopicBg2, 
        gTopicFg1, gTopicBg1, 
        gRoom, 
        gTopicFg3, gTopicBg3, 
        gTopicFg1, gTopicBg1, 
        displayableTopic);
}

/**
 *  Updates the number of users in the second line of the status bar
 */
void updateUserCount() {
    od_set_cursor(od_control.user_screen_length - 1, 13);
    od_printf("`%s %s`%-3d``", gUserCountFg, gUserCountBg, gChatterCount);
}

/**
 *  Updates the Latency in the second line of the status bar.
 */
void updateLatency() {
    int ltcy = atoi(gLatency);
    if (ltcy > 999) {
        strcpy_s(gLatency, sizeof(gLatency), ">999");
    }
    od_set_cursor(od_control.user_screen_length - 1, 31);
    od_printf(        
        "`%s %s`%-4s``", 
        ltcy > 400 ? "bright red" : (
        ltcy > 200 ? "bright yellow" : ( 
        gLatencyFg)), gLatencyBg, 
        strlen(gLatency) > 0 ? gLatency : "--");
}

/**
 *  Updates the mention counter in the second line of the status bar
 */
void updateMentions() {
    od_set_cursor(od_control.user_screen_length - 1, 52);
    od_printf("`%s %s`%-4d``", gMentionsFg, gMentionsBg, gMentionCount);
}

/**
 *  Updates the input buffer (000/140) in the second line of the status bar
 */
void updateBuffer(int typed) {
    od_set_cursor(od_control.user_screen_length - 1, 68);
    od_printf(

        "`%s %s`%03d`%s %s`/`%s %s`%03d``"

        , typed >= 135 ? "bright red" : (typed >= 120 ? "bright yellow" : gBufferFg1 )
        , gBufferBg1
        
        , typed
        , gBufferFg2
        , gBufferBg2

        , gBufferFg1
        , gBufferBg1
        
        , MSG_LEN - 1);
}

void drawStatusBar() {
    char themeLines[1024] = "";
    _snprintf_s(themeLines, sizeof(themeLines), -1, "%s\r\n%s", gStatusThemeLine1, gStatusThemeLine2);
    od_set_cursor(od_control.user_screen_length - 2, 1);
    od_disp_emu(themeLines, TRUE);

    // Line #1: Room & topic
    updateRoomTopic();
   
     // Line #2: Users, Latency, Mentions, and Input Buffer
    updateUserCount();
    updateLatency();
    updateMentions();
    updateBuffer(0);
}

/**
 *  Lets the user modify settings
 */
void enterChatterSettings(char* quitToWhere) {
    bool changesMade = false;
    bool exit = false;
    char pickedTheme[20] = "";

    // failsafe in case a user hasn't picked a theme
    if (strlen(user.theme) == 0) {
        strcpy_s(user.theme, sizeof(user.theme), "default.ans");
    } 
    loadTheme();

    while (!exit) {
        od_clr_scr();

        drawStatusBar();
        od_set_cursor(od_control.user_screen_length - 2, 6);

        od_printf("`%s %s`#`%s %s`%s`%s %s`:`%s %s`%s %s",
            gTopicFg2, gTopicBg2,
            gTopicFg1, gTopicBg1,
            strlen(gRoom) > 0 ? gRoom : user.defaultRoom,
            gTopicFg3, gTopicBg3,
            gTopicFg1, gTopicBg1,
            "Theme Preview of", user.theme);


        od_set_cursor(1, 1);
        od_printf("`bright white`Chatter Settings``\r\n");
        od_printf(DIVIDER);
        od_printf("``\r\n");

        od_printf(" `bright magenta`1`bright white`) `white`Display name:  ");
        dispEmuPipe(gDisplayChatterName, TRUE);
        od_printf("``\r\n");

        od_printf(" `bright magenta`2`bright white`) `white`Default room:  `bright white`%s\r\n", user.defaultRoom);

        od_printf(" `bright magenta`3`bright white`) `white`Text color:    ");
        char sampletext[80] = "";
        _snprintf_s(sampletext, sizeof(sampletext), -1, "|%02dSample text using color #%d...", user.textColor, user.textColor);
        dispEmuPipe(sampletext, TRUE);
        od_printf("``\r\n");

        od_printf(" `bright magenta`4`bright white`) `white`Chat sounds:   `bright white`%s\r\n", user.chatSounds ? "ON" : "OFF");

        od_printf(" `bright magenta`5`bright white`) `white`Enter message: ");
        dispEmuPipe(user.joinMessage, TRUE);
        od_printf("``\r\n");

        od_printf(" `bright magenta`6`bright white`) `white`Exit message:  ");
        dispEmuPipe(user.exitMessage, TRUE);
        od_printf("``\r\n");

        od_printf(" `bright magenta`7`bright white`) `white`Theme:         `bright white`%s\r\n", user.theme);

        od_printf("``\r\n");
        od_printf(" `bright green`Q`bright white`) `white`Quit to %s", quitToWhere);
        od_printf("``\r\n\r\n> ");

        switch (od_get_answer("1234567Q")) {
        case '1':
            if (editDisplayName("settings menu") > 0) {
                changesMade = true;
            }
            break;

        case '2':
            od_clr_scr();
            od_printf("`bright white`Default Room``\r\n\r\n");
            od_printf("Enter a new default room (suggestions: `bright green`lobby, `bright magenta` ddial``)\r\n\r\n`bright white`> ");
            getInputString(user.defaultRoom, 30, 32, 127);
            if (strlen(user.defaultRoom) == 0) {
                strcpy_s(user.defaultRoom, sizeof(user.defaultRoom), DEFAULT_ROOM);
            }
            stripPipeCodes(user.defaultRoom);
            changesMade = true;
            break;

        case '3':
            od_clr_scr();
            od_printf("`bright white`Text Color``\r\n\r\n");
            user.textColor = colorPrompt(1, 15);
            changesMade = true;
            break;

        case '4':
            user.chatSounds = !user.chatSounds;
            changesMade = true;
            od_printf("\r\n\r\n`bright white`Chat sounds are now `bright yellow`%s``", user.chatSounds ? "ON" : "OFF");
            doPause();
            break;

        case '5':
            od_clr_scr();
            od_printf("`bright white`Join Message``\r\n\r\n");
            od_printf("Enter a message you'd like to announce you when you JOIN chat:\r\n\r\n `bright black`(pipe colors allowed)\r\n\r\n");
            showPipeColors(1, 23);
            od_printf("\r\n\r\n`bright white`> ");
            getInputString(user.joinMessage, 50, 32, 127);
            if (strlen(user.joinMessage) == 0) {
                _snprintf_s(user.joinMessage, sizeof(user.joinMessage), -1, DEFAULT_JOIN_MSG, user.chatterName);
            }
            changesMade = true;
            break;

        case '6':
            od_clr_scr();
            od_printf("`bright white`Exit Message``\r\n\r\n");
            od_printf("Enter a message you'd like to announce you when you EXIT chat:\r\n\r\n `bright black`(pipe colors allowed)\r\n\r\n");
            showPipeColors(1, 23);
            od_printf("\r\n\r\n`bright white`> ");
            getInputString(user.exitMessage, 50, 32, 127);
            if (strlen(user.exitMessage) == 0) {
                _snprintf_s(user.exitMessage, sizeof(user.exitMessage), -1, DEFAULT_EXIT_MSG, user.chatterName);
            }
            changesMade = true;
            break;

        case '7':
            od_clr_scr();
            pickTheme(pickedTheme);
            strcpy_s(user.theme, sizeof(user.theme), pickedTheme);
            changesMade = true;
            loadTheme();
            break;

        case 'Q':
            exit = true;
            break;
        }
    }
    if (changesMade) {
        saveUser(&user, gUserDataFile);
        od_printf("\r\n`bright white`Changes saved!``");
        doPause();
    }
}

#if !defined(WIN32) && !defined(_MSC_VER)
int WSAGetLastError() {
    return errno;
}
#endif

/**
 *  Sends a command packet over the bridge
 */
bool sendCmdPacket(SOCKET* sock, char* cmd, char* cmdArg) {
    int iResult;
    char cmdstr[MSG_LEN] = "";
    char packet[PACKET_LEN] = "";
    if (strlen(cmdArg) > 0) {
        _snprintf_s(cmdstr, MSG_LEN, -1, strchr(cmd, ':') ? "%s%s" : "%s %s", cmd, cmdArg);
    }
    else {
        strcpy_s(cmdstr, MSG_LEN, cmd);
    }
    strcpy_s(packet, PACKET_LEN, createPacket(user.chatterName, gFromSite, gRoom, "SERVER", "", "", cmdstr));
    iResult = send(*sock, packet, (int)strlen(packet), 0);
    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendCmdPacket failed with error: %d", WSAGetLastError());
        writeToLog(logstring, PROGRAM, od_control.user_handle);
    }
    return (iResult != SOCKET_ERROR);
}

/**
 *  Sends a CTCP command over the bridge
 */
bool sendCtcpPacket(SOCKET* sock, char* target, char* p, char* data) {
    int iResult;
    char ctcpstr[MSG_LEN] = "";
    char packet[PACKET_LEN] = "";
    _snprintf_s(ctcpstr, MSG_LEN, -1, "%s %s %s", p, user.chatterName, data);
    strcpy_s(packet, PACKET_LEN, createPacket(user.chatterName, gFromSite, CTCP_ROOM, target, "", CTCP_ROOM, ctcpstr));
    iResult = send(*sock, packet, (int)strlen(packet), 0);
    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendCtcpPacket failed with error: %d", WSAGetLastError());
        writeToLog(logstring, PROGRAM, od_control.user_handle);
    }
    return (iResult != SOCKET_ERROR);
}

/**
 *  Sends a Message packet over the bridge
 */
bool sendMsgPacket(SOCKET* sock, char* toUser, char* msgExt, char* toRoom, char* body) {
    int iResult;
    char packet[PACKET_LEN] = "";
    strcpy_s(packet, PACKET_LEN, createPacket(user.chatterName, gFromSite, gRoom, toUser, msgExt, toRoom, body));
    iResult = send(*sock, packet, (int)strlen(packet), 0);
    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendMsgPacket failed with error: %d", WSAGetLastError());
        writeToLog(logstring, PROGRAM, od_control.user_handle);
    }
    return (iResult != SOCKET_ERROR);
}

void resetInputLine() {
    updateBuffer(0);
    od_set_cursor(od_control.user_screen_length, 1);
    od_clr_line();
    for (int i = 1; i < od_control.user_screenwidth - 1; i++) {
        od_printf("`bright black`\372");
    }
    od_set_cursor(od_control.user_screen_length, 1);
}

void scrollToScrollbackSection(char** scrollLines, int start, int end, int height) {
    int row = 1;
    int fg = 7;
    int bg = 16;

    // find the last colors used before this point in the buffer
    for (int i = start; i >= 0; i--) {        
        for (int ii = (int)strlen(scrollLines[i])-2; ii >= 5; ii--) {
            if (scrollLines[i][ii] == '|' && ii < ((int)strlen(scrollLines[i]) - 2)) { // check the next 2 characters for digits
                if (isdigit(scrollLines[i][ii + 1]) && isdigit(scrollLines[i][ii + 2])) {

                    int pcci = 0;
                    char pcc[3] = "";
                    strncpy_s(pcc, sizeof(pcc), scrollLines[i] + ii + 1, 2);
                    pcci = atoi(pcc);

                    if (pcci > 0 && pcci <= 15 && fg == 7) {
                        fg = pcci;
                    }
                    else if (pcci > 16 && pcci < 24 && bg == 16) {
                        bg = pcci;
                    }
                }
            }
            if (fg != 7 || bg != 16) {
                break;
            }
        }
        if (fg != 7 || bg != 16) {
            break;
        }
    }
    char startcolor[10] = "";
    _snprintf_s(startcolor, sizeof(startcolor), -1, "|%02d|%02d", fg, bg);

    for (int i = start; i < end && row < height; i++) {
        od_set_cursor(row, 1);
        od_clr_line();
        if (i == start) {
            dispEmuPipe(startcolor, TRUE);
        }
        dispEmuPipe(scrollLines[i], TRUE);
        row = row + 1;
    }
}

/**
 *
 * initialScroll:
 *  - how far up to scroll when entering scrollback mode
 * 
 * mode: 
 *  - 0 = Standard scrollback
 *  - 1 = Mention scrollback
 * 
 */
void enterScrollBack(int initialScroll, int mode) {
    isChatPaused = true; // pause the chat before doing anything else

    bool exitScrollback = false;
    int height = od_control.user_screen_length - 2;
    char** scrollLines;
    int scrollLineCount = split(mode==0?gScrollBack:gMentions, '\n', &scrollLines);

    int scrollMax = scrollLineCount - height;
    if (scrollMax < 0) {
        scrollMax = 0;
    }
    int scrollPos = scrollMax - initialScroll;
    if (scrollPos < 0) {
        scrollPos = 0;
    }

    // Clear the visible chat window before displaying the scrollback
    for (int i = 0; i < height; i++) {
        od_set_cursor(i, 1);
        od_clr_line();
    }
    
    od_set_cursor(od_control.user_screen_length, 1);
    od_printf("``");
    od_clr_line();
    scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
    od_set_cursor(od_control.user_screen_length - 2, 1);
    od_disp_emu(gStatusThemeLine1, TRUE);
    od_clr_line();
    od_set_cursor(od_control.user_screen_length - 2, 2);
    od_printf("`%s %s`%12.12s`%s %s`:          `%s %s`\030`%s %s`/`%s %s`\031`%s %s`/`%s %s`PGUP`%s %s`/`%s %s`PGDN`%s %s`/`%s %s`HOME`%s %s`/`%s %s`END`%s %s`   `%s %s`ENTER`%s %s` to return to chat      ",
        gTopicFg1, gTopicBg1,
        mode == 0 ? "Scrollback" : "Mentions",
        gTopicFg2, gTopicBg2,
        gTopicFg1, gTopicBg1,
        gTopicFg2, gTopicBg2,
        gTopicFg1, gTopicBg1,
        gTopicFg2, gTopicBg2,
        gTopicFg1, gTopicBg1,
        gTopicFg2, gTopicBg2,
        gTopicFg1, gTopicBg1,
        gTopicFg2, gTopicBg2,
        gTopicFg1, gTopicBg1,
        gTopicFg2, gTopicBg2,
        gTopicFg1, gTopicBg1, 
        gTopicFg2, gTopicBg2,
        gTopicFg1, gTopicBg1,
        gTopicFg2, gTopicBg2
    );

    od_set_cursor(od_control.user_screen_length - 1, 2);
    od_printf("``");
    //od_clr_line();
    od_printf("`flashing %s %s`%s``", gTopicFg1, gTopicBg1,
        "                           * * * CHAT PAUSED * * *                           ");

    tODInputEvent InputEvent;

    while (!exitScrollback) {

        od_set_cursor(od_control.user_screen_length - 2, 18);
        od_printf("`%s %s`%.0f`%s %s`%c`%s %s` ``",
            gTopicFg1, gTopicBg1,
            (scrollPos / (scrollMax + 0.1)) * 100,
            gTopicFg2, gTopicBg2, 
            '%',
            gTopicFg2, gTopicBg2);            

        od_get_input(&InputEvent, OD_NO_TIMEOUT, GETIN_NORMAL);

        if (InputEvent.EventType == EVENT_EXTENDED_KEY)
        {
            switch (InputEvent.chKeyPress)
            {
            case OD_KEY_UP:
                scrollPos = scrollPos - 1;
                if (scrollPos < 0) {
                    scrollPos = 0;
                }
                scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
                break;

            case OD_KEY_DOWN:
                scrollPos = scrollPos + 1;
                if (scrollPos > scrollMax) {
                    scrollPos = scrollMax;
                }
                scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
                break;

            case '\x10':
            case OD_KEY_PGUP:
                scrollPos = scrollPos - (height-1);
                if (scrollPos < 0) {
                    scrollPos = 0;
                }
                scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
                break;

            case '\x0e':
            case OD_KEY_PGDN:
                scrollPos = scrollPos + (height-1);
                if (scrollPos > scrollMax) {
                    scrollPos = scrollMax;
                }
                scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
                break;

            case OD_KEY_HOME:
                scrollPos = 0;
                scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
                break;

            case OD_KEY_END:
                scrollPos = scrollMax;
                scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
                break;
            }
        }
        else if (InputEvent.EventType == EVENT_CHARACTER) {
            if (InputEvent.chKeyPress == 13 || InputEvent.chKeyPress == 10 || InputEvent.chKeyPress == 27) {
                exitScrollback = true;
                break;
            }
        }
    }
    freeSplitResult(scrollLines, scrollLineCount);

    // refresh the scrollLines and re-display the latest lines when exiting scrollback, in 
    // case any were received while scrolling.
    scrollLineCount = split(gScrollBack, '\n', &scrollLines);
    scrollPos = scrollLineCount - height;
    if (scrollPos < 0) {
        scrollPos = 0;
    }
    scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
    freeSplitResult(scrollLines, scrollLineCount);

    isChatPaused = false;
    drawStatusBar();
    resetInputLine();
    od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
}

/**
 *  Adds a string to the Scrollback OR to the Mentions
 *   mode=0: regular chat history scrollback
 *   mode=1: mention history
 */
void addToScrollBack(char* msg, int mode) {
    const size_t a = strlen(mode==0 ? gScrollBack : gMentions);
    const size_t b = strlen(msg);
    const size_t size_ab = a + b + 2;
    char* tmp = realloc(mode == 0 ? gScrollBack : gMentions, size_ab);
    if (tmp != NULL) {
        memcpy(tmp + a, msg, b + 1);
        if (mode == 0) {
            gScrollBack = tmp;
        }
        else if (mode==1) {
            gMentions = tmp;
        }
        strcat_s(mode == 0 ? gScrollBack : gMentions, size_ab, "\n");
    }
}

void displayMessage(char* msg, bool mention) {
    // A size of 512 allows for the standard 140 char message, plus line wraps, pipe codes, and the timestamp.
    char copyMsg[512] = ""; // This holds a copy of the source message while we're processing it.
    char dispMsg[512] = ""; // This will be the final message after adding line wraps.
    char timeStamp[30] = ""; // This holds the message timestamp.

    strcpy_s(copyMsg, sizeof(copyMsg), msg);
    _snprintf_s(timeStamp, sizeof(timeStamp), -1, (mention ? "|00|23%s|26\x1b[5m\376\x1b[0m|07" : "|08%s|07 "), getTimestamp());

    if (strLenWithoutPipecodes(copyMsg) >= (od_control.user_screenwidth - 7)) {

        // This code wraps the text neatly if it's too long for the terminal width,
        // by breaking it apart word by word and building a new string, calculating 
        // the line length as each one gets re-appended, and inserting an indented
        // CR-LF when a new line is needed.
        //
        // Since we're using strtok_s to tokenize the message string, this does have
        // the side effect of eliminating consecutive spaces if the message string is
        // long enough to require wrapping.
        //
        int linelen = 0;
        int tokencnt = 0;
        int tokenLen = 0;
        char wrappedMsg[512] = ""; 
        char* token;
        char* context = NULL;
        token = strtok_s(copyMsg, " ", &context);
        while (token != NULL) {
            bool breakUpLongToken = false;
            tokenLen = strLenWithoutPipecodes(token);
            if (tokenLen > od_control.user_screenwidth - 8) { // single word is longer than the terminal width
                breakUpLongToken = true;                      // so it needs broken up down below.
            }
            else {
                linelen = linelen + tokenLen + 1;
            }
            if (!breakUpLongToken && (linelen > (od_control.user_screenwidth - 8))) {
                strcat_s(wrappedMsg, sizeof(wrappedMsg), "\r\n      \300");
                linelen = 3 + (tokenLen + 1);
            }
            if (breakUpLongToken) { 
                char tkn1[100] = "", tkn2[100] = "";
                int breakPoint = (od_control.user_screenwidth - linelen - 7);
                strncpy_s(tkn1, sizeof(tkn1), token, breakPoint);
                strcpy_s(tkn2, sizeof(tkn2), token + breakPoint);
                strcat_s(wrappedMsg, sizeof(wrappedMsg), " ");
                strcat_s(wrappedMsg, sizeof(wrappedMsg), tkn1);
                strcat_s(wrappedMsg, sizeof(wrappedMsg), "\r\n      \300 ");
                if (strLenWithoutPipecodes(tkn2) > od_control.user_screenwidth - 8) {
                    char tkn3[100]="";
                    breakPoint = (od_control.user_screenwidth - 9);
                    strcpy_s(tkn3, sizeof(tkn3), tkn2 + breakPoint);
                    tkn2[breakPoint] = '\0';
                    strcat_s(wrappedMsg, sizeof(wrappedMsg), tkn2);
                    strcat_s(wrappedMsg, sizeof(wrappedMsg), "\r\n      \300 ");
                    strcat_s(wrappedMsg, sizeof(wrappedMsg), tkn3);
                }
                else {
                    strcat_s(wrappedMsg, sizeof(wrappedMsg), tkn2);
                }
            }
            else {
                if (tokencnt > 0) {
                    strcat_s(wrappedMsg, sizeof(wrappedMsg), " ");
                }
                strcat_s(wrappedMsg, sizeof(wrappedMsg), token);
            }
            tokencnt = tokencnt + 1;
            token = strtok_s(NULL, " ", &context);
        }
        strcpy_s(copyMsg, sizeof(copyMsg), wrappedMsg);
    }

    _snprintf_s(dispMsg, sizeof(dispMsg), -1, "%s%s\x1b[0m", timeStamp, copyMsg);

    // Save this message to the chat scrollback buffer
    addToScrollBack(dispMsg, 0);
    if (mention) {
        addToScrollBack(dispMsg, 1);
    }
    if (!isChatPaused) {
        od_scroll(1, 1, od_control.user_screenwidth, od_control.user_screen_length - 3, countOfChars(dispMsg, '\n') +1, 0);
        od_set_cursor(od_control.user_screen_length - (2 + countOfChars(dispMsg, '\n') +1), 1);
        dispEmuPipe(dispMsg, TRUE);
    }
}

/**
 * Reads the twits into the gTwits array.
 */
void loadTwits() {
    FILE* tfile;
#if defined(WIN32) || defined(_MSC_VER)  
    fopen_s(&tfile, gTwitFile, "r");
#else
    tfile = fopen(gTwitFile, "r");
#endif
    char* tbuf = malloc(40);
    strcpy_s(tbuf, 40, "");
    if (tfile != NULL) {
        char tline[40] = "";
        while (fgets(tline, sizeof(tline), tfile)) {
            const size_t a = strlen(tbuf);
            const size_t b = strlen(tline);
            const size_t size_ab = a + b + 1;
            char* tmp = realloc(tbuf, size_ab);
            if (tmp != NULL) {
                memcpy(tmp + a, tline, b + 1);
                tbuf = tmp;
            }
        }
        fclose(tfile);
        gTwitCount = split(tbuf, '\n', &gTwits);
    }
    free(tbuf);
}

bool checkTwit(char* twit) {
    bool isTwit = false;
    EnterCriticalSection(&gChattersLock);
    for (int i = 0; i < gTwitCount; i++) {
        if (_stricmp(gTwits[i], twit) == 0) {
            isTwit = true;
            break;
        }
    }
    LeaveCriticalSection(&gChattersLock);
    return isTwit;
}

/**
 * Add the new twit to the file, and then read the file into the gTwits array...
 *
 */
void addTwit(char* twit) {

    if (checkTwit(twit)) {
        return;
    }

    FILE* tfile;
#if defined(WIN32) || defined(_MSC_VER)  
    fopen_s(&tfile, gTwitFile, "a");
#else
    tfile = fopen(gTwitFile, "a");
#endif
    if (tfile != NULL) {
        fprintf(tfile, "%s\n", twit);
        fclose(tfile);
        loadTwits();
        if (checkTwit(twit)) {
            char result[140] = "";
            _snprintf_s(result, sizeof(result), -1, "|15* |14%s |07added to twit list.", twit);
            displayMessage(result, false);
        }
    }
}

/**
 * Rewrites the twit list, ommitting the one to be deleted.
 */
void removeTwit(char* twit) {
    bool wasInList = false;
    remove(gTwitFile);
    od_sleep(100);
    FILE* tfile;
#if defined(WIN32) || defined(_MSC_VER)  
    fopen_s(&tfile, gTwitFile, "w");
#else
    tfile = fopen(gTwitFile, "w");
#endif
    if (tfile != NULL) {
        for (int i = 0; i < gTwitCount; i++) {
            if (_stricmp(gTwits[i], twit) != 0 && strlen(gTwits[i]) > 0) {
                fprintf(tfile, "%s\n", gTwits[i]);
            }
            else if (_stricmp(gTwits[i], twit) == 0) {
                wasInList = true;
            }
        }
        fclose(tfile);
    }
    loadTwits();
    if (!checkTwit(twit) && wasInList) {
        char result[140] = "";
        _snprintf_s(result, sizeof(result), -1, "|15* |14%s |07removed from twit list.", twit);
        displayMessage(result, false);
    }
}

/**
 *  This function takes an incoming message string,
 *  and temporarily stores it to the "message courier"
 *  struct. If it's already holding a message, it waits
 *  until it's NULL.
 *
 *  If chat is currently paused (because the user is
 *  viewing scrollback or mentions), the incoming message
 *  gets stored directly to the scrollback.
 */
void queueIncomingMessage(char* msg, bool mention) {
    if (isChatPaused) {
        // Just send it to the displayMessage function, since it has
        // built-in handling for adding wrapped messages to scrollback
        // and won't try to display it if chat is paused.        
        displayMessage(msg, mention);
    }
    else {
        while (strlen(mq.message) > 0) {
            od_sleep(1);
        }
        strcpy_s(mq.message, sizeof(mq.message), msg);
        mq.isMention = mention;
    }
}

void listThemesInChat() {
#if defined(WIN32) || defined(_MSC_VER) 
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;
    if ((hFind = FindFirstFile("themes\\*.ans", &fdFile)) == INVALID_HANDLE_VALUE) {
        displayMessage("|07 Theme path not found", false);
        return;
    }
    displayMessage("|15* Pick a theme with |09/theme |10name", false);
    do {
        char line[200] = "";
        _snprintf_s(line, sizeof(line), -1, "|08* - |07%s", fdFile.cFileName);
        lstr(line);
        displayMessage(strReplace(line, ".ans", ""), false);
    } while (FindNextFile(hFind, &fdFile));
    FindClose(hFind);
#else
    DIR *d;
    struct dirent *dir;
    d = opendir("themes");
    if (d) {
        while ((dir=readdir(d)) != NULL) {
            if (strstr(dir->d_name, ".ans")==NULL) {
                continue;
            }
            char line[200] = "";
            _snprintf_s(line, sizeof(line), -1, "|08* - |07%s", dir->d_name);
            lstr(line);
            displayMessage(strReplace(line, ".ans", ""), false);
        }
    }
#endif
    displayMessage("|08__", false);
}

void displayPipeFileInChat(char* filename) {
    FILE* extFile;
#if defined(WIN32) || defined(_MSC_VER)  
    fopen_s(&extFile, filename, "r");
#else
    extFile = fopen(filename, "r");
#endif
    if (extFile != NULL) {
        char line[200] = "";
        while (fgets(line, sizeof(line), extFile)) {            
           displayMessage(strReplace(line, "\n", ""), false);
        }
        displayMessage("|08__", false);
        fclose(extFile);
    }
}

void displayPipeFile(char* filename) {
    FILE* extFile;
#if defined(WIN32) || defined(_MSC_VER)  
    fopen_s(&extFile, filename, "r");
#else
    extFile=fopen(filename, "r");
#endif            
    int lineCounter = 0;
    if (extFile != NULL) {
        char line[200] = "";
        while (fgets(line, sizeof(line), extFile)) {
            od_printf("\r"); // why???
            dispEmuPipe(line, TRUE);

            if (lineCounter >= od_control.user_screen_length - 2) {
                doPause();
                lineCounter = 0;
            }

            lineCounter = lineCounter + 1;
        }
        fclose(extFile);
        od_printf("\r\n");
    }
}

void processUserCommand(char* cmd, char* params) {
    if (_stricmp(cmd, "quit") == 0 || _stricmp(cmd, "q") == 0) {
        sendMsgPacket(&mrcSock, "NOTME", "", "", user.exitMessage);
        sendMsgPacket(&mrcSock, "SERVER", "", "", "LOGOFF");
        gIsInChat = false;
    }
    else if (_stricmp(cmd, "join") == 0 || _stricmp(cmd, "j") == 0) {
        char newRoom[20] = "";
        strcpy_s(newRoom, sizeof(newRoom), strReplace(params, "#", ""));  // no #
        strcpy_s(newRoom, sizeof(newRoom), strReplace(newRoom, " ", "_")); // Single word
        stripPipeCodes(newRoom); // No pipe codes

        if (strlen(newRoom) == 0) {
            displayMessage("|15* |14No room specified|07.", false);
            return;
        }

        // Let the user know they're trying to join a room they're already in.
        // Not necessarily needed, since the server doesn't consider the action
        // invalid, but we'll inform the user anyway. 
        if (_stricmp(newRoom, gRoom) == 0) {
            displayMessage("|15* |14Already in that room|07.", false);
            return;
        }

        char newRoomCmd[30] = "";
        _snprintf_s(newRoomCmd, sizeof(newRoomCmd), -1, "NEWROOM:%s:", gRoom); // include the OLD room as the first parameter...
        sendCmdPacket(&mrcSock, newRoomCmd, newRoom); // the NEW room will be included as the second parameter...
        strcpy_s(gRoom, sizeof(gRoom), newRoom); 
        sendCmdPacket(&mrcSock, "USERLIST", ""); // Need a new user list after joining a different room
    }
    else if (_stricmp(cmd, "topic") == 0) {              
        stripPipeCodes(params);  // No pipe codes
        char newTopicCmd[30] = "";
        _snprintf_s(newTopicCmd, sizeof(newTopicCmd), -1, "NEWTOPIC:%s:", gRoom);
        sendCmdPacket(&mrcSock, newTopicCmd, params);
    }
    else if (strcmp(cmd, "?") == 0) {
        sendCmdPacket(&mrcSock, "help", "");
    }
    else if (_stricmp(cmd, "rooms") == 0) {
        sendCmdPacket(&mrcSock, "list", "");
    }
    else if (_stricmp(cmd, "me") == 0) {
        char action[MSG_LEN] = "";
        _snprintf_s(action, MSG_LEN, -1, "|15* |13%s %s", user.chatterName, params);
        sendMsgPacket(&mrcSock, "", "", gRoom, action);
    }
    else if (_stricmp(cmd, "t") == 0 || _stricmp(cmd, "msg") == 0) {
        char to[36] = "";
        char msg[PACKET_LEN] = "";
        int nextspcidx = indexOfChar(params, ' ') + 1;
        if (nextspcidx > 0) {
            getSubStr(params, to, 0, nextspcidx-1);
            _snprintf_s(msg, PACKET_LEN, -1, "|15* |08(|15%s|08/|14DirectMsg|08) |07%s", user.chatterName, params + nextspcidx);
            sendMsgPacket(&mrcSock, to, "", "", msg);
            _snprintf_s(msg, PACKET_LEN, -1, "|15* |08(|14DirectMsg|08->|15%s|08) |07%s", to, params + nextspcidx);
            displayMessage(msg, false);
        }
    }
    else if (_stricmp(cmd, "r") == 0) {
        char rep[PACKET_LEN] = "";
        _snprintf_s(rep, PACKET_LEN, -1, "|15* |08(|15%s|08/|14DirectMsg|08) |07%s", user.chatterName, params);
        sendMsgPacket(&mrcSock, gLastDirectMsgFrom, "", "", rep);
        _snprintf_s(rep, PACKET_LEN, -1, "|15* |08(|14DirectMsg|08->|15%s|08) |07%s", gLastDirectMsgFrom, params);
        displayMessage(rep, false);
    }
    else if (_stricmp(cmd, "b") == 0) {
        char bcast[PACKET_LEN] = "";
        _snprintf_s(bcast, PACKET_LEN, -1, "|15* |08(|15%s|08/|14Broadcast|08) |07%s", user.chatterName, params);
        sendMsgPacket(&mrcSock, "", "", "", bcast);
    }
    else if (_stricmp(cmd, "sound") == 0) {
        user.chatSounds = !user.chatSounds;
        char sndstat[50] = "";
        _snprintf_s(sndstat, sizeof(sndstat), -1, "|15* |14Chat sounds |15%s|14.", (user.chatSounds ? "ON" : "OFF"));
        displayMessage(sndstat, false);
    }
    else if (_stricmp(cmd, "ctcp") == 0) {
        char target[36] = "";
        char ctcp_data[50] = "";
        int nextspcidx = indexOfChar(params, ' ');
        if (nextspcidx > 0) {
            getSubStr(params, target, 0, nextspcidx);
            _snprintf_s(ctcp_data, sizeof(ctcp_data), -1, "%s %s", target, params + nextspcidx + 1);
            sendCtcpPacket(&mrcSock, (strcmp(target, "*") == 0 || target[0] == '#') ? "" : target, "[CTCP]", ctcp_data);
        }
    }
    else if (_stricmp(cmd, "nick") == 0) {
        isChatPaused = true;
        editDisplayName("chat");
        // refresh the scrollLines and re-display the latest lines when exiting scrollback, in 
        // case any were received while editing.
        char** scrollLines;
        int height = od_control.user_screen_length - 2;
        int scrollLineCount = split(gScrollBack, '\n', &scrollLines);
        int scrollPos = scrollLineCount - height;
        if (scrollPos < 0) {
            scrollPos = 0;
        }
        scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
        freeSplitResult(scrollLines, scrollLineCount);
        isChatPaused = false;
        drawStatusBar();
        resetInputLine();
        od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
    }
    else if (_stricmp(cmd, "set") == 0) {
        isChatPaused = true;
        enterChatterSettings("chat");
        // refresh the scrollLines and re-display the latest lines when exiting scrollback, in 
        // case any were received while editing.
        char** scrollLines;
        int height = od_control.user_screen_length - 2;
        int scrollLineCount = split(gScrollBack, '\n', &scrollLines);
        int scrollPos = scrollLineCount - height;
        if (scrollPos < 0) {
            scrollPos = 0;
        }
        scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
        freeSplitResult(scrollLines, scrollLineCount);
        isChatPaused = false;
        drawStatusBar();
        resetInputLine();
        od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
    }
    else if (_stricmp(cmd, "twit") == 0) {
        char action[10] = "";
        int nextspcidx = indexOfChar(params, ' ') + 1;
        if (nextspcidx > 0) {
            getSubStr(params, action, 0, nextspcidx - 1);
            if (_stricmp(action, "add") == 0 && _stricmp(params + 4, user.chatterName) != 0) {
                addTwit(params + 4);                
            } else if (_stricmp(action, "del") == 0) {
                removeTwit(params + 4);
            }
        }
        else if (_stricmp(params, "clear") == 0) {
            gTwitCount = split("", '\n', &gTwits);
            remove(gTwitFile);
            displayMessage("|15* |07Twit list cleared|08.", false);
        }
        else if (_stricmp(params, "list") == 0) {
            displayMessage("|09___ |15Twit List|09___", false);
            displayPipeFileInChat(gTwitFile);
        }
    }
    else if (_stricmp(cmd, "help") == 0) {
        char helpfile[30] = "";
        _snprintf_s(helpfile, sizeof(helpfile), -1, "screens%chelp%s.txt", PATH_SEP, strlen(params) > 0 ? params : "");
        displayPipeFileInChat(helpfile);
    }
    else if (_stricmp(cmd, "quote") == 0) {
        sendCmdPacket(&mrcSock, params, "");
    }
    else if (_stricmp(cmd, "redraw") == 0) {
        drawStatusBar();
        displayMessage("|15* |14Status bar redrawn", false);
    }
    else if (_stricmp(cmd, "scroll") == 0) {
        enterScrollBack(0, 0);
    }
    else if (_stricmp(cmd, "mentions") == 0) {
        gMentionCount = 0;
        enterScrollBack(0, 1);
        updateMentions();
    }
    else if (_stricmp(cmd, "theme") == 0) {
        if (strlen(params) == 0) {
            listThemesInChat();
        }
        else {
            _snprintf_s(user.theme, sizeof(user.theme), -1, "%s.ans", params);
            loadTheme();
            drawStatusBar();
        }
    }
    else if (_stricmp(cmd, "cls") == 0) {
        isChatPaused = true;
        // Insert a screen's worth of blank lines to the chat history, and then
        // scroll to the first blank line.
        // We'll add a line indicating that the screen was cleared, and when.
        displayMessage("|15 * * * CHAT AREA CLEARED * * * |07", false);
        for (int i = 0; i < od_control.user_screen_length - 3; i++) {
            addToScrollBack(" ", 0);
        }
        // refresh the scrollLines and re-display the latest lines when exiting scrollback, in 
        // case any were received while editing.
        char** scrollLines;
        int height = od_control.user_screen_length - 2;
        int scrollLineCount = split(gScrollBack, '\n', &scrollLines);
        int scrollPos = scrollLineCount - height;
        if (scrollPos < 0) {
            scrollPos = 0;
        }
        scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
        freeSplitResult(scrollLines, scrollLineCount);
        isChatPaused = false;
        drawStatusBar();
        resetInputLine();
        od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
    }

    // Any other server command goes straight to the server and processed there.
    // If the server received an invalid command, it will let the user know.
    else {
        ustr(cmd);
        sendCmdPacket(&mrcSock, cmd, params);
        
        // request a new USERLIST when checking the current users
        if (_stricmp(cmd, "users") == 0 ||
            _stricmp(cmd, "whoon") == 0 ||
            _stricmp(cmd, "chatters") == 0) {
            sendCmdPacket(&mrcSock, "USERLIST", "");
        }
    }
}

void processServerMessage(char* body, char* toUser) {
        
    if (body == NULL) { // don't process if there's nothing to send..
        return;
    }
    if (strlen(body) == 0) {
        return;
    }

    // Parse the body for command strings
    char cmd[141] = "";
    char params[512] = ""; // needs to be large enough to hold long lists of data, e.g.: USERLISTs
    int cmdsep = indexOfChar(body, ':');

    if (cmdsep > 0) {
        getSubStr(body, cmd, 0, cmdsep);
        getSubStr(body, params, cmdsep+1, (int)strlen(body));
    }
    else {
        strncpy_s(cmd, sizeof(cmd), body, 140);
    }

    // Implemented SERVER commands - notes provided from the MRC developer wiki on how each is handled:
    //
    if (strcmp(cmd, "BANNER") == 0 || strcmp(cmd, "NOTIFY") == 0) {
        // This client doesn't currently make special use of banners.
        // If a banner is ever sent, we simply display it in chat.
        //
        // Notification messages have the same structure as a 
        // BANNER packet, so we handle it the same way.
        char banner[512] = "";
        _snprintf_s(banner, sizeof(banner), -1, (strcmp(cmd, "BANNER") == 0 ? "|14* |13<|15#|13> |15%s" : "|14* |14<|15!|14> |15%s"), params);
        queueIncomingMessage(banner, false);
    }
    else if (strcmp(cmd, "ROOMTOPIC") == 0) {
        // ROOMTOPIC : Server will send new topic when changed
        // - No pipe codes
        // - Room name cannot contain spaces
        // - Topic can contain spaces
        strncpy_s(gTopic, sizeof(gTopic), params + strlen(gRoom) + 1, 54);
        gRoomTopicChanged = true;
    }
    else if (strcmp(cmd, "USERROOM") == 0) {
        // USERROOM: Server confirm the room user is in, usually sent after NEWROOM but may be the result of other reasons.[NEW in 1.3]
        // Client must enforce or routing will break
        strncpy_s(gRoom, sizeof(gRoom), params, 29);
        gRoomTopicChanged = true;
    }
    else if (strcmp(cmd, "USERNICK") == 0) {
        // USERNICK : Server confirm the nick user can use, usually sent after IAMHERE but may be the result of other reasons. [NEW in 1.3]
        // Client must enforce or routing will break.
        // This will be used to avoid delivery conflict in case 2 users with the same nick on 2 different BBSes are connected.
        // The server will append a number to the original nick to resolve the conflict.
        strcpy_s(user.chatterName, sizeof(user.chatterName), params);
        _snprintf_s(gDisplayChatterName, sizeof(gDisplayChatterName), -1, "|%02d|%02d%c|%02d|%02d%s%s|16", user.chatterNamePrefixFgColor, user.chatterNamePrefixBgColor, user.chatterNamePrefix, user.chatterNameFgColor, user.chatterNameBgColor, user.chatterName, user.chatterNameSuffix);
        // inform the user of the change...
        char nicknotice[512] = "";
        _snprintf_s(nicknotice, sizeof(nicknotice), -1, "|15* |08(|14Notice|08) |07The MRC server has updated your name to |15%s|07.", user.chatterName);
        queueIncomingMessage(nicknotice, true);
        stripPipeCodes(nicknotice);
        writeToLog(nicknotice, PROGRAM, od_control.user_handle);
    }
    else if (strcmp(cmd, "TERMINATE") == 0) {
        // TERMINATE: Server requests the client interface to terminate.[NEW in 1.3]
        // Client must enforce
        // This is to allow for the chat interface to gracefully terminate the connection.
        gIsInChat = false;
        displayMessage(params, true); // should be displayed immediately
        writeToLog("User was terminated by the server", PROGRAM, od_control.user_handle);
        writeToLog(params, PROGRAM, od_control.user_handle);
    }
    else if (strcmp(cmd, "GOODBYE") == 0) {
        // GOODBYE : Server is gracefully closing connection. [NEW in 1.4]
        // Sent only if client reports the CAPABILITY
        gIsInChat = false;
        displayMessage("Server is closing the connection.", true); // should be displayed immediately
        writeToLog("Server is closing the connection.", PROGRAM, od_control.user_handle);
    }
    else if (strcmp(cmd, "USERLIST") == 0) {
		char** newChatters;
		int newCount = split(params, ',', &newChatters);

		EnterCriticalSection(&gChattersLock);
		char** oldChatters = gChattersInRoom;
		int oldCount = gChatterCount;
		gChattersInRoom = newChatters;
		gChatterCount = newCount;
		LeaveCriticalSection(&gChattersLock);

		freeSplitResult(oldChatters, oldCount);
		gUserCountChanged = true;
    }
    else if (strcmp(cmd, "LATENCY") == 0) {
        strcpy_s(gLatency, sizeof(gLatency), params);
        gLatencyChanged = true;
    }
    else if (strcmp(cmd, "RECONNECT") == 0) {
        queueIncomingMessage("|15* |08(|14Notice|08) |07MRC host connection re-established!", false);
        // Rejoin the room if re-connecting to the host
        sendCmdPacket(&mrcSock, "IAMHERE", "");
        sendCmdPacket(&mrcSock, "NEWROOM::", gRoom);
        writeToLog("MRC host connection re-established", PROGRAM, od_control.user_handle);
    }
    // Just display the whole incoming server message if it's not a recognized command string,
    // since it's most likely an informational message from the SERVER.
    else if (strlen(toUser) == 0 || _stricmp(toUser, user.chatterName) == 0) {
        queueIncomingMessage(body, false);
    }
    //od_sleep(5);
}

void processCtcpCommand(char* body, char* toUser, char* fromUser) {

    if (strncmp(body, "[CTCP] ", 7) == 0 && (_stricmp(toUser, user.chatterName) == 0 || strlen(toUser) == 0))
    {
        // body is expected to look like:
        //   "[CTCP] <senderName> <target> <cmd> [args...]"
        // (see how "ctcp" the slash-command builds ctcp_data as
        // "target cmd..." and sendCtcpPacket wraps that as
        // "p senderName data" -- so two tokens, sender name and target,
        // are embedded before the actual command).
        //
        // The previous version located <cmd> by adding strlen(fromUser) and
        // strlen(toUser) -- lengths of separate packet fields -- to a fixed
        // offset, assuming they matched the text actually embedded in body.
        // Nothing guarantees that, so a body that doesn't match the packet's
        // fromUser/toUser fields (malformed, or simply a different/older
        // client) could push that offset past the end of body, and
        // strcpy_s() would then start reading from wherever that landed.
        //
        // Instead, find <cmd>'s start by walking body's own content: skip
        // the literal "[CTCP] " prefix, then skip past the next two
        // space-delimited tokens (sender name, then target).
        char* cursor = body + 7;
        for (int i = 0; i < 2; i++) {
            cursor = strchr(cursor, ' ');
            if (cursor == NULL || *(cursor + 1) == '\0') {
                return; // malformed CTCP command -- no command portion present
            }
            cursor += 1;
        }
        char* cmdStart = cursor;

        char cmdStr[80] = "";
        char repStr[80] = "";

        strcpy_s(cmdStr, sizeof(cmdStr), cmdStart);

        if (_strnicmp(cmdStr, "VERSION", 7) == 0) {
            _snprintf_s(repStr, sizeof(repStr), -1, "VERSION %s(%c) v%s.%s %s [%s]", TITLE, tolower(PLATFORM[0]), PROTOCOL_VERSION, UMRC_VERSION, COMPILE_DATE, AUTHOR_INITIALS);
        }
        else if (_strnicmp(cmdStr, "TIME", 4) == 0) {
            _snprintf_s(repStr, sizeof(repStr), -1, "TIME %s ", getCtcpDatetime());
        }
        else if (_strnicmp(cmdStr, "PING", 4) == 0) {
            // "PING" alone (4 chars, no trailing argument) is valid input --
            // cmdStr + 5 would read past the end of the string in that case.
            const char* pingArg = (strlen(cmdStr) > 5) ? cmdStr + 5 : "";
            _snprintf_s(repStr, sizeof(repStr), -1, "PING %s", pingArg);
        }
        else if (_strnicmp(cmdStr, "CLIENTINFO", 10) == 0) {
            _snprintf_s(repStr, sizeof(repStr), -1, "CLIENTINFO VERSION TIME PING CLIENTINFO");
        }
        else {
            _snprintf_s(repStr, sizeof(repStr), -1, "Unsupported CTCP command: '%s'", cmdStr);
        }
        sendCtcpPacket(&mrcSock, fromUser, "[CTCP-REPLY]", repStr);
    }
    else if (strncmp(body, "[CTCP-REPLY] ", 13) == 0 && _stricmp(toUser, user.chatterName) == 0) {
        // Same fix applied here: locate the reply text by finding the space
        // after the embedded sender name in body, rather than computing
        // "14 + strlen(fromUser)" and trusting it lines up.
        char* replyText = strchr(body + 13, ' ');
        char resp[200] = ""; // needs to be long enough in case of lengthy responses
        strcat_s(resp, sizeof(resp), "* |14[CTCP-REPLY] |10");
        strcat_s(resp, sizeof(resp), fromUser);
        strcat_s(resp, sizeof(resp), " |15");
        if (replyText != NULL && *(replyText + 1) != '\0') {
            strcat_s(resp, sizeof(resp), replyText + 1);
        }
        queueIncomingMessage(resp, false);
        od_sleep(20);
    }
}

#if defined(WIN32) || defined(_MSC_VER)    
DWORD WINAPI handleIncomingMessages(LPVOID lpArg) {
#else
void* handleIncomingMessages(void* lpArg) {
#endif
    int iResult = 0;
    time_t lastIamHere;
    time(&lastIamHere);

    while (gIsInChat) {

        time_t curtime;
        time(&curtime);
        if (curtime - lastIamHere >= 60) {
            sendCmdPacket(&mrcSock, "IAMHERE:", curtime - gLastActTm > 600 ? "AWAY" : "ACTIVE");
            lastIamHere = curtime;
        }

        char inboundData[DATA_LEN] = "";

        // Reserve the last byte for a guaranteed NUL terminator -- recv() does
        // not NUL-terminate, and a full-size read previously left no room for
        // one, so downstream split()/strlen() calls could read past the end
        // of this buffer.
        while ((iResult = recv(mrcSock, inboundData, DATA_LEN - 1, 0)) != 0 && gIsInChat) { // continue till disconnected       
            if (iResult == -1) {
                if (WSAGetLastError() == WSAEMSGSIZE) { // server has more data to send than the buffer can get in one call                   
                    continue; // iterate again to get more data
                }
                else {
                    break;
                }
            }
            else {
                break;
            }
        }
        if (iResult > 0) {
            inboundData[iResult] = '\0';
        }

        if (!gIsInChat) { // user left chat.. none of the below needs to be executed
            break;
        }

        if (iResult > 0) {

            char** packets;
            int pktCount = split(inboundData, '\n', &packets);
            for (int i = 0; i < pktCount; i++) {

                char packet[PACKET_LEN] = "";
                _snprintf_s(packet, PACKET_LEN, -1, "%s", packets[i]);

                if (strstr(packet, "~") == NULL) { // Anything that doesn't contain a tilde 
                    continue;                      // isn't a valid packet, so skip it.
                }

                char fromUser[PACKET_FLD_LEN] = "", fromSite[PACKET_FLD_LEN] = "", fromRoom[PACKET_FLD_LEN] = "", toUser[PACKET_FLD_LEN] = "", msgExt[PACKET_FLD_LEN] = "", toRoom[PACKET_FLD_LEN] = "", body[PACKET_LEN] = "";
                processPacket(packet, fromUser, fromSite, fromRoom, toUser, msgExt, toRoom, body);

                if (strcmp(fromUser, "SERVER") == 0 && (strcmp(toRoom, gRoom) == 0 || strlen(toRoom) == 0)) {

                    processServerMessage(body, toUser);

                    // Check whether the incoming server command should
                    // terminate the session, breaking the loop.
                    if (!gIsInChat) {
                        break;
                    }
                                        
                    // refresh the user list when the SERVER announces joins and exits
                    if (strstr(body, "Joining") != NULL ||
                        strstr(body, "Leaving") != NULL ||
                        strstr(body, "Timeout") != NULL ||
                        strstr(body, "Rename") != NULL || 
                        strstr(body, "Linked") != NULL ||
                        strstr(body, "Unlink") != NULL) {

                        sendCmdPacket(&mrcSock, "USERLIST", "");
                    }

                    // should display SERVER messages to NOTME, if they're to or from the same room
                    if (strcmp(toUser, "NOTME") == 0) {
                        if ((_stricmp(fromRoom, gRoom) == 0 || strlen(fromRoom) == 0) &&
                            (_stricmp(toRoom, gRoom) == 0 || strlen(toRoom) == 0)) {
                            queueIncomingMessage(body, false);
                        }
                    }
                }                
                else if (strcmp(toRoom, CTCP_ROOM) == 0 && strcmp(fromRoom, CTCP_ROOM) == 0) {
                    processCtcpCommand(body, toUser, fromUser);
                }
                else if (strcmp(toUser, "NOTME") == 0) {
                    if ((_stricmp(fromRoom, gRoom) == 0 || strlen(fromRoom) == 0) && 
                        (_stricmp(toRoom, gRoom) == 0 || strlen(toRoom) == 0)) {
                        queueIncomingMessage(body, false);
                        sendCmdPacket(&mrcSock, "USERLIST", "");
                    }
                }
                else if (
                        // messages addressed to the room or to the chatter
                        ((strcmp(gRoom, toRoom)==0 || strlen(toRoom)==0) && (strlen(toUser) == 0 || _stricmp(toUser, user.chatterName) == 0)) || 
                        // broadcast: messages addressed to everyone; no room and no user specified
                        (strlen(toRoom) == 0 && strlen(toUser) == 0)) {

                    if (checkTwit(fromUser)) {
                        continue;
                    }

                    // Direct message (DirectMsg)
                    else if (strlen(toUser) != 0 && strlen(fromUser) != 0) {
                        gMentionCount = gMentionCount + 1;
                        gMentionCountChanged = true;
                        if (user.chatSounds) {
                            od_putch(7);
                        }
                        queueIncomingMessage(body, true);
                        strcpy_s(gLastDirectMsgFrom, sizeof(gLastDirectMsgFrom), fromUser);
                    }

                    // Standard message
                    else {
                        if (_stricmp(fromUser, user.chatterName) != 0 && strContainsStrI(body, user.chatterName)) {
                            gMentionCount = gMentionCount + 1;
                            gMentionCountChanged = true;
                            if (user.chatSounds) {
                                od_putch(7);
                            }
                        }
                        queueIncomingMessage(body, gMentionCountChanged);
                    }
                }

                od_sleep(0);
            }
        }
        else if (iResult == 0) {
            displayMessage("|12* |14Connection closed", true);
            writeToLog("Connection closed", PROGRAM, od_control.user_handle);
            gIsInChat = false;
        }
        else {
            char errstr[50] = "";
            _snprintf_s(errstr, sizeof(errstr), -1, "|12* |14recv failed with error: %d", WSAGetLastError());
            displayMessage(errstr, true);
            stripPipeCodes(errstr);
            writeToLog(errstr, PROGRAM, od_control.user_handle);
            gIsInChat = false;
        }
    }
    return 0;
}

/**
 *  This function handles all chat I/O: getting input from the user as
 *  well as updating the screen.
 */
void doChatRoutines(char* input) {

    bool masking = false;
    char key = ' ';
    tODInputEvent InputEvent;

    while (true) {

        // We can only update the screen from this thread, so we're going to 
        // check for any incoming messages and other changes, and display 
        // them as needed. Then we'll handle user input
        
        Sleep(0);

        if (strlen(mq.message) > 0) {
            displayMessage(mq.message, mq.isMention);
            mq.message[0] = '\0';
            mq.isMention = false;
        }
        if (gRoomTopicChanged) {
            drawStatusBar();
            gRoomTopicChanged = false;
        }
        if (gLatencyChanged) {
            updateLatency();
            gLatencyChanged = false;
        }
        if (gUserCountChanged) {
            updateUserCount();
            gUserCountChanged = false;
        }
        if (gMentionCountChanged) {
            updateMentions();
            gMentionCountChanged = false;
        }
        if (od_get_input(&InputEvent, 1, GETIN_NORMAL) == FALSE) {    
            od_sleep(0);
            continue;
        }

        if (!gIsInChat) { // check the connection status
            doPause();
            break;
        }

        key = ' ';
        bool updateInput = false;
        char pcol[4] = "";
        int endOfInput = 0;
        char tabResult[31] = "";

        if (InputEvent.EventType == EVENT_EXTENDED_KEY) {
            int overfill = 0;
            time(&gLastActTm);
            switch (InputEvent.chKeyPress)
            {

            case OD_KEY_LEFT:
                if (masking) {
                    break;
                }

                user.textColor = user.textColor - 1;
                if (user.textColor < 1) {
                    user.textColor = 15;
                }
                if ((int)strlen(input) >= (int)od_control.user_screenwidth - 3) {
                    overfill = ((int)strlen(input)) - ((int)od_control.user_screenwidth - 4);
                }
                _snprintf_s(pcol, sizeof(pcol), -1, "|%02d", user.textColor);

                // Scroll the input string display if it's longer than the terminal width
                od_set_cursor(od_control.user_screen_length, 1);
                dispEmuPipe(pcol, TRUE);
                if (overfill > 0) {
                    od_disp_str(input + overfill - 1);
                }
                else {
                    od_disp_str(input);
                }
                od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
                break;

            case OD_KEY_RIGHT:
                if (masking) {
                    break;
                }
                user.textColor = user.textColor + 1;
                if (user.textColor > 15) {
                    user.textColor = 1;
                }

                if ((int)strlen(input) >= (int)od_control.user_screenwidth - 3) {
                    overfill = ((int)strlen(input)) - ((int)od_control.user_screenwidth - 4);
                }
                _snprintf_s(pcol, sizeof(pcol), -1, "|%02d", user.textColor);

                // Scroll the input string display if it's longer than the terminal width
                od_set_cursor(od_control.user_screen_length, 1);
                dispEmuPipe(pcol, TRUE);
                if (overfill > 0) {
                    od_disp_str(input + overfill - 1);
                }
                else {
                    od_disp_str(input);
                }
                od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
                break;

            case OD_KEY_DELETE: 
                // Treat the same as Backspace
                updateInput = true;
                if (strlen(input) > 0) {
                    input[strlen(input) - 1] = '\0';
                    endOfInput = endOfInput + 1;
                }
                key = 8;
                break;

            case OD_KEY_END:
                strcpy_s(input, MSG_LEN, "");
                resetInputLine();
                masking = false;
                od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
                break;

            case '\x0e':
            case OD_KEY_PGUP:
                od_set_cursor(1, 1);
                enterScrollBack(od_control.user_screen_length - 3, 0);
                // If there was something in the input buffer before entering scrollback, 
                // put it back in the input buffer. Should only be needed when pressing the
                // hotkey.
                if (strlen(input) > 0) {
                    // Scroll the input string display if it's longer than the terminal width
                    if ((int)strlen(input) >= (int)od_control.user_screenwidth - 3) {
                        overfill = ((int)strlen(input)) - ((int)od_control.user_screenwidth - 4);
                    }              
                    od_set_cursor(od_control.user_screen_length, 1);
                    _snprintf_s(pcol, sizeof(pcol), -1, "|%02d", user.textColor);
                    dispEmuPipe(pcol, TRUE);
                    if (overfill > 0) {
                        od_disp_str(input + overfill - 1);
                    }
                    else {
                        od_disp_str(input);
                    }
                    od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
                    updateBuffer((int)strlen(input));
                }
                break;

            case OD_KEY_UP:
                od_set_cursor(1, 1);
                enterScrollBack(0, 1);
                // If there was something in the input buffer before entering scrollback, 
                // put it back in the input buffer. Should only be needed when pressing the
                // hotkey.
                if (strlen(input) > 0) {
                    // Scroll the input string display if it's longer than the terminal width
                    if ((int)strlen(input) >= (int)od_control.user_screenwidth - 3) {
                        overfill = ((int)strlen(input)) - ((int)od_control.user_screenwidth - 4);
                    }                 
                    od_set_cursor(od_control.user_screen_length, 1);
                    _snprintf_s(pcol, sizeof(pcol), -1, "|%02d", user.textColor);
                    dispEmuPipe(pcol, TRUE);
                    if (overfill > 0) {
                        od_disp_str(input + overfill - 1);
                    }
                    else {
                        od_disp_str(input);
                    }
                    od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
                    updateBuffer((int)strlen(input));
                }
                gMentionCount = 0;
                updateMentions();
                break;
            }
        }
        else if (InputEvent.EventType == EVENT_CHARACTER) {
            key = InputEvent.chKeyPress;
            updateInput = true;
            time(&gLastActTm);

            // Add the keystroke to the input string...            
            if (key == 13 || key == 10) {
                resetInputLine();
                masking = false;
                break;
            }
            else if (key >= 32 && key <= 125) { // allowed characters
                if (strlen(input) <= MSG_LEN) {
                    char tmpipt[MSG_LEN] = "";
                    _snprintf_s(tmpipt, MSG_LEN, -1, "%s%c", input, key);
                    strcpy_s(input, MSG_LEN, tmpipt);
                }
            }
            else if (key == 8) { // backspace
                if (strlen(input) > 0) {
                    input[strlen(input) - 1] = '\0';
                    endOfInput = endOfInput + 1;
                }
            }
            else if (key == 27) { // ESC
                strcpy_s(input, MSG_LEN, "");
                resetInputLine();
                masking = false;
                od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
                break;
            }
            else if (key == 9) {   // TAB - chatter name completion
                if (strlen(input) == 0) {
                    continue; // do nothing if there's no input
                }

                int indexOfTabSearch = -1;
                char tabSearch[140] = "";
                // capture the typed portion after the last space or the beginning of the string,
                // and use that as the user search string.
                for (int i = (int)strlen(input); i >= 0 && input[i] != ' '; i--) {
                    if (input[i] != ' ') {
                        indexOfTabSearch = i;
                    }
                }
                strcpy_s(tabSearch, sizeof(tabSearch), input + indexOfTabSearch);

				EnterCriticalSection(&gChattersLock);
                for (int i = 0; i < gChatterCount; i++) {
                    if (_strnicmp(tabSearch, gChattersInRoom[i], strlen(tabSearch)) == 0 && _stricmp(gChattersInRoom[i], user.chatterName) != 0) {
                        strcpy_s(tabResult, sizeof(tabResult), gChattersInRoom[i]);
                        input[strlen(input) - strlen(tabSearch)] = '\0';
                        strcat_s(input, MSG_LEN, tabResult);
                        endOfInput = endOfInput + 1;
                        break;
                    }
                }
				LeaveCriticalSection(&gChattersLock);
                if (strlen(tabResult) == 0) { // if no match, just skip
                    continue;
                }
            }
            else {
                continue;
            }
        } // Done capturing input...

        if (updateInput) { // Update the input display...

            int overfill = 0;
            if ((int)strlen(input) < (int)od_control.user_screenwidth - 3) {
                endOfInput += ((int)strlen(input)) - ((int)strlen(tabResult));
            }
            else {
                // determine whether the input string is longer than the terminal width,
                // so it can be displaying appropriately below
                overfill = ((int)strlen(input)) - ((int)od_control.user_screenwidth - 4);
                endOfInput = ((int)od_control.user_screenwidth) - 1 + (key == 8 ? -1 : -2) - (((int)strlen(tabResult)) > 0 ? ((int)strlen(tabResult))-1: 0);
            }
            _snprintf_s(pcol, sizeof(pcol), -1, "|%02d", user.textColor);

            // Scroll the input string display if it's longer than the terminal width
            if (overfill > 0) {
                od_set_cursor(od_control.user_screen_length, 1);
                dispEmuPipe(pcol, TRUE);
                od_disp_str(input + overfill - 1);
            }
            od_set_cursor(od_control.user_screen_length, endOfInput);
            dispEmuPipe(pcol, TRUE);

            if (key == 8) {              // Type the backspace...
                if (strlen(input) > 0) {
                    dispEmuPipe("|08|16", TRUE);
                    od_disp_str(" \b.\b"); // ugghh... I hate this, but it works.
                }
                else {
                    od_set_cursor(od_control.user_screen_length, 1);
                }
            }
            else if (key == 9) {        // Type the rest of the tabbed username...
                od_disp_str(tabResult);
            }
            else if (key >= 32) {       // Just type the character that was typed...

                // mask displayed input if input string matches the following inputs:
                if ((((_strnicmp(input, "/identify ", 10) == 0 ||
                       _strnicmp(input, "/roompass ", 10) == 0 ||
                       _strnicmp(input, "/update password ", 17) == 0 ||
                       _strnicmp(input, "/roomconfig password ", 21) == 0)) ||
                      (_strnicmp(input, "/register ", 10) == 0 && countOfChars(input, ' ') < 2)) && key != 32) {
                    masking = true;
                    od_putch('*'); 
                }
                else {            // Any other character simply gets displayed as typed...
                    masking = false;
                    od_putch(key);
                }
            }
            od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);
            updateBuffer((int)strlen(input));
        }
    }
}

bool enterChat() {
        
    if (strlen(user.theme) == 0) {
        strcpy_s(user.theme, sizeof(user.theme), "default.ans");
    }
    loadTheme();  
    loadTwits();

    od_disp_emu("\x1b[?25l", TRUE); // disable the blinking cursor.. don't need it since we're going to make our own..
    od_clr_scr();    

    // get the sysop name from config in case the one OpenDoors pulls from the dropfile is "Sysop" or blank.
    char sysopName[50] = "";
    strcpy_s(sysopName, sizeof(sysopName), (_stricmp(od_control.sysop_name, "sysop")==0 || strlen(od_control.sysop_name)==0) ? cfg.sys : od_control.sysop_name);
    stripPipeCodes(sysopName);
    removeNonAlphanumeric(sysopName); // make sure the name only contains alphanumeric characters, and no silliness

    strcpy_s(gRoom, sizeof(gRoom), user.defaultRoom);

    gScrollBack = malloc(50);
    strcpy_s(gScrollBack, 50, "|15 * * * TOP OF SCROLLBACK * * * |07\n\n\n\n\n");

    gMentions = malloc(50);
    strcpy_s(gMentions, 50, ""); // |15 * * * TOP OF MENTIONS * * * |07\n

    int iResult;
    struct addrinfo* mhResult = NULL, * ptrMh = NULL, mrcHost;

    drawStatusBar();
    displayMessage("Starting up...", false);
#if defined(WIN32) || defined(_MSC_VER)    
    WSADATA wsaData;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        displayMessage("WSAStartup failed", false);
        writeToLog("WSAStartup failed", PROGRAM, od_control.user_handle);
        doPause();
        return false;
    } 
    ZeroMemory(&mrcHost, sizeof(mrcHost));
#else
    memset(&mrcHost, 0, sizeof mrcHost);
#endif
    mrcHost.ai_family = AF_UNSPEC;
    mrcHost.ai_socktype = SOCK_STREAM;
    mrcHost.ai_protocol = IPPROTO_TCP;
    iResult = getaddrinfo("localhost", cfg.port, &mrcHost, &mhResult);
    if (iResult != 0) {
        displayMessage("getaddrinfo failed", false);
        writeToLog("getaddrinfo failed", PROGRAM, od_control.user_handle);
#if defined(WIN32) || defined(_MSC_VER)  
        WSACleanup();
#endif
        doPause();
        return false;
    }

    for (ptrMh = mhResult; ptrMh != NULL; ptrMh = ptrMh->ai_next) {
        mrcSock = socket(ptrMh->ai_family, ptrMh->ai_socktype, ptrMh->ai_protocol);
        if (mrcSock == INVALID_SOCKET) {
#if defined(WIN32) || defined(_MSC_VER)  
            WSACleanup();
#endif
            doPause();
            return false;
        }
        iResult = connect(mrcSock, ptrMh->ai_addr, (int)ptrMh->ai_addrlen);
        if (iResult == SOCKET_ERROR) {
            closesocket(mrcSock);
            mrcSock = INVALID_SOCKET;
            continue;
        }
        break;
    }
    freeaddrinfo(mhResult);

    if (mrcSock == INVALID_SOCKET) {
        displayMessage("Unable to connect to bridge", false);
        writeToLog("Unable to connect to bridge", PROGRAM, od_control.user_handle);
#if defined(WIN32) || defined(_MSC_VER)  
        WSACleanup();
#endif
        doPause();
        return false;
    }

    if (sendCmdPacket(&mrcSock, "IAMHERE", "")) {
        gIsInChat = true;
        od_sleep(20);
    }

    // Client connection to the Bridge has been established a this point.
    //
    // 2 threads:
    //
    //  - Incoming one to listen to messages from the Bridge, and place them in the chat window.
#if defined(WIN32) || defined(_MSC_VER)   
    DWORD incomingThreadID;
    HANDLE hIncoming = CreateThread(NULL, 0, handleIncomingMessages, NULL, 0, &incomingThreadID);
#else
	pthread_t incomingThreadID;
    pthread_create(&incomingThreadID, NULL, handleIncomingMessages, NULL);
#endif   

    time(&gLastActTm);

    // Send some initial packets to the server...
    //
    sendCmdPacket(&mrcSock, "motd", "");
    od_sleep(20);

    char termsize[15] = "";
    _snprintf_s(termsize, sizeof(termsize), -1, "%dx%d", od_control.user_screenwidth, od_control.user_screen_length);
    sendCmdPacket(&mrcSock, "TERMSIZE:", termsize);    
    od_sleep(20);

    if (strlen(sysopName) > 0 && od_control.user_security > 0) {
        char bbsmeta[100] = "";
        _snprintf_s(bbsmeta, sizeof(bbsmeta), -1, " SecLevel(%d) SysOp(%s)", od_control.user_security, sysopName);
        sendCmdPacket(&mrcSock, "BBSMETA:", bbsmeta);
        od_sleep(20);
    }

    if (strlen(gUserIP) > 0 && strcmp(gUserIP, "127.0.0.1") != 0) {
        sendCmdPacket(&mrcSock, "USERIP:", gUserIP);
        od_sleep(20);
    }

    // Announce user and place user into room after initial packets
    //
    sendMsgPacket(&mrcSock, "NOTME", "", "", user.joinMessage);
    od_sleep(20);
    sendCmdPacket(&mrcSock, "NEWROOM::", gRoom);
    od_sleep(20);

    // Loop to get input from the user, and send it over the Bridge.
    while (gIsInChat) {

        char input[MSG_LEN] = "";
        updateBuffer(0);
        resetInputLine();
        od_printf(CHAT_CURSOR, CURSOR_COLORS[user.textColor]);

        doChatRoutines(input); // capture user input and keep chat display updated

        if (strlen(input) == 0) { // do nothing on blank entry
            continue;
        }

        if (input[0] == '/') {
            char cmd[15] = "";
            char params[130] = "";
            int spcidx = indexOfChar(input, ' ');

            if (spcidx > 0) {
                getSubStr(input, cmd, 1, spcidx - 1);
                getSubStr(input, params, spcidx+1, (int)strlen(input));
            }
            else {
                strncpy_s(cmd, sizeof(cmd), input + 1, 14);
            }
            processUserCommand(cmd, params);
        }
        else {
            char msg[PACKET_LEN] = "";
            _snprintf_s(msg, PACKET_LEN, -1, "%s |%02d%s", gDisplayChatterName, user.textColor, input);
            sendMsgPacket(&mrcSock, "", "", gRoom, msg);
        }
    }
    displayMessage("Exiting...", false);

    // cleanup
    shutdown(mrcSock, SD_SEND);
    closesocket(mrcSock);
#if defined(WIN32) || defined(_MSC_VER)  
    WSACleanup();
#endif

    free(gScrollBack);
    free(gMentions);
    free(gTwits);
    gMentionCount=0;
    strcpy_s(gRoom, sizeof(gRoom), "");
    strcpy_s(gTopic, sizeof(gTopic), "");

    od_disp_emu("\x1b[?25h", TRUE); // re-enable the blinking cursor..

    return true;
}

void displayTimeWarning(char* str) {
    if (gIsInChat) {
        displayMessage(strReplace(strReplace(str, "\r", ""), "\n", ""), false);
    }
    else {
        od_set_cursor(15, 29);
        od_printf(str);
    }
}

#if defined(WIN32) || defined(_MSC_VER)
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpszCmdLine, int nCmdShow)
{
    char** argv;
    int argc = split(lpszCmdLine, ' ', &argv);
#else
int main(int argc, char** argv)
{
#endif

    bool exit = false;
    int userNumber = -1;

#if defined WIN32
    od_parse_cmd_line(lpszCmdLine);
#else
    od_parse_cmd_line(argc, argv);
#endif

    for (int i = 0; i < argc; i++) {
        if (_strnicmp(argv[i], "-IP", 3) == 0) {
            strcpy_s(gUserIP, sizeof(gUserIP), argv[i] + 3);
        }
    }

    od_control.od_page_pausing = false;
    od_control.od_disable |= DIS_NAME_PROMPT; // Disable the local user name prompt; it doesn't work.. always results in "Sysop"
    od_control.od_time_msg_func = displayTimeWarning;
    strcpy_s(od_control.od_prog_name, sizeof(od_control.od_prog_name), TITLE);
    _snprintf_s(od_control.od_prog_version, sizeof(od_control.od_prog_version), -1, "v%s", UMRC_VERSION);
    strcpy_s(od_control.od_prog_copyright, sizeof(od_control.od_prog_copyright), YEAR_AND_AUTHOR);

#if defined(WIN32) || defined(_MSC_VER)
    HICON hIcon = (HICON)LoadImage(NULL, "icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
    if (hIcon != NULL) {
        od_control.od_app_icon = hIcon;
    }
    InitializeCriticalSection(&gChattersLock);
#endif

    od_init();

    if (loadData(&cfg, CONFIG_FILE) != 0) {
        od_printf("Invalid config. Run setup.\r\n");
        od_exit(-1, FALSE);
    }

    od_control.od_inactivity = 0;     
    od_control.od_maxtime = 0;           
    od_control.user_ansi = TRUE;
    od_control.user_screen_length = 24;

    od_clr_scr();

    userNumber = od_control.user_num;
    if (0 == userNumber && strcmp(od_control.user_name, "Sysop") == 0) {
        od_printf("`bright yellow`***`bright white`LOCAL MODE`bright yellow`***``\r\n\r\n");
        strcpy_s(user.chatterName, sizeof(user.chatterName), od_control.user_name);
        od_control.user_timelimit = 1000;
    }
    else {
        strcpy_s(user.chatterName, sizeof(user.chatterName), strlen(od_control.user_handle) > 0 ? od_control.user_handle : od_control.user_name);
    }
    strcpy_s(user.chatterName, sizeof(user.chatterName), strReplace(user.chatterName, " ", "_")); // spaces are disallowed; replace with underscores
    strcpy_s(user.chatterName, sizeof(user.chatterName), strReplace(user.chatterName, "~", ""));  // tildes are disallowed; strip them out
    strcpy_s(gFromSite, sizeof(gFromSite), strReplace(cfg.name, "~", ""));
    stripPipeCodes(gFromSite);
    // Spaces must be replaced by _[underscore / chr(95)] when sent to server
    for (int i = 0; i < (int)strlen(gFromSite); i++) {
        if (gFromSite[i] == ' ') {
            gFromSite[i] = '_';
        }
    }
    if (strlen(gFromSite) > 30) {
        gFromSite[30] = '\0';
    }
    char userFileName[36] = "";
    strcpy_s(userFileName, sizeof(userFileName), user.chatterName);
    cleanUpFilename(userFileName); 
    _snprintf_s(gUserDataFile, sizeof(gUserDataFile), -1, "%s%c%s.dat", USER_DATA_DIR, PATH_SEP, userFileName);
    _snprintf_s(gTwitFile, sizeof(gTwitFile), -1, "%s%c%s.twit", USER_DATA_DIR, PATH_SEP, userFileName);

#if defined(WIN32) || defined(_MSC_VER)  
    DWORD attributes = GetFileAttributesA(USER_DATA_DIR);
    if (!(attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))) {
        CreateDirectory(USER_DATA_DIR,NULL);
    }
#else
    struct stat st ={0};
    if (stat(USER_DATA_DIR, &st)==-1) {
        mkdir(USER_DATA_DIR, 0700);
    }
#endif

    if (loadUser(&user, gUserDataFile) != 0) {

        // set some defaults for the new user
        user.userNumber = userNumber;
        user.chatSounds = false;                               
        user.textColor = 7;
        user.chatterNamePrefixFgColor = 8;
        user.chatterNamePrefixBgColor = 16;
        user.chatterNameFgColor = 7;
        user.chatterNameBgColor = 16;
        strcpy_s(user.defaultRoom, sizeof(user.defaultRoom), DEFAULT_ROOM);
        stripPipeCodes(user.defaultRoom);

        // Reserved words
        // Usernames: SERVER, CLIENT and NOTME must be blacklisted 
        // from being used as BBS users in the client
        if (_stricmp(user.chatterName, "SERVER") == 0 ||
            _stricmp(user.chatterName, "CLIENT") == 0 ||
            _stricmp(user.chatterName, "NOTME") == 0) {
            // replace it with a generic SAFE name
            _snprintf_s(user.chatterName, sizeof(user.chatterName), -1, "User_%d", user.userNumber);
        }

        od_printf("`bright white`Welcome to %s!``\r\n", TITLE);
        od_printf(DIVIDER);
        od_printf("Looks like you're new here, %s.\r\n\r\n\r\n", user.chatterName);
        od_printf("``First things first...\r\n\r\nIs \"`bright white`%s``\" the name you want to use in chat? (Y/N) > ", user.chatterName);
        if (od_get_answer("YN") == 'N') {
            bool namevalid = false;
            char newchatname[36] = "";
            od_printf("``\r\n\r\nOK, what shall your chat name be?\r\n`bright black`Don't use pipe codes or anything here. We'll cover that shortly.`` \r\n\r\n> ");
            do {
                getInputString(newchatname, 36, 32, 127);
                if (strchr(newchatname, ' ') != NULL || strchr(newchatname, '~') != NULL) {
                    strcpy_s(newchatname, sizeof(newchatname), strReplace(newchatname, " ", "_"));
                    strcpy_s(newchatname, sizeof(newchatname), strReplace(newchatname, "~", ""));
                }

                if (strstr(newchatname, "|") != NULL) {
                    od_printf("``\r\n\r\nAs I said, don't get cute with pipe codes.\r\nJust use alphas and/or numerics here.`` \r\n\r\n> ");
                    namevalid = false;
                }
                else if (strlen(newchatname) != 0) { 
                    namevalid = true;
                }

                if (namevalid == true) {
                    od_printf("\r\n\r\nBe known as %s? (Y/N) > ", newchatname);
                    if (od_get_answer("YN") == 'N') {
                        namevalid = false;
                        od_printf("``\r\n\r\nOK, try again:`` \r\n\r\n> ");
                    }
                }

            } while (!namevalid);

            strcpy_s(user.chatterName, sizeof(user.chatterName), newchatname);
            od_printf("`bright white`\r\n\r\nOK, you will be known as `bright green`%s`bright white` while in chat!``\r\n\r\n", user.chatterName);
        }
        else {
            od_printf("`bright white`\r\n\r\nOK, we'll leave it as-is!``\r\n\r\n");
        }

        od_printf("``If you ever change your mind, your sysop will need to `bright white`reset`` \r\nyour uMRC settings.`` \r\n\r\n");
        defaultDisplayName();
        _snprintf_s(user.joinMessage, sizeof(user.joinMessage), -1, DEFAULT_JOIN_MSG, user.chatterName);
        _snprintf_s(user.exitMessage, sizeof(user.exitMessage), -1, DEFAULT_EXIT_MSG, user.chatterName);

        if (saveUser(&user, gUserDataFile) != -1) {

            od_printf("We've gone ahead set some default options for you. You can go ahead and\r\ncustomize them now.\r\n\r\n");
            od_printf(DIVIDER);
            doPause();

            _snprintf_s(gDisplayChatterName, sizeof(gDisplayChatterName), -1, "|%02d|%02d%c|%02d|%02d%s%s", user.chatterNamePrefixFgColor, user.chatterNamePrefixBgColor, user.chatterNamePrefix, user.chatterNameFgColor, user.chatterNameBgColor, user.chatterName, user.chatterNameSuffix);
            enterChatterSettings("main menu");
        }
    }
    else {
        _snprintf_s(gDisplayChatterName, sizeof(gDisplayChatterName), -1, "|%02d|%02d%c|%02d|%02d%s%s", user.chatterNamePrefixFgColor, user.chatterNamePrefixBgColor, user.chatterNamePrefix, user.chatterNameFgColor, user.chatterNameBgColor, user.chatterName, user.chatterNameSuffix);
        od_printf("Existing user data LOADED.\r\n\r\n");
    }

    if (strlen(user.theme) == 0) {
        strcpy_s(user.theme, sizeof(user.theme), "default.ans");
    }
    loadTheme();    

    // Main menu .........................................
    //
    while (!exit) {

        od_clr_scr();
#if defined(WIN32) || defined(_MSC_VER)  
        od_send_file("screens\\intro.ans");
#else
        od_send_file("screens/intro.ans");
#endif
        int act = 0;
        char* bbses="", * rooms = "", * users = "", * activity = "";
        char mrcStTm[30]="";
        FILE* mrcstats;
#if defined(WIN32) || defined(_MSC_VER)  
        fopen_s(&mrcstats, MRC_STATS_FILE, "r");
#else
        mrcstats = fopen(MRC_STATS_FILE, "r");
#endif
        if (mrcstats != NULL) {
            char stats[30] = "";
            fgets(stats, 30, mrcstats);

            char** stat;
            int statCount = split(stats, ' ', &stat);
            if (statCount >= 5) {
                bbses = stat[0];
                rooms = stat[1];
                users = stat[2];
                activity = stat[3];
            }
            act = atoi(activity);
            fclose(mrcstats);
        }

        struct stat file_stat;
        if (stat(MRC_STATS_FILE, &file_stat) == 0) {
#if defined(WIN32) || defined(_MSC_VER)  
            ctime_s(mrcStTm, 30, &file_stat.st_mtime);
            strcpy_s(mrcStTm, sizeof(mrcStTm), strReplace(mrcStTm, "\n", ""));
#else
            strcpy_s(mrcStTm, sizeof(mrcStTm), strReplace(ctime(&file_stat.st_mtime), "\n", ""));
#endif
        }

        od_set_cursor(3, 25);
        od_printf("`bright white`%s for %s ``v%s", "Universal Multi-Relay Chat", PLATFORM, UMRC_VERSION);

        od_set_cursor(5, 25);
        od_printf("`bright black`MRC Host: `magenta`%s", cfg.host);
        od_set_cursor(6, 25);
        od_printf("`bright black`Protocol Version: `magenta`%s", PROTOCOL_VERSION);
        od_set_cursor(6, 65);
        od_printf("%s", cfg.ssl ? "`green`SSL ENABLED``" : "");

        od_set_cursor(9, 25);
        od_printf("`bright white`(`bright magenta`C`bright white`) `white`Enter `bright white`c`white`hat!");
        od_set_cursor(10, 25);
        od_printf("`bright white`(`bright magenta`S`bright white`) `white`Chatter `bright white`s`white`ettings");
        od_set_cursor(11, 25);
        od_printf("`bright white`(`bright magenta`I`bright white`) `white`Read `bright white`I`white`nstructions");
        
        od_set_cursor(13, 25);
        od_printf("`bright white`(`bright green`Q`bright white`) `bright white`Q`white`uit to `bright white`");
        dispEmuPipe(cfg.name, TRUE); // display the bbs name it all its pipe code colorful glory :P

        time_t curtime;
        time(&curtime);

        od_set_cursor(18, 25);
        od_printf("`white`MRC Stats as of `bright green`%s`white`:", mrcStTm);
        od_set_cursor(20, 30);
        od_printf("`bright black`State``: `bright white`%s", curtime - file_stat.st_mtime <= 60 ? "`bright green`ONLINE``" : "`gray`OFFLINE``");
        od_set_cursor(21, 30);
        od_printf("`bright black`BBSes``: `bright white`%s", bbses);
        od_set_cursor(22, 30);
        od_printf("`bright black`Rooms``: `bright white`%s", rooms);
        od_set_cursor(21, 45);
        od_printf("`bright black`   Users``: `bright white`%s", users);
        od_set_cursor(22, 45);
        od_printf("`bright black`Activity``: `bright white`%s", ACTIVITY[act]);
        
        od_set_cursor(14, 29);
        od_printf("`white`Make a selection `bright black`(`bright blue`C`white`,`bright blue`S`white`,`bright blue`I`white`,`bright blue`Q`bright black`)");

        switch (od_get_answer("CSITQ")) {
        case 'C':
            enterChat();
            saveUser(&user, gUserDataFile);
            break;

        case 'S':
            enterChatterSettings("main menu");
            break;

        case 'I':

            od_clr_scr();
#if defined(WIN32) || defined(_MSC_VER)  
            displayPipeFile("screens\\help.txt");
#else
            displayPipeFile("screens/help.txt");
#endif
            doPause();
            break;

        case 'T':
            od_clr_scr();
            od_printf(DIVIDER);
            od_printf("`` ChatterName:          `bright white`%s``", user.chatterName);
            od_printf("\r\n`` DisplayChatterName:   "); dispEmuPipe(gDisplayChatterName, true);
            od_printf("\r\n`` FromSite:             `bright white`%s``", gFromSite);
            od_printf("\r\n`` user_num:             `bright white`%d``", od_control.user_num);
            od_printf("\r\n`` user_name:            `bright white`%s``", od_control.user_name);
            od_printf("\r\n`` UserIP:               `bright white`%s``", gUserIP);
            od_printf("\r\n`` user_handle:          `bright white`%s``", od_control.user_handle);
            od_printf("\r\n`` user_security:        `bright white`%d``", od_control.user_security);
            od_printf("\r\n`` user_timelimit:       `bright white`%d``", od_control.user_timelimit);
            od_printf("\r\n`` user_ansi:            `bright white`%d``", od_control.user_ansi);
            od_printf("\r\n`` user_screen_length:   `bright white`%d``", od_control.user_screen_length);
            od_printf("\r\n`` user_screenwidth:     `bright white`%d``", od_control.user_screenwidth);
            od_printf("\r\n`` sysop_name:           `bright white`%s``", od_control.sysop_name);
            od_printf("\r\n`` sysop_name (cleansed):`bright white`%s``", od_control.sysop_name);
            od_printf("\r\n`` system_name:          `bright white`%s``", od_control.system_name);
            od_printf("\r\n`` od_maxtime:           `bright white`%d``", od_control.od_maxtime);
            od_printf("\r\n`` od_inactivity:        `bright white`%d``", od_control.od_inactivity);

            od_printf("\r\n");
            od_printf(DIVIDER);
            od_printf("This screen is for testing and troubleshooting purposes.\r\n");
            od_printf("Include this screen when posting a GitHub issue.\r\n");

            doPause();
            break;

        case 'Q':
            exit = true;
            break;
        }
    }
    	
    od_clr_scr();
	od_exit(0, FALSE);
}