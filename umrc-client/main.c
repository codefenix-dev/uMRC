/*

 uMRC Client

 This is a Windows door that interfaces with the uMRC Bridge and lets users
 chat with other users in MRC.





 */


#if defined(WIN32) || defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")
#endif

#include <sys/stat.h>
#include <sys/timeb.h>

#include "OpenDoor.h"
#include "../common/common.h"

#define DIVIDER "`bright blue`___________________________________________________________________________\r\n``"
#define DEFAULT_ROOM "lobby"
#define DEFAULT_JOIN_MSG "|11%s |03has arrived."
#define DEFAULT_EXIT_MSG "|12%s |04has left chat|08."
#define CTCP_ROOM "ctcp_echo_channel"
#define CHAT_CURSOR "`flashing white`\262`` "


bool gRoomTopicChanged = false;
bool gLatencyChanged = false;
bool gUserCountChanged = false;
bool gMentionCountChanged = false;

bool gIsInChat = false;
bool isChatPaused = false;

int gMentionCount = 0;
int gChatterCount = 0;
char gDisplayChatterName[80] = "";
char gRoom[30] = "";
char gTopic[55] = "";
char gLatency[6] = "";
char gFromSite[30] = "";
char gLastDirectMsgFrom[36] = "";

char** gChattersInRoom;
char** sTwits;

char* gScrollBack;
char* gMentions;

// dummy theme in case the theme file doesn't load properly
char gStatusThemeLine1[400] = "\325\315\315[                                                                 /help ]\315\315\270";
char gStatusThemeLine2[400] = "\324\315\315[ users:     ]\315\315[ latency:      ]\315\315\315[ mentions:     ]\315[ buffer:         ]\315\315\276";


struct messageCourier {
    char* message;
    bool isMention;
};
struct messageCourier mc;


#define DEFAULT_BRACKETS_COUNT 12
const char* DEFAULT_BRACKETS[DEFAULT_BRACKETS_COUNT] = {
    "<>",
    "{}",
    "[]",
    "@@",
    "!!",
    ":)",
    "//",
    "==",
    "--",
    "%%",
    "^^",
    "##"
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
 *  Returns a time string )HH:MM) to place in front of all chat messages.
 */
char* getTimestamp() {
    char timeStamp[10] = "";
    time_t rawtime;
    struct tm timeinfo;

    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);

    _snprintf_s(timeStamp, 10, -1, "%02d:%02d",
        timeinfo.tm_hour,
        timeinfo.tm_min);

    return _strdup(timeStamp);
}

/**
 *  Returns a date/time string for CTCP responses, formatted as MM/DD/YY HH:MM.
 */
char* getCtcpDatetime() {
    char dtStr[30] = "";
    time_t rawtime;
    struct tm timeinfo;

    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);

    _snprintf_s(dtStr, 30, -1, "%02d/%02d/%02d %02d:%02d",
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_year - 100, // + 1900,
        timeinfo.tm_hour,
        timeinfo.tm_min);

    return _strdup(dtStr);
}

/**
 *  Pauses and waits for any key input from the user.
 */
void pause() {
    od_printf("\r\n `bright white`[`cyan`Press any key to continue`bright white`]`white` ");
    od_get_key(TRUE);
}

/**
 * Checks if a string contains another string. Case-insensitive.
 */
bool strContainsStrI(char* str, char* contains) {
    char s[512] = "";
    char c[512] = "";
    strcpy_s(s, 512, str);
    strcpy_s(c, 512, contains);
    lstr(c);
    lstr(s);
    return strstr(s, c) != NULL;
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
    strncpy_s(brackets, 3, DEFAULT_BRACKETS[rand() % DEFAULT_BRACKETS_COUNT], 3);
    do { 
        user.chatterNamePrefixFgColor = (rand() % 11) + 2;
    } while (user.chatterNamePrefixFgColor == user.chatterNameFgColor);
    suffixColor = user.chatterNamePrefixFgColor;
    user.chatterNamePrefix = brackets[0];

    // This commented-out section sets a suffix string based on the BBS name.
    // People didn't seem to like it, so I settled for a simple 1-character 
    // default suffix instead. Leaving this section commented out in case I
    // decide to ever revisit.
    //
    // form a default suffix string using the board's name, i.e. an abbreviation..
    //for (int i = 0; cfg.name[i] != '\0' && suffixLen <= 3; i++) {
    //    if (buildSuffix == true && tolower(cfg.name[i]) >= 97 && tolower(cfg.name[i]) <= 122) {
    //        _snprintf_s(defaultSuf, 30, -1, "%s%c", defaultSuf, tolower(cfg.name[i]));
    //        suffixLen = suffixLen + 1;
    //        if (strstr(cfg.name, " ") != NULL) {
    //            buildSuffix = false;
    //        }
    //    }
    //    else if (cfg.name[i] == 32) {
    //        buildSuffix = true;
    //    }
    //}
    //_snprintf_s(user.chatterNameSuffix, 33, -1, "%s|%02d%c", defaultSuf, user.chatterNamePrefixFgColor, brackets[1]);
    
    _snprintf_s(user.chatterNameSuffix, 33, -1, "|%02d%c|16", user.chatterNamePrefixFgColor, brackets[1]);
}

/** 
 *  Removes pipe color code sequences (0-24) from a string.
 */
void stripPipeCodes(char* str) {
    char tmpSite[MSG_LEN] = "";
    strncpy_s(tmpSite, MSG_LEN, str, -1);
    for (int i = 0; i < 24; i++) {
        char fndstr[4] = "";
        _snprintf_s(fndstr, 4, -1, "|%02d", i);
        strncpy_s(tmpSite, MSG_LEN, strReplace(tmpSite, fndstr, ""), -1);
        if (strstr(tmpSite, "|") == NULL) {
            break;
        }
    }
    strncpy_s(str, 140, tmpSite, -1);
}

/**
 *  Prompts the user to select a color within a range. 
 */
int colorPrompt(int lo, int hi) {
    bool validEntry = false;
    int pickedColor = -1;

    for (int i = lo; i <= hi; i++) {
        char c[9] = "";
        _snprintf_s(c, 9, -1, i < 17 ? "|%02d%d " : "|00|%02d%d ", i, i);
        od_disp_emu(pipeToAnsi(c), TRUE);
    }
    while (!validEntry) {
        od_printf("``\r\n\r\nPick a color (%d-%d): ", lo, hi);        
        char ci[3];
        od_input_str(ci, 2, '0', '9');
        pickedColor = atoi( ci );
        validEntry = (pickedColor >= lo && pickedColor <= hi);
    }
    return pickedColor;
}

/**
 *  Lets a user customize their chatter display name.
 */
int editDisplayName() {

    bool doneEditing = false;
    int changeCount = 0;
    while (!doneEditing) {

        char newSuf[30] = "";
        char prefixprev[10] = "";
        char nameprev[50] = "";
        _snprintf_s(prefixprev, sizeof(prefixprev), -1, "|%02d|%02d%c", user.chatterNamePrefixFgColor, user.chatterNamePrefixBgColor, user.chatterNamePrefix);
        _snprintf_s(nameprev, sizeof(nameprev), -1, "|%02d%s", user.chatterNameFgColor, user.chatterName);
        _snprintf_s(gDisplayChatterName, sizeof(gDisplayChatterName), -1, "|%02d|%02d%c|%02d|%02d%s%s", user.chatterNamePrefixFgColor, user.chatterNamePrefixBgColor, user.chatterNamePrefix, user.chatterNameFgColor, user.chatterNameBgColor, user.chatterName, user.chatterNameSuffix);

        od_clr_scr();

        od_printf("`bright white`Display Name Settings for `bright cyan`%s``\r\n", user.chatterName);
        od_printf(DIVIDER);
        od_printf("``\r\n");

        od_printf("``Preview: ");
        od_disp_emu(pipeToAnsi(gDisplayChatterName), TRUE);
        od_printf("\r\n");

        od_printf("\r\n `bright magenta`P`bright white`) `white`Edit prefix `bright black`:           ``");
        od_disp_emu(pipeToAnsi(prefixprev), TRUE);
        od_printf("\r\n `bright magenta`C`bright white`) `white`Edit name color `bright black`:       ``");
        od_disp_emu(pipeToAnsi(nameprev), TRUE);
        od_printf("\r\n `bright magenta`S`bright white`) `white`Edit suffix `bright black`:           ``");
        od_disp_emu(pipeToAnsi(user.chatterNameSuffix), TRUE);

        od_printf("``\r\n\r\n");
        od_printf(" `cyan`R`bright white`) `white`Randomize!");
        od_printf("``\r\n\r\n");
        od_printf(" `bright green`Q`bright white`) `white`Quit to Settings Menu");
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
            od_printf(" `bright black`Current string is: `bright white`%s``\r\n\r\n\r\n", user.chatterNameSuffix);
            od_printf(" `bright black`Current string is: `bright white`%s``\r\n\r\n\r\n", user.chatterNameSuffix);
            od_printf("Enter a suffix to add to your username:\r\n\r\n `bright black`(pipe colors allowed)\r\n\r\n `bright white`> ");
            od_input_str(newSuf, 30, 32, 127);
            if (_strcmpi(newSuf, user.chatterNameSuffix) != 0 && strlen(newSuf) != 0) {
                _snprintf_s(user.chatterNameSuffix, 33, -1, "|07|16%s|16", newSuf);
                changeCount = changeCount + 1;
            }
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
    WIN32_FIND_DATA fdFile;
    HANDLE hFind = NULL;

    bool validEntry = false;
    int pickedOption = -1;
    int count = 0;
    char themeFiles[1000] = "";

    if ((hFind = FindFirstFile("themes\\*.ans", &fdFile)) == INVALID_HANDLE_VALUE) {
        printf("Path not found: [%s]\r\n", "themes");
        pause();
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

    char** themeList;
    count = split(themeFiles, '|', &themeList);

    for (int i = 0; i < count; i++) {
        od_printf("``%d`bright black`: `bright white`%s``\r\n", i + 1, themeList[i]);
    }
    while (!validEntry) {
        od_printf("``\r\n\r\nPick a theme (%d-%d): ", 1, count);

        char ci[3];
        od_input_str(ci, 2, '0', '9');
        pickedOption = atoi(ci);
        validEntry = (pickedOption >= 1 && pickedOption <= count);
    }
    strcpy_s(pickedTheme, 20,  themeList[pickedOption - 1]);
}

void loadTheme() {
    char themePath[30] = "";
    _snprintf_s(themePath, 30, -1, "themes\\%s", user.theme);
    FILE* extFile;
    fopen_s(&extFile, themePath, "r");
    int lineCount = 0;
    if (extFile != NULL) {
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
                break;
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
    strncpy_s(displayableTopic, 65 - (strlen(gRoom) + 2), gTopic, /*64 - (strlen(gRoom) + 2)*/ -1);
    od_set_cursor(od_control.user_screen_length - 2, 6); 
    od_printf("`white`#`bright white`%s`bright black`:`bright white`%s ", gRoom, displayableTopic);
}

/**
 *  Updates the number of users in the second line of the status bar
 */
void updateUserCount() {
    od_set_cursor(od_control.user_screen_length - 1, 13);
    od_printf("`bright white`%-3d``", gChatterCount);
}

/**
 *  Updates the Latency in the second line of the status bar.
 */
void updateLatency() {
    od_set_cursor(od_control.user_screen_length - 1, 31);
    od_printf("`bright white`%-4s``", gLatency);
}

/**
 *  Updates the mention counter in the second line of the status bar
 */
void updateMentions() {
    od_set_cursor(od_control.user_screen_length - 1, 52);
    od_printf("`bright white`%-4d``", gMentionCount);
}

/**
 *  Updates the input buffer (000/140) in the second line of the status bar
 */
void updateBuffer(int typed) {
    od_set_cursor(od_control.user_screen_length - 1, 68);
    od_printf(typed > 120 ? "`bright yellow`%03d`white`/`bright white`%03d``" : "`bright white`%03d`white`/`bright white`%03d``", typed, MSG_LEN - 1);
}

void drawStatusBar() {
    char themeLines[1024] = "";
    _snprintf_s(themeLines, 1024, -1, "%s\r\n%s", gStatusThemeLine1, gStatusThemeLine2);
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
void enterChatterSettings(char* userFile) {
    bool changesMade = false;
    bool exit = false;
    char newRoom[30] = "";
    char newJoin[50] = "";
    char newExit[50] = "";
    char pickedTheme[20] = "";

    // failsafe in case a user hasn't picked a theme
    if (user.theme == NULL || strlen(user.theme) == 0) {
        strcpy_s(user.theme, sizeof(user.theme), "default.ans");
    } 
    loadTheme();

    while (!exit) {
        od_clr_scr();

        drawStatusBar();
        od_set_cursor(od_control.user_screen_length - 2, 6);
        od_printf("`white`#`bright white`%s`bright black`:`bright white`%s %s ", "room", "Theme preview of", user.theme);

        od_set_cursor(1, 1);
        od_printf("`bright white`Chatter Settings``\r\n");
        od_printf(DIVIDER);
        od_printf("``\r\n");

        od_printf(" `bright magenta`1`bright white`) `white`Display name:  ");
        od_disp_emu(pipeToAnsi(gDisplayChatterName), TRUE);
        od_printf("``\r\n");

        od_printf(" `bright magenta`2`bright white`) `white`Default room:  `bright white`%s\r\n", user.defaultRoom);

        od_printf(" `bright magenta`3`bright white`) `white`Text color:    ");
        char sampletext[80] = "";
        _snprintf_s(sampletext, 80, -1, "|%02dSample text using color #%d...", user.textColor, user.textColor);
        od_disp_emu(pipeToAnsi(sampletext), TRUE);
        od_printf("``\r\n");

        od_printf(" `bright magenta`4`bright white`) `white`Chat sounds:   `bright white`%s\r\n", user.chatSounds ? "ON" : "OFF");

        od_printf(" `bright magenta`5`bright white`) `white`Enter message: ");
        od_disp_emu(pipeToAnsi(user.joinMessage), TRUE);
        od_printf("``\r\n");

        od_printf(" `bright magenta`6`bright white`) `white`Exit message:  ");
        od_disp_emu(pipeToAnsi(user.exitMessage), TRUE);
        od_printf("``\r\n");

        od_printf(" `bright magenta`7`bright white`) `white`Theme:         `bright white`%s\r\n", user.theme );

        od_printf("``\r\n");
        od_printf(" `bright green`Q`bright white`) `white`Quit to main menu");
        od_printf("``\r\n\r\n> ");

        switch (od_get_answer("1234567Q")) {
        case '1':
            if (editDisplayName() > 0) {
                changesMade = true;
            }
            break;

        case '2':
            od_clr_scr();
            od_printf("`bright white`Default Room``\r\n\r\n");
            od_printf("Enter a new default room (suggestions: `bright green`lobby, `bright magenta` ddial``)\r\n\r\n`bright white`> ");
            od_input_str(newRoom, 30, 32, 127);
            if (_strcmpi(newRoom, user.defaultRoom) != 0) {
                if (strlen(newRoom) == 0) {
                    strncpy_s(newRoom, 30, DEFAULT_ROOM, -1);
                }
                stripPipeCodes(newRoom);
                strncpy_s(user.defaultRoom, 30, strReplace(strReplace(strReplace(newRoom, " ", "_"), "#", ""), "~", ""), -1);
                changesMade = true;
            }
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
            pause();
            break;

        case '5':
            od_clr_scr();
            od_printf("`bright white`Join Message``\r\n\r\n");
            od_printf(" `bright black`Current string is: `bright white`%s``\r\n\r\n\r\n", user.joinMessage);
            od_printf("Enter a message you'd like to announce you when you JOIN chat:\r\n\r\n `bright black`(pipe colors allowed)\r\n\r\n `bright white`> ");
            od_input_str(newJoin, 50, 32, 127);
            if (_strcmpi(newJoin, user.joinMessage) != 0) {
                if (strlen(newJoin) == 0) {
                    _snprintf_s(newJoin, 50, -1, DEFAULT_JOIN_MSG, user.chatterName);
                }
                strncpy_s(user.joinMessage, 50, newJoin, -1);
                changesMade = true;
            }
            break;

        case '6':
            od_clr_scr();
            od_printf("`bright white`Exit Message``\r\n\r\n");
            od_printf(" `bright black`Current string is: `bright white`%s``\r\n\r\n\r\n", user.exitMessage);
            od_printf("Enter a message you'd like to announce you when you EXIT chat:\r\n\r\n `bright black`(pipe colors allowed)\r\n\r\n `bright white`> ");
            od_input_str(newExit, 50, 32, 127);
            if (_strcmpi(newExit, user.exitMessage) != 0) {
                if (strlen(newExit) == 0) {
                    _snprintf_s(newExit, 50, -1, DEFAULT_EXIT_MSG, user.chatterName);
                }
                strncpy_s(user.exitMessage, 50, newExit, -1);
                changesMade = true;
            }
            break;

        case '7':
            od_clr_scr();
            pickTheme(pickedTheme);
            strncpy_s(user.theme, 20, pickedTheme, -1);
            changesMade = true;
            loadTheme();
            break;

        case 'Q':
            exit = true;
            break;
        }
    }
    if (changesMade) {
        saveUser(&user, userFile);
        od_printf("\r\n`bright white`Changes saved!``");
        pause();
    }
}

/**
 *  Sends a command packet to the MRC host
 */
bool sendCmdPacket(SOCKET* sock, char* cmd, char* cmdArg) {
    int iResult;
    char cmdstr[MSG_LEN] = "";
    char packet[PACKET_LEN] = "";
    if (strlen(cmdArg) > 0) {
        _snprintf_s(cmdstr, MSG_LEN, -1, strchr(cmd, ':') ? "%s%s" : "%s %s", cmd, cmdArg);
    }
    else {
        strncpy_s(cmdstr, MSG_LEN, cmd, -1);
    }
    strncpy_s(packet, PACKET_LEN, createPacket(user.chatterName, gFromSite, gRoom, "SERVER", "", "", cmdstr), -1);
    iResult = send(*sock, packet, (int)strlen(packet), 0);
    return (iResult != SOCKET_ERROR);
}

/**
 *  Sends a CTCP command to the MRC host
 */
bool sendCtcpPacket(SOCKET* sock, char* target, char* p, char* data) {
    int iResult;
    char ctcpstr[MSG_LEN] = "";
    char packet[PACKET_LEN] = "";
    _snprintf_s(ctcpstr, MSG_LEN, -1, "%s %s %s", p, user.chatterName, data);
    strncpy_s(packet, PACKET_LEN, createPacket(user.chatterName, gFromSite, CTCP_ROOM, target, "", CTCP_ROOM, ctcpstr), -1);
    iResult = send(*sock, packet, (int)strlen(packet), 0);
    return (iResult != SOCKET_ERROR);
}

/**
 *  Sends a Message packet to the MRC host
 */
bool sendMsgPacket(SOCKET* sock, char* toUser, char* msgExt, char* toRoom, char* body) {
    int iResult;
    char packet[PACKET_LEN] = "";
    strncpy_s(packet, PACKET_LEN, createPacket(user.chatterName, gFromSite, gRoom, toUser, msgExt, toRoom, body), -1);
    iResult = send(*sock, packet, (int)strlen(packet), 0);
    return (iResult != SOCKET_ERROR);
}

/**
 *  Returns the length of a string without pipe color codes.
 */
int strLenWithoutPipecodes(char* str) {
    int len = 0;
    for (int i = 0; i < (int)strlen(str); i++) {
        if (str[i] == '|' && i < ((int)strlen(str) - 2)) { // check the next 2 characters for digits
            if (isdigit(str[i + 1]) && isdigit(str[i + 2])) {
                i = i + 2; // skip and don't don't if it's a pipe code
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

void scrollToScrollbackSection(char** scrollLines, int start, int end, int height) {
    int row = 1;
    for (int i = start; i < end && row < height; i++) {
        od_set_cursor(row, 1);
        od_clr_line();
        od_disp_emu(pipeToAnsi(scrollLines[i]), TRUE);
        row = row + 1;
    }
}

/**
 *
 * initialScroll:
 *  - how far up to scroll when entering scrollback mode
 * 
 * mode: 
 *  - 0 = Standard scrollback; full scrolling with arrorws, pgup/pgdn, home, and end
 *  - 1 = Mention scrollback; only scroll to mention markers with arrows
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
    od_clr_line();
    scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
    od_set_cursor(od_control.user_screen_length - 2, 1);
    od_disp_emu(gStatusThemeLine1, TRUE);
    od_set_cursor(od_control.user_screen_length - 2, 6);
    if (mode == 0) { // 0 = standard scrollback mode        
        od_printf("`bright white`Scrollback`bright black`:          `bright white`\030`bright black`/`bright white`\031`bright black`/`bright white`PGUP`bright black`/`bright white`PGDN`bright black`/`bright white`HOME`bright black`/`bright white`END``   `bright white`ENTER`` to return to chat");
    }
    else if (mode == 1) { // 1 = mention scrollback mode
        od_printf("`bright white`Mentions`bright black`:            `bright white`\030`bright black`/`bright white`\031`bright black`/`bright white`PGUP`bright black`/`bright white`PGDN`bright black`/`bright white`HOME`bright black`/`bright white`END``   `bright white`ENTER`` to return to chat");
    }
    od_set_cursor(od_control.user_screen_length - 1, 1);
    od_disp_emu(gStatusThemeLine2, TRUE);

    bool isEscapeSequence = false;
    int escapeLoop = false;
    tODInputEvent InputEvent;

    while (!exitScrollback) {

        od_set_cursor(od_control.user_screen_length - 2, 18);
        od_printf("`bright white`%.0f``%%  ", (scrollPos / (scrollMax + 0.1)) * 100);
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
            case OD_KEY_PGUP:  // Only local keyboard PGUP
                scrollPos = scrollPos - height;
                if (scrollPos < 0) {
                    scrollPos = 0;
                }
                scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
                break;

            case '\x0e':
            case OD_KEY_PGDN:  // Only local keyboard PGDN
                scrollPos = scrollPos + height;
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

            if (InputEvent.chKeyPress == 27 && !isEscapeSequence) { // User pressed a key that triggers an escape sequence
                isEscapeSequence = true;
                continue;
            }
            else if (isEscapeSequence && (toupper(InputEvent.chKeyPress) >= 65 && toupper(InputEvent.chKeyPress) <= 90)) { // Get the remaining escape code; up to the next alpha
                isEscapeSequence = false;
                if (InputEvent.chKeyPress == 86) { // User pressed PGUP
                    scrollPos = scrollPos - height;
                    if (scrollPos < 0) {
                        scrollPos = 0;
                    }
                    scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
                }
                else if (InputEvent.chKeyPress == 85) { // User pressed PGDN
                    scrollPos = scrollPos + height;
                    if (scrollPos > scrollMax) {
                        scrollPos = scrollMax;
                    }
                    scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
                }
                else {
                    escapeLoop = 0;
                }
                continue;
            }
            else if (InputEvent.chKeyPress == 13 || InputEvent.chKeyPress == 10) {
                exitScrollback = true;
                isEscapeSequence = false;
                break;
            }
            else if (isEscapeSequence) {
                escapeLoop = escapeLoop + 1;
                continue;
            }
        }
    }

    // refresh the scrollLines and re-display the latest lines when exiting scrollback, in 
    // case any were received while scrolling.
    scrollLineCount = split(gScrollBack, '\n', &scrollLines);
    scrollPos = scrollLineCount - height;
    if (scrollPos < 0) {
        scrollPos = 0;
    }
    scrollToScrollbackSection(scrollLines, scrollPos, scrollLineCount, height);
    free(scrollLines); // needed?

    isChatPaused = false;
    drawStatusBar();
    od_set_cursor(od_control.user_screen_length, 1);
    od_printf(CHAT_CURSOR);
}

/**
 *  Adds a string to the Scrollback OR to the Mentions
 *   mode=0: regular chat history scrollback
 *   mode=1: mention history
 */
void addToScrollBack(char* msg, int mode) {
    const size_t a = strlen(mode==0 ? gScrollBack : gMentions );
    const size_t b = strlen(msg);
    const size_t size_ab = a + b + 1 +1 ;
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
    int scrollLen = 1;
    char timeStamp[50] = "";
    _snprintf_s(timeStamp, 50, -1, (mention ? "|00|23%s|26\x1b[5m\376\x1b[0m|07" : "|08%s|07 "), getTimestamp());

    if (strLenWithoutPipecodes(msg) >= (od_control.user_screenwidth - 7)) {

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
        token = strtok_s(msg, " ", &context);
        while (token != NULL) {
            tokenLen = strLenWithoutPipecodes(token);
            if (tokenLen > od_control.user_screenwidth - 8) { // Insert an additional scrollLen if a single 
                scrollLen = scrollLen + 1;                    // word is longer than the terminal width.
            }
            linelen = linelen + tokenLen + 1;
            if (linelen > (od_control.user_screenwidth  - 8 )) {
                _snprintf_s(wrappedMsg, sizeof(wrappedMsg), -1, "%s%s", wrappedMsg, "\r\n      \300");
                scrollLen = scrollLen + 1;
                linelen = 6 + (tokenLen + 1);
            }
            _snprintf_s(wrappedMsg, sizeof(wrappedMsg), -1, tokencnt==0 ? "%s%s" : "%s %s", wrappedMsg, token);
            tokencnt = tokencnt + 1;
            token = strtok_s(NULL, " ", &context);
        }
        strncpy_s(msg, sizeof(wrappedMsg), wrappedMsg, -1);
    }

    char dispMsg[512]="";
    _snprintf_s(dispMsg, 512, -1, "%s%s|07", timeStamp, msg);        

    // Save this message to the chat scrollback buffer
    addToScrollBack(dispMsg, 0);
    if (mention) {
        addToScrollBack(dispMsg, 1);
    }
    if (!isChatPaused) {
        od_scroll(1, 1, od_control.user_screenwidth, od_control.user_screen_length - 3, scrollLen, 0);
        od_set_cursor(od_control.user_screen_length - (2 + scrollLen), 1);
        od_disp_emu(pipeToAnsi(dispMsg), TRUE);
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
        /*char timeStamp[50] = "";
        char msgToStore[512] = "";
        _snprintf_s(timeStamp, 50, -1, (mention ? "|00|23%s|26\x1b[5m\376\x1b[0m|07" : "|08%s|07 "), getTimestamp());
        _snprintf_s(msgToStore, 512, -1, "%s%s|07", timeStamp, msg);
        addToScrollBack(msgToStore, 0);                              // TODO: wrap message before storing it
        if (mention) {
            addToScrollBack(msgToStore, 1);
        }
        */

        // Just send it to the displayMessage function, since it has
        // built-in handling for adding wrapped messages to scrollback
        // and won't try to display it if chat is paused.        
        displayMessage(msg, mention);
    }
    else {
        while (mc.message != NULL) {
            od_sleep(0);
        }
        mc.message = msg;
        mc.isMention = mention;
    }
}

void listThemesInChat() {
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
    displayMessage("|08__", false);
}

void displayPipeFileInChat(char* filename) {
    FILE* extFile;
    fopen_s(&extFile, filename, "r");
    if (extFile != NULL) {
        char line[200] = "";
        while (fgets(line, sizeof(line), extFile)) {            
           displayMessage( strReplace( line, "\n", ""), false);
        }
        displayMessage("|08__", false);
        fclose(extFile);
    }
}

void displayPipeFile(char* filename) {
    FILE* extFile;
    fopen_s(&extFile, filename, "r");
    if (extFile != NULL) {
        char line[200] = "";
        while (fgets(line, sizeof(line), extFile)) {
            od_printf("\r"); // why???
            od_disp_emu(pipeToAnsi(line), TRUE);
        }
        fclose(extFile);
    }
}

void processUserCommand(char* cmd, char* params) {
    if (_strcmpi(cmd, "quit") == 0 || _strcmpi(cmd, "q") == 0) {
        sendMsgPacket(&mrcSock, "NOTME", "", "", user.exitMessage);
        sendMsgPacket(&mrcSock, "SERVER", "", "", "LOGOFF");
        gIsInChat = false;
        shutdown(mrcSock, SD_SEND);
    }
    else if (_strcmpi(cmd, "join") == 0 || _strcmpi(cmd, "j") == 0) {
        char newRoom[20] = "";
        strncpy_s(newRoom, 20, strReplace(params, "#", ""), -1);  // no #
        strncpy_s(newRoom, 20, strReplace(newRoom, " ", "_"), -1); // Single word
        stripPipeCodes(newRoom); // No pipe codes

        char newRoomCmd[30] = "";
        _snprintf_s(newRoomCmd, 30, -1, "NEWROOM:%s:", gRoom); // include the OLD room as the first parameter...
        sendCmdPacket(&mrcSock, newRoomCmd, newRoom); // the NEW room will be included as the second parameter...
        strncpy_s(gRoom, 30, newRoom, -1); 
        sendCmdPacket(&mrcSock, "USERLIST", ""); // Need a new user list after joining a different room
    }
    else if (_strcmpi(cmd, "topic") == 0) {              
        stripPipeCodes(params);  // No pipe codes
        char newTopicCmd[30] = "";
        _snprintf_s(newTopicCmd, 30, -1, "NEWTOPIC:%s:", gRoom);
        sendCmdPacket(&mrcSock, newTopicCmd, params);
    }
    else if (strcmp(cmd, "?") == 0) {
        sendCmdPacket(&mrcSock, "help", "");
    }
    else if (_strcmpi(cmd, "rooms") == 0) {
        sendCmdPacket(&mrcSock, "list", "");
    }
    else if (_strcmpi(cmd, "me") == 0) {
        char action[MSG_LEN] = "";
        _snprintf_s(action, MSG_LEN, -1, "|15* |13%s %s", user.chatterName, params);
        sendMsgPacket(&mrcSock, "", "", gRoom, action);
    }
    else if (_strcmpi(cmd, "t") == 0 || _strcmpi(cmd, "msg") == 0) {
        char to[36] = "";
        char msg[PACKET_LEN] = "";
        int nextspcidx = indexOfChar(params, ' ') + 1;
        if (nextspcidx > 0) {
            strncpy_s(to, nextspcidx, params, -1);
            _snprintf_s(msg, PACKET_LEN, -1, "|15* |08(|15%s|08/|14DirectMsg|08) |07%s", user.chatterName, params + nextspcidx);
            sendMsgPacket(&mrcSock, to, "", gRoom, msg);
            _snprintf_s(msg, PACKET_LEN, -1, "|15* |08(|14DirectMsg|08->|15%s |07%s", to, params + nextspcidx);
            displayMessage(msg, false);
        }
    }
    else if (_strcmpi(cmd, "r") == 0) {
        char rep[PACKET_LEN] = "";
        _snprintf_s(rep, PACKET_LEN, -1, "|15* |08(|15%s|08/|14DirectMsg|08) |07%s", user.chatterName, params);
        sendMsgPacket(&mrcSock, gLastDirectMsgFrom, "", gRoom, rep);
        _snprintf_s(rep, PACKET_LEN, -1, "|15* |08(|14DirectMsg|08->|15%s|08) |07%s", gLastDirectMsgFrom, params);
        displayMessage(rep, false);
    }
    else if (_strcmpi(cmd, "b") == 0) {
        char bcast[PACKET_LEN] = "";
        _snprintf_s(bcast, PACKET_LEN, -1, "|15* |08(|15%s|08/|14Broadcast|08) |07%s", gDisplayChatterName, params);
        sendMsgPacket(&mrcSock, "", "", "", bcast);
    }
    else if (_strcmpi(cmd, "sound") == 0) {
        user.chatSounds = !user.chatSounds;
        char sndstat[50] = "";
        _snprintf_s(sndstat, sizeof(sndstat), -1, "|15* |14Chat sounds |15%s|14.", (user.chatSounds ? "ON" : "OFF"));
        displayMessage(sndstat, false);
    }
    else if (_strcmpi(cmd, "ctcp") == 0) {
        char target[36] = "";
        char ctcp_data[50] = "";
        int nextspcidx = indexOfChar(params, ' ') + 1;
        if (nextspcidx > 0) {
            strncpy_s(target, nextspcidx, params, -1);
            ustr(params);
            _snprintf_s(ctcp_data, 50, -1, "%s %s", target, params + nextspcidx);
            sendCtcpPacket(&mrcSock, (strcmp(target, "*") == 0 || target[0] == '#') ? "" : target, "[CTCP]", ctcp_data);
        }
    }
    //else if (_strcmpi(cmd, "twit") == 0) { // For later
    //}
    else if (_stricmp(cmd, "help") == 0) {

        if (_stricmp(params, "ctcp") == 0) {
            displayPipeFileInChat("screens\\helpctcp.txt");
        }
        else {
            displayPipeFileInChat("screens\\help.txt");
        }
    }
    else if (_strcmpi(cmd, "quote") == 0) {
        sendCmdPacket(&mrcSock, params, "");
    }
    else if (_strcmpi(cmd, "redraw") == 0) {
        drawStatusBar();
        displayMessage("|15* |14Status bar redrawn", false);
    }
    else if (_strcmpi(cmd, "scroll") == 0) {
        enterScrollBack(0, 0);
    }
    else if (_strcmpi(cmd, "mentions") == 0) {
        gMentionCount = 0;
        enterScrollBack(0, 1);
        updateMentions();
    }
    else if (_strcmpi(cmd, "theme") == 0) {
        if (strlen(params) == 0) {
            listThemesInChat();
        }
        else {
            _snprintf_s(user.theme, 20, -1, "%s.ans", params);
            loadTheme();
            drawStatusBar();
        }
    }

    // Any other server command goes straight to the server and processed there.
    // If the server received an invalid command, it will let the user know.
    else {
        sendCmdPacket(&mrcSock, cmd, params);
        
        // request a new USERLIST when checking the current users
        if (_strcmpi(cmd, "users") == 0 ||
            _strcmpi(cmd, "whoon") == 0 ||
            _strcmpi(cmd, "chatters") == 0) {
            sendCmdPacket(&mrcSock, "USERLIST", "");
        }
    }
}

bool processServerMessage(char* body, char* toUser) {

    bool shouldTerminateSession = false;

    // Parse the body for command strings
    char cmd[141] = "";
    char params[512] = "";
    int cmdsep = indexOfChar(body, ':');
    if (cmdsep > 0) {
        strncpy_s(cmd, cmdsep + 1, body, -1);
        strncpy_s(params, sizeof(params), body + cmdsep + 1, -1);
    }
    else {
        strncpy_s(cmd, sizeof(cmd), body, -1);
    }

    // Implemented SERVER commands:
    //
    if (strcmp(cmd, "BANNER") == 0 || strcmp(cmd, "NOTIFY") == 0) {
        // BANNERS are seldom sent from the host, so this client doesn't
        // bother making special use of them. If a banner is ever sent,
        // simply display it in chat.
        //
        // NOTIFY: notification messages are seldom used as well, and 
        // have the same structure as a BANNER packet, so handle it
        // the same way.
        queueIncomingMessage(params, true);
    }
    else if (strcmp(cmd, "ROOMTOPIC") == 0) {
        // ROOMTOPIC : Server will send new topic when changed
        //
        // No pipe codes
        // Room name cannot contain spaces
        // Topic can contain spaces
        strncpy_s(gTopic, sizeof(gTopic), params + strlen(gRoom) + 1, -1);
        gRoomTopicChanged = true;
    }
    else if (strcmp(cmd, "USERROOM") == 0) {
        // USERROOM: Server confirm the room user is in, usually sent after NEWROOM but may be the result of other reasons.[NEW in 1.3]
        // Client must enforce or routing will break
        strncpy_s(gRoom, 30, params, -1);
        gRoomTopicChanged = true;
    }
    else if (strcmp(cmd, "USERNICK") == 0) {
        // USERNICK : Server confirm the nick user can use, usually sent after IAMHERE but may be the result of other reasons. [NEW in 1.3]
        // Client must enforce or routing will break.
        // This will be used to avoid delivery conflict in case 2 users with the same nick on 2 different BBSes are connected.
        // The server will append a number to the original nick to resolve the conflict.
        strncpy_s(user.chatterName, 30, params, -1);
        _snprintf_s(gDisplayChatterName, 80, -1, "|%02d|%02d%c|%02d|%02d%s%s|16", user.chatterNamePrefixFgColor, user.chatterNamePrefixBgColor, user.chatterNamePrefix, user.chatterNameFgColor, user.chatterNameBgColor, user.chatterName, user.chatterNameSuffix);
        // inform the user of the change...
        char nicknotice[140] = "";
        _snprintf_s(nicknotice, 140, -1, "|15* |08(|14Notice|08) |07The MRC server has updated your name to |15%s|07.", user.chatterName);
        queueIncomingMessage(nicknotice, true);
    }
    else if (strcmp(cmd, "TERMINATE") == 0) {
        // TERMINATE: Server requests the client interface to terminate.[NEW in 1.3]
        // Client must enforce
        // This is to allow for the chat interface to gracefully terminate the connection.
        gIsInChat = false;
        displayMessage(params, true); // should be displayed immediately
        pause();
        shouldTerminateSession = true;
    }
    else if (strcmp(cmd, "USERLIST") == 0) {
        gChatterCount = split(params, ',', &gChattersInRoom);
        gUserCountChanged = true;
    }
    else if (strcmp(cmd, "LATENCY") == 0) {
        strncpy_s(gLatency, sizeof(gLatency), params, -1);
        gLatencyChanged = true;
    }
    else if (strcmp(cmd, "RECONNECT") == 0) {
        queueIncomingMessage("|15* |08(|14Notice|08) |07MRC host connection re-established!", false);
        // Rejoin the room if re-connecting to the host
        sendCmdPacket(&mrcSock, "IAMHERE", "");
        sendCmdPacket(&mrcSock, "NEWROOM::", gRoom);
    }
    // Just display the whole incoming server message if it's not a recognized command string,
    // since it's most likely an informational message from the SERVER.
    else if (strlen(toUser) == 0 || _strcmpi(toUser, user.chatterName) == 0) {
        queueIncomingMessage(body, false);
        od_sleep(10);
    }
    return shouldTerminateSession;
}

void processCtcpCommand(char* body, char* toUser, char* fromUser) {

    // TODO: - This works, but could be written better.. 

    if (strncmp(body, "[CTCP] ", 7) == 0 && (_strcmpi(toUser, user.chatterName) == 0 || strlen(toUser)==0 /* || strcmp(toUser, "*") == 0) || (toUser[0] == '#' && _strcmpi(toUser + 1, gRoom) == 0*/))
    {
        char cmdStr[80] = "";
        char repStr[80] = "";

        strncpy_s(cmdStr, sizeof(cmdStr), body + 7 + strlen(fromUser) + 1 + (strlen(toUser) == 0 ? 1 : strlen(toUser))  + 1, -1);

        if (_strnicmp(cmdStr, "VERSION", 7) == 0) {
            _snprintf_s(repStr, sizeof(repStr), -1, "VERSION %s v%s.%s %s [%s]", TITLE, PROTOCOL_VERSION, UMRC_VERSION, COMPILE_DATE, AUTHOR_INITIALS);
        }
        else if (_strnicmp(cmdStr, "TIME", 4) == 0) {
            _snprintf_s(repStr, sizeof(repStr), -1, "TIME %s ", getCtcpDatetime());
        }
        else if (_strnicmp(cmdStr, "PING", 4) == 0) {
            _snprintf_s(repStr, sizeof(repStr), -1, "PING %s", cmdStr + 5);
        }
        else if (_strnicmp(cmdStr, "CLIENTINFO", 10) == 0) {
            _snprintf_s(repStr, sizeof(repStr), -1, "CLIENTINFO VERSION TIME PING CLIENTINFO");
        }
        sendCtcpPacket(&mrcSock, fromUser, "[CTCP-REPLY]", repStr);
    }
    else if (strncmp(body, "[CTCP-REPLY] ", 13) == 0 && _strcmpi(toUser, user.chatterName) == 0) {
        char repStr[80] = "";
        char resp[MSG_LEN] = "";
        strncpy_s(repStr, 80, body + 13 + strlen(fromUser) + 1, -1);
        _snprintf_s(resp, MSG_LEN, -1, "* |14[CTCP-REPLY] |10%s |15%s", fromUser, repStr);
        queueIncomingMessage( resp, false);
        od_sleep(20);
    }
}

DWORD WINAPI handleIncomingMessages(LPVOID lpArg) {
    int iResult = 0;
    time_t lastIamHere;
    time(&lastIamHere);

    while (gIsInChat) {

        time_t curtime;
        time(&curtime);
        if (curtime - lastIamHere >= 60) {
            sendCmdPacket(&mrcSock, "IAMHERE", "");
            lastIamHere = curtime;
        }

        char inboundData[DATA_LEN] = "";

        while ((iResult = recv(mrcSock, inboundData, DATA_LEN, 0)) != 0 && gIsInChat) { // continue till disconnected       
            if (iResult == -1) {
                if (WSAGetLastError() == WSAEMSGSIZE) { // server has more data to send than the buffer can get in one call
                    continue; // iterate again to get more data
                }
            }
            else {
                break;
            }
        }

        // I know it's redundant, but I need a check here to abort the loop and not enter the next if
        if (!gIsInChat) {
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

                char* fromUser = "", * fromSite = "", * fromRoom = "", * toUser = "", * msgExt = "", * toRoom = "", * body = "";
                processPacket(packet, &fromUser, &fromSite, &fromRoom, &toUser, &msgExt, &toRoom, &body);

                if (strcmp(fromUser, "SERVER") == 0 && (strcmp(toRoom, gRoom) == 0 || strlen(toRoom) == 0)) {

                    if (processServerMessage(body, toUser)) {
                        // Returns TRUE if the incoming server command should
                        // terminate the session, breaking the loop.
                        gIsInChat = false;
                        shutdown(mrcSock, SD_SEND);
                        break;
                    }
                                        
                    // refresh the user list when the SERVER announces joins and exits
                    if (strstr(body, "Joining") != NULL ||
                        strstr(body, "Leaving") != NULL ||
                        strstr(body, "Timeout") != NULL ||
                        strstr(body, "Rename") != NULL || 
                        strstr(body, "Linked") != NULL ||
                        strstr(body, "Unlink") != NULL ) {

                        sendCmdPacket(&mrcSock, "USERLIST", "");
                    }
                }                
                else if (strcmp(toRoom, CTCP_ROOM) == 0 && strcmp(fromRoom, CTCP_ROOM) == 0) {
                    processCtcpCommand(body, toUser, fromUser);
                }
                
                // Joe_Boo~2o_fOr_beeRS~lobby~NOTME~~lobby~|07- |11Joe_Boo |03has entered the room.~
                // Joe_Boo~2o_fOr_beeRS~ddial~NOTME~~ddial~|07- |11Joe_Boo |03has entered the room.~
                
                else if (strcmp(toUser, "NOTME") == 0 ) {
                    if ((_strcmpi(fromRoom, gRoom) == 0 || strlen(fromRoom) == 0) && 
                        (_strcmpi(toRoom, gRoom) == 0 || strlen(toRoom) == 0)) {
                        queueIncomingMessage( body, false);
                        sendCmdPacket(&mrcSock, "USERLIST", "");
                    }
                    //else if (_strcmpi(fromUser, user.chatterName) != 0) {
                    //    queueIncomingMessage( body, false);
                    //}
                }
                //else if (strcmp(toUser, "NOTME") == 0 && _strcmpi(fromUser, user.chatterName) != 0) { // fix for join messages in other rooms
                //    queueIncomingMessage( body, false);
                //}
                else if (
                        // messages addressed to the room or to the chatter
                        ((strcmp(gRoom, toRoom)==0 || strlen(toRoom)==0  ) && (strlen(toUser) == 0 || _strcmpi(toUser, user.chatterName) == 0)) || 
                        // broadcast: messages addressed to everyone; no room and no user specified
                        (strlen(toRoom) == 0 && strlen(toUser) == 0)) {
                    // Direct message (DirectMsg) ...? 
                    if (strlen(toUser) != 0 && strlen(fromUser) != 0) {
                        queueIncomingMessage(body, false);
                        strncpy_s(gLastDirectMsgFrom, 36, fromUser, -1);
                    }

                    // Standard message
                    else {
                        bool mentioned = false;

                        if (_strcmpi(fromUser, user.chatterName) != 0 && strContainsStrI(body, user.chatterName)) {
                            gMentionCount = gMentionCount + 1;
                            mentioned = true;
                            gMentionCountChanged = true;
                            if (user.chatSounds) {
                                od_putch(7);
                            }
                        }
                        queueIncomingMessage( body, mentioned);
                    }
                }
                od_sleep(1);
            }
        }
        else if (iResult == 0) {
            od_printf("%s Connection closed\r\n", getTimestamp());
            gIsInChat = false;
            pause();
        }
        else {
            od_printf("%s recv failed with error: %d\r\n", getTimestamp(), WSAGetLastError());
            gIsInChat = false;
            pause();
        }
    }
    return 0;
}



/**
 *  This function handles all chat I/O: getting input from the user as
 *  well as updating the screen.
 */
void doChatRoutines(char* input) {

    bool isEscapeSequence = false;
    char key = ' ';
    tODInputEvent InputEvent;

    while (true) {

        // We can only update the screen from this thread, so we're going to 
        // check for any incoming messages and other changes, and display 
        // them as needed. Then we'll handle user input
        
        Sleep(0);

        if (mc.message != NULL) {
            displayMessage(mc.message, mc.isMention);
            mc.message = NULL;
            mc.isMention = false;
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
        if (od_get_input(&InputEvent, 1, GETIN_NORMAL ) == FALSE) {    
            od_sleep(0);
            continue;
        }

        key = ' ';
        char pcol[4] = "";

        if (InputEvent.EventType == EVENT_EXTENDED_KEY)
        {
            int overfill = 0;
            switch (InputEvent.chKeyPress)
            {

            case OD_KEY_LEFT:
                user.textColor = user.textColor - 1;
                if (user.textColor < 1) {
                    user.textColor = 15;
                }
                if ((int)strlen(input) >= (int)od_control.user_screenwidth - 2) {
                    overfill = strlen(input) - (od_control.user_screenwidth - 3);
                }
                _snprintf_s(pcol, 4, -1, "|%02d", user.textColor);

                // Scroll the input string display if it's longer than the terminal width
                od_set_cursor(od_control.user_screen_length, 1);
                od_disp_emu(pipeToAnsi(pcol), TRUE);
                if (overfill > 0) {
                    od_disp_str(input + overfill - 1);
                }
                else {
                    od_disp_str(input);
                }
                break;

            case OD_KEY_RIGHT:
                user.textColor = user.textColor + 1;
                if (user.textColor > 15) {
                    user.textColor = 1;
                }

                if ((int)strlen(input) >= (int)od_control.user_screenwidth - 2) {
                    overfill = strlen(input) - (od_control.user_screenwidth - 3);
                }
                _snprintf_s(pcol, 4, -1, "|%02d", user.textColor);

                // Scroll the input string display if it's longer than the terminal width
                od_set_cursor(od_control.user_screen_length, 1);
                od_disp_emu(pipeToAnsi(pcol), TRUE);
                if (overfill > 0) {
                    od_disp_str(input + overfill - 1);
                }
                else {
                    od_disp_str(input);
                }
                break;

            case OD_KEY_DELETE:
                strcpy_s(input, sizeof(input), "");
                od_set_cursor(od_control.user_screen_length, 1);
                od_clr_line(); // TODO: replace with gray dots
                od_printf(CHAT_CURSOR);
                break;

            case '\x0e':       // This seems to only recognize PGUP if
            case OD_KEY_PGUP:  // pressed on the local keyboard, but not from remote.
                enterScrollBack(od_control.user_screen_length - 3, 0);
                break;

            case OD_KEY_UP:
                enterScrollBack(0, 1);
                gMentionCount = 0;
                updateMentions();
                break;
            }
        }

        else if (InputEvent.EventType == EVENT_CHARACTER)
        {
            int endOfInput = 0;
            char tabResult[30] = "";
            key = InputEvent.chKeyPress;

            // Add the keystroke to the input string...            
            if (key == 27 && !isEscapeSequence) { // User pressed a key that triggers an escape sequence
                isEscapeSequence = true;
                continue;
            }
            else if (isEscapeSequence && (toupper(key) >= 65 && toupper(key) <= 90)) { // Get the remaining escape code; up to the next alpha
                isEscapeSequence = false;

                if (key == 86) { // User pressed PGUP
                    enterScrollBack(od_control.user_screen_length - 3, 0);
                }
                continue;
            }
            else if (key == 13 || key == 10) {
                od_set_cursor(od_control.user_screen_length, 1);
                od_clr_line(); // TODO: replace with gray dots
                isEscapeSequence = false;
                break;
            }
            else if (isEscapeSequence) { // skip and get the next part of the escape sequence
                continue;
            }
            else if (key >= 32 && key <= 125) { // allowed characters
                if (strlen(input) <= MSG_LEN) {
                    _snprintf_s(input, MSG_LEN, -1, "%s%c", input, key);
                }
            }
            else if (key == 8) { // backspace
                if (strlen(input) > 0) {
                    _snprintf_s(input, MSG_LEN, strlen(input) - 1, "%s", input);
                    endOfInput = endOfInput + 1;
                }
            }
            else if (key == 9) {   // TAB - chatter name completion

                int indexOfTabSearch = -1;
                char tabSearch[140] = "";
                // capture the typed portion after the last space or the beginning of the string,
                // and use that as the user search string.
                for (int i = strlen(input); i >= 0 && input[i] != ' '; i--) {
                    if (input[i] != ' ') {
                        indexOfTabSearch = i;
                    }
                }
                strcpy_s(tabSearch, sizeof(tabSearch), input + indexOfTabSearch);

                for (int i = 0; i < gChatterCount; i++) {
                    if (_strnicmp(tabSearch, gChattersInRoom[i], strlen(tabSearch)) == 0 && _strcmpi(gChattersInRoom[i], user.chatterName) != 0) {
                        strcpy_s(tabResult, 30, gChattersInRoom[i] + strlen(tabSearch));
                        _snprintf_s(input, MSG_LEN, -1, "%s%s", input, tabResult);
                        endOfInput = endOfInput + 1;
                        break;
                    }
                }
                if (strlen(tabResult) == 0) { // if no match, just skip
                    continue;
                }
            }                   
            else {
                continue;
            }

            //
            // Done capturing input...
            // 
            // Below updates the input display...
            // 
            int overfill = 0;
            if ((int)strlen(input) < (int)od_control.user_screenwidth - 3) {
                endOfInput += strlen(input) - strlen(tabResult);
            }
            else {
                // determine whether the input string is longer than the terminal width,
                // so it can be displaying appropriately below
                overfill = strlen(input) - (od_control.user_screenwidth - 4);
                endOfInput = od_control.user_screenwidth - 1 + (key == 8 ? -1 : -2) - (strlen(tabResult) > 0 ? strlen(tabResult)-1: 0);
            }
            _snprintf_s(pcol, 4, -1, "|%02d", user.textColor);

            // Scroll the input string display if it's longer than the terminal width
            if (overfill > 0) {
                od_set_cursor(od_control.user_screen_length, 1);
                od_disp_emu(pipeToAnsi(pcol), TRUE);  // .................................
                od_disp_str(input + overfill - 1); // possible source of screen dragging?
            }
            od_set_cursor(od_control.user_screen_length, endOfInput);
            od_disp_emu(pipeToAnsi(pcol), TRUE);

            if (key == 8) {              // Type the backspace...
                if (strlen(input) > 0) {
                    od_disp_emu(pipeToAnsi("|08|16"), TRUE);
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

                    od_putch('*'); 
                }

                else {            // Any other character simply gets displayed as typed...
                    od_putch(key);
                }
            }
            od_printf(CHAT_CURSOR);
            updateBuffer(strlen(input));
        }
    }
}

bool enterChat() {
        
    if (user.theme == NULL || strlen(user.theme) == 0) {
        strcpy_s(user.theme, sizeof(user.theme), "default.ans");
    }
    loadTheme();    

    od_disp_emu("\x1b[?25l", TRUE); // disable the blinking cursor.. don't need it since we're going to make our own..
    od_clr_scr();    

    // get the sysop name from config in case the one OpenDoors pulls from the dropfile is "Sysop" or blank.
    char sysopName[140] = "";
    strcpy_s(sysopName, sizeof(sysopName), strReplace((_strcmpi(od_control.sysop_name, "sysop")==0 || strlen(od_control.sysop_name)==0) ? cfg.sys : od_control.sysop_name, "~", ""));
    stripPipeCodes(sysopName);

    strcpy_s(gRoom, sizeof(gRoom), user.defaultRoom);

    gScrollBack = malloc(50);
    strcpy_s(gScrollBack, 50, "|15 * * * TOP OF SCROLLBACK * * * |07\n\n\n\n\n");

    gMentions = malloc(50);
    strcpy_s(gMentions, 50, ""); // |15 * * * TOP OF MENTIONS * * * |07\n

    int iResult;
    WSADATA wsaData;
    struct addrinfo* mhResult = NULL, * ptrMh = NULL, mrcHost;

    drawStatusBar();
    displayMessage("Starting up...", false);

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        displayMessage("WSAStartup failed", false);
        pause();
        return false;
    }

    ZeroMemory(&mrcHost, sizeof(mrcHost));
    mrcHost.ai_family = AF_UNSPEC;
    mrcHost.ai_socktype = SOCK_STREAM;
    mrcHost.ai_protocol = IPPROTO_TCP;

    iResult = getaddrinfo("localhost", cfg.port, &mrcHost, &mhResult);
    if (iResult != 0) {
        displayMessage("getaddrinfo failed", false);
        WSACleanup();
        pause();
        return false;
    }

    for (ptrMh = mhResult; ptrMh != NULL; ptrMh = ptrMh->ai_next) {
        mrcSock = socket(ptrMh->ai_family, ptrMh->ai_socktype, ptrMh->ai_protocol);
        if (mrcSock == INVALID_SOCKET) {
            WSACleanup();
            pause();
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
        displayMessage("Unable to connect to bridge!", false);
        WSACleanup();
        pause();
        return false;
    }

    if (sendCmdPacket(&mrcSock, "IAMHERE", "")) {
        gIsInChat = true;
        displayMessage("Connected!", false);
        od_sleep(20);
    }

    // Client connection to the Bridge has been established a this point.
    //
    // 2 threads:
    //
    //  - Incoming one to listen to messages from the Bridge, and place them in the chat window.
    DWORD incomingThreadID;
    HANDLE hIncoming = CreateThread(NULL, 0, handleIncomingMessages, NULL, 0, &incomingThreadID);

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

    // TODO: User IP (USERIP)? ... maybe make IP an optional custom param passed by the BBS, assuming the BBS software is capable of detecting it...

    //sendCmdPacket(&mrcSock, "USERLIST", ""); // needed? if we're joining a room shortly below?
    //od_sleep(20);

    // Announce user and place user into room after initial packets
    //
    sendMsgPacket(&mrcSock, "NOTME", "", "", user.joinMessage);
    od_sleep(20);
    sendCmdPacket(&mrcSock, "NEWROOM::", gRoom);
    od_sleep(20);    
    sendCmdPacket(&mrcSock, "BANNERS", "");
    od_sleep(20);

    // Loop to get input from the user, and send it over the Bridge.
    while (gIsInChat) {

        char input[MSG_LEN] = "";
        updateBuffer(0);

        od_set_cursor(od_control.user_screen_length, 1);
        od_printf(CHAT_CURSOR);

        doChatRoutines(input); // capture user input and keep chat display updated

        if (strlen(input) == 0) { // do nothing on blank entry
            continue;
        }

        if (input[0] == '/') {
            char cmd[15] = "";
            char params[130] = "";
            int spcidx = indexOfChar(input, ' ');
            if (spcidx > 0) {
                strncpy_s(cmd, spcidx, input + 1, -1);
                strncpy_s(params, 130, input + spcidx + 1, -1);
            }
            else {
                strncpy_s(cmd, 15, input + 1, -1);
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
    closesocket(mrcSock);
    WSACleanup();

    gMentionCount=0;
    free(gScrollBack);

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
#else
int main(int argc, char** argv)
{
#endif

    bool exit = false;
    int userNumber = -1;
    char gUserDataFile[260] = "";

#if defined WIN32
    od_parse_cmd_line(lpszCmdLine);
#else
    od_parse_cmd_line(argc, argv);
#endif
       
    od_control.od_page_pausing = false;
    od_control.od_disable |= DIS_NAME_PROMPT; // Disable the local user name prompt; it doesn't work.. always results in "Sysop"
    od_control.od_time_msg_func = displayTimeWarning;
    strncpy_s(od_control.od_prog_name, 40, TITLE, -1);
    _snprintf_s(od_control.od_prog_version, 40, -1, "v%s", UMRC_VERSION);
    strncpy_s(od_control.od_prog_copyright, 40, YEAR_AND_AUTHOR, -1);

    HICON hIcon = (HICON)LoadImage(NULL, "icon.ico", IMAGE_ICON, 0, 0, LR_LOADFROMFILE | LR_DEFAULTSIZE | LR_SHARED);
    if (hIcon != NULL) {
        od_control.od_app_icon = hIcon;
    }

    if (loadData(&cfg, CONFIG_FILE) != 0) {
        od_printf("Invalid config. Run Config.exe.\r\n");
        od_exit(-1, FALSE);
    }

    od_init();

    // TODO: Make these configurable in mrc.cfg? 
    od_control.od_inactivity = 0;     
    od_control.od_maxtime = 0;           
    od_control.user_ansi = TRUE;
    od_control.user_screen_length = 24;

    od_clr_scr();

    userNumber = od_control.user_num;
    if (0 == userNumber && strcmp(od_control.user_name, "Sysop") == 0) {
        od_printf("`bright yellow`***`bright white`LOCAL MODE`bright yellow`***``\r\n\r\n");
        strncpy_s(user.chatterName, 36, od_control.user_name, -1);
    }
    else {
        strncpy_s(user.chatterName, 36, strlen(od_control.user_handle) > 0 ? od_control.user_handle : od_control.user_name, -1);
    }    
    strncpy_s(user.chatterName, 36, strReplace(user.chatterName, " ", "_"), -1); // spaces are disallowed; replace with underscores
    strncpy_s(user.chatterName, 36, strReplace(user.chatterName, "~", ""), -1);  // tildes are disallowed; strip them out
    strncpy_s(gFromSite, 30, strReplace(cfg.name, "~", ""), -1);
    stripPipeCodes(gFromSite);
    _snprintf_s(gUserDataFile, sizeof(gUserDataFile), -1, "%s\\%s.dat", USER_DATA_DIR, user.chatterName);

    DWORD attributes = GetFileAttributesA(USER_DATA_DIR);
    if (!( attributes != INVALID_FILE_ATTRIBUTES && (attributes & FILE_ATTRIBUTE_DIRECTORY))) {
        CreateDirectory(USER_DATA_DIR,NULL);
    }

    if (loadUser(&user, gUserDataFile) != 0) {

        // set some defaults for the new user
        user.userNumber = userNumber;
        user.chatSounds = false;                               
        user.textColor = 7;
        user.chatterNamePrefixFgColor = 8;
        user.chatterNamePrefixBgColor = 16;
        user.chatterNameFgColor = 7;
        user.chatterNameBgColor = 16;
        strncpy_s(user.defaultRoom, 30, DEFAULT_ROOM, -1);
        stripPipeCodes(user.defaultRoom);

        od_printf("`bright white`Welcome to %s!``\r\n", TITLE);
        od_printf(DIVIDER);

        od_printf("Looks like you're new here, %s.\r\n\r\n\r\n", user.chatterName);

        od_printf("``First things first...\r\n\r\nIs \"`bright white`%s``\" the name you want to use in chat? (Y/N) > ", user.chatterName);
        if (od_get_answer("YN") == 'N') {
            bool namevalid = false;
            char newchatname[36] = "";
            od_printf("``\r\n\r\nOK, what shall your chat name be?\r\n`bright black`Don't use pipe codes or anything here. We'll cover that shortly.`` \r\n\r\n> ");
            do {
                od_input_str(newchatname, 36, 32, 127);
                if (strchr(newchatname, ' ') != NULL || strchr(newchatname, '~') != NULL) {
                    strncpy_s(newchatname, 36, strReplace(newchatname, " ", "_"), -1);
                    strncpy_s(newchatname, 36, strReplace(newchatname, "~", ""), -1);
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

            strncpy_s(user.chatterName, 36, newchatname, -1);
            od_printf("`bright white`\r\n\r\nOK, you will be known as `bright green`%s`bright white` while in chat!``\r\n\r\n", user.chatterName);
        }
        else {
            od_printf("`bright white`\r\n\r\nOK, we'll leave it as-is!``\r\n\r\n");
        }

        od_printf("``If you ever change your mind, your sysop will need\r\nto `bright white`reset`` your uMRC settings.`` \r\n\r\n");
        defaultDisplayName();
        _snprintf_s(user.joinMessage, 50, -1, DEFAULT_JOIN_MSG, user.chatterName);
        _snprintf_s(user.exitMessage, 50, -1, DEFAULT_EXIT_MSG, user.chatterName);

        if (saveUser(&user, gUserDataFile) != -1) {

            od_printf("We've gone ahead set some default options for you. You can go ahead and\r\ncustomize them now.\r\n\r\n");
            od_printf(DIVIDER);
            pause();

            _snprintf_s(gDisplayChatterName, 80, -1, "|%02d|%02d%c|%02d|%02d%s%s", user.chatterNamePrefixFgColor, user.chatterNamePrefixBgColor, user.chatterNamePrefix, user.chatterNameFgColor, user.chatterNameBgColor, user.chatterName, user.chatterNameSuffix);
            enterChatterSettings(gUserDataFile);
        }
    }
    else {
        _snprintf_s(gDisplayChatterName, 80, -1, "|%02d|%02d%c|%02d|%02d%s%s", user.chatterNamePrefixFgColor, user.chatterNamePrefixBgColor, user.chatterNamePrefix, user.chatterNameFgColor, user.chatterNameBgColor, user.chatterName, user.chatterNameSuffix);
        od_printf("Existing user data LOADED.\r\n\r\n");
    }

    if (user.theme == NULL || strlen(user.theme) == 0) {    
        strcpy_s(user.theme, sizeof(user.theme), "default.ans");
    }
    loadTheme();    

    // Main menu .........................................
    //
    while (!exit) {

        od_clr_scr();
        od_send_file("screens\\intro.ans");

        int act = 0;
        char* bbses="", * rooms = "", * users = "", * activity = "";
        char mrcStTm[30]="";
        FILE* mrcstats;
        fopen_s(&mrcstats, MRC_STATS_FILE, "r");
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
                strncpy_s(gLatency, sizeof(gLatency), stat[4], -1);
            }
            act = atoi(activity);
            fclose(mrcstats);
        }

        struct stat file_stat;
        if (stat(MRC_STATS_FILE, &file_stat) == 0) {
            ctime_s(mrcStTm, 30, &file_stat.st_mtime);
            strncpy_s(mrcStTm, 30, strReplace(mrcStTm, "\n", ""), -1);
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
        
        od_set_cursor(12, 25);
        od_printf("`bright white`(`cyan`T`bright white`) `white`Show `bright white`T`white`ester Information");
        
        od_set_cursor(13, 25);
        od_printf("`bright white`(`bright green`Q`bright white`) `bright white`Q`white`uit to `bright white`%s", strReplace(gFromSite, "_", " "));

        time_t curtime;
        time(&curtime);

        od_set_cursor(18, 25);
        od_printf("`white`MRC Stats as of `bright green`%s`white`:", mrcStTm);
        od_set_cursor(20, 30);
        od_printf("`bright black`State``: `bright white`%s", curtime - file_stat.st_mtime <= 60 ? "`bright green`ONLINE``" : "`gray`OFFLINE``");
        od_set_cursor(20, 45);
        od_printf("`bright black` Latency``: `bright white`%s", gLatency);
        od_set_cursor(21, 30);
        od_printf("`bright black`BBSes``: `bright white`%s", bbses);
        od_set_cursor(22, 30);
        od_printf("`bright black`Rooms``: `bright white`%s", rooms);
        od_set_cursor(21, 45);
        od_printf("`bright black`   Users``: `bright white`%s", users);
        od_set_cursor(22, 45);
        od_printf("`bright black`Activity``: `bright white`%s", ACTIVITY[act]);
        
        od_set_cursor(14, 29);
        od_printf("`white`Make a selection `bright black`(`bright blue`C`white`,`bright blue`S`white`,`bright blue`I`white`,`bright blue`Q`bright black`)"); //,`bright blue`B`white`

        switch (od_get_answer("CSITQ")) {
        case 'C':
            enterChat(gUserDataFile);

            // save any changed user setting, e.g.: text color and chat sounds
            saveUser(&user, gUserDataFile);
            break;

        case 'S':
            enterChatterSettings(gUserDataFile);
            break;

        case 'I':

            od_clr_scr();
            displayPipeFile("screens\\help.txt");
            pause();
            break;

        case 'T':
            od_clr_scr();
            od_printf(DIVIDER);
            od_printf("\r\n`` ChatterName:        `bright white`%s``", user.chatterName);
            od_printf("\r\n`` DisplayChatterName: "); od_disp_emu(pipeToAnsi(gDisplayChatterName), true);
            od_printf("\r\n`` FromSite:           `bright white`%s``", gFromSite);
            od_printf("\r\n`` user_num:           `bright white`%d``", od_control.user_num);
            od_printf("\r\n`` user_name:          `bright white`%s``", od_control.user_name);
            od_printf("\r\n`` user_handle:        `bright white`%s``", od_control.user_handle);
            od_printf("\r\n`` user_security:      `bright white`%d``", od_control.user_security);
            od_printf("\r\n`` user_timelimit:     `bright white`%d``", od_control.user_timelimit);
            od_printf("\r\n`` user_ansi:          `bright white`%d``", od_control.user_ansi);
            od_printf("\r\n`` user_screen_length: `bright white`%d``", od_control.user_screen_length);
            od_printf("\r\n`` sysop_name:         `bright white`%s``", od_control.sysop_name);
            od_printf("\r\n`` system_name:        `bright white`%s``", od_control.system_name);
            od_printf("\r\n`` od_maxtime:         `bright white`%d``", od_control.od_maxtime);
            od_printf("\r\n`` od_inactivity:      `bright white`%d``", od_control.od_inactivity);

            //od_printf("\r\n`` od_num_keys:      `bright white`%d``", od_control.od_num_keys);
            //od_printf("\r\n`` baud:               `bright white`%d``", od_control.baud);

            od_printf("\r\n");
            od_printf(DIVIDER);
            od_printf("\r\n\r\n send a screenshot of this info to `bright blue`codefenix@gmail.com``\r\n");

            pause();
            break;

        case 'Q':
            exit = true;
            break;
        }
    }
    	
	od_exit(0, FALSE);
}