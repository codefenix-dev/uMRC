/*

 uMRC Setup

 This is the setup utility that sysops use to configure the MRC host info as 
 well as info about their BBS.

 Sysops must run first before being able to launch uMRC Bridge or uMRC Client.





 */


#include "../common/common.h"

#define NOT_LISTED "Something Else..."
#define BBS_TYPE_COUNT 10
const char* BBS_TYPES[BBS_TYPE_COUNT] = {
	"EleBBS",
	"Mystic",
	"Oblivion/2",
	"PCBoard",
	"ProBoard",
	"Renegade",
	"Synchronet",
	"Talisman",
	"WWIV",
	NOT_LISTED
};


char* textPrompt(char* promptText, int maxLen, int displayableLen, char* defaultValue, bool convertPipeCodes) {
	//clearScreen(); 
	int enteredLen = 0;
	char buffer[75];
	while (enteredLen == 0) {
		printPipeCodeString(promptText);
		if (displayableLen > 0) {
			char limitdisp[140] = "";
			_snprintf_s(limitdisp, 140, -1, "\r\n(max length: %d, displayable length: %d)", maxLen, displayableLen);
			printPipeCodeString(limitdisp);
		}
		if (convertPipeCodes == true) {
			printPipeCodeString("\r\n|07(pipe color codes |15ALLOWED|07)");
		}
		if (strlen(defaultValue) > 0) {
			char defdisp[140] = "";
			_snprintf_s(defdisp, 140, -1, "\r\n |08(Press ENTER for: |15\"%s\"|08)|07", defaultValue);
			printPipeCodeString(defdisp);
		}
		printPipeCodeString("\r\n\r\n> |17 ");
		for (int i = 0; i < maxLen; i++) {
			printf(" ");
		}
		for (int i = 0; i < maxLen; i++) {
			printf("\b");
		}
        fflush(stdin);
		fgets(buffer, sizeof(buffer), stdin);
		buffer[strcspn(buffer, "\r")] = '\0';
		buffer[strcspn(buffer, "\n")] = '\0';
		if (strlen(buffer) == 0) {
			strcpy_s(buffer, sizeof(buffer), defaultValue);
		}
		printPipeCodeString("|07|16");

		for (int i = 1; i <= 255; i++) { // strip out invalid/disallowed characters
			if (i < 32 || i > 125) {
				char f[] = { i, '\0' };
				strcpy_s(buffer, sizeof(buffer), strReplace(buffer, f, ""));
			}
		}
		enteredLen = (int)strlen(buffer);
	}

	return _strdup(buffer);
}

char charPrompt(char* promptText, char allowedChars[], char defaultChar) {

	bool valid = false;
	char input;
	while (!valid) {
		printPipeCodeString(promptText);
		if (defaultChar != '\0') {
			char defdisp[140] = "";
			_snprintf_s(defdisp, 140, -1, "\r\n |08(Press ENTER for: |15\"%c\"|08)|07", defaultChar);
			printPipeCodeString(defdisp);
		}
		printPipeCodeString("\r\n\r\n> |17  \b");
		input = toupper(_getch());
        
        if (input == 13 || input == 10) {
            input = defaultChar;
            break;
        }
        
		for (int i = 0; i < (int)strlen(allowedChars); i++) {
			if (allowedChars[i] == input) {
				valid = true;
				break;
			}
		}
		printPipeCodeString("|07|16");
	}
	printPipeCodeString("|07");
	return input;
}

char* listPrompt(char* promptText, const char* choices[], int choiceCount, char* freeTextPrompt, int size) {

	printPipeCodeString(promptText);
	puts("\r\n");

	char buffer[5];
	int iIpt = 0;
	for (int i = 0; i < choiceCount; i++) {
		char dispOpt[80] = "";
		_snprintf_s(dispOpt, 80, -1, "%2d: %s\r\n", i + 1, choices[i]);
		printPipeCodeString(dispOpt);
	}
	while (iIpt < 1 || iIpt > choiceCount) {
		char dispPrmpt[80] = "";
		_snprintf_s(dispPrmpt, 80, -1,"\r\nMake a selection (%d-%d): |17   \b\b", 1, choiceCount);
		printPipeCodeString(dispPrmpt);
		fgets(buffer, size, stdin);
		iIpt = atoi(buffer);
		printPipeCodeString("|07|16");
	}
	printPipeCodeString("|07");
	char fdbk[50] = "";
	if (strcmp(choices[iIpt-1], NOT_LISTED)==0) { // if option isn't listed, let them type in whatever
		char* rEntered = textPrompt(freeTextPrompt, size, 0, "", false);		
		_snprintf_s(fdbk,50, -1,"|15%s|07 entered.\r\n\r\n",  rEntered);
		printPipeCodeString(fdbk);
		return rEntered;
	} else {
		char* rPicked = (char*)choices[iIpt - 1];
		_snprintf_s(fdbk, 50,-1,"|15%s|07 selected.\r\n\r\n", rPicked);
		printPipeCodeString(fdbk);
		return rPicked;
	}
}

void enterInfo(struct settings info, bool enter_new) {
		
	clearScreen(); 

	strcpy_s(info.host, 80, textPrompt("|07Enter the |15MRC host address|08:|07", 70, 0, enter_new ? DEFAULT_HOST : info.host, false));
	lstr(info.host);
	puts("");
	info.ssl = charPrompt("|07Use |15SSL|08 (|15Y|07/|15N|08)|07:", "YN", (enter_new ? 'Y' : (info.ssl ? 'Y' : 'N'))) == 'Y';

	puts(""); 
	strcpy_s(info.port, 6, textPrompt("|07Enter the |15MRC host port number|08:|07", 6, 0, enter_new ? (info.ssl ? DEFAULT_SSL_PORT : DEFAULT_PORT) : info.port, false));

	// BBS info
	clearScreen(); 
	strcpy_s(info.name, 140, textPrompt("|07Enter your |15BBS name|08:|07", 70, 60, enter_new ? "" : info.name, true));

	clearScreen(); 
	strcpy_s(info.soft, 140, listPrompt("|07Choose your |15BBS software|08:|07", BBS_TYPES, BBS_TYPE_COUNT, "|07Enter your |15BBS software|08:|07", 30));
	ustr(info.soft);
    
	clearScreen(); 
	strcpy_s(info.web, 140, textPrompt("|07If your BBS has a |15website|07, enter it here |08(include either |07http:// |08or |07https://|08):|07", 70, 60, enter_new ? "NONE" : info.web, true));
	
	clearScreen(); 
    strcpy_s(info.tel, 140, textPrompt("|07Enter your BBS |15telnet address|07 (and port number)|08:|07", 70, 60, enter_new ? "" : info.tel, true));
	
	clearScreen(); 
    strcpy_s(info.ssh, 140, textPrompt("|07Enter your BBS |15SSH address|07 (and port number)|08:|07", 70, 60, enter_new ? "NONE" : info.ssh, true));
	
	clearScreen(); 
    strcpy_s(info.sys, 140, textPrompt("|07Enter your |15sysop name|08:|07", 70, 60, enter_new ? "" : info.sys, true));
	
	clearScreen(); 
    strcpy_s(info.dsc, 140, textPrompt("|07Enter a |15description for your BBS|08:|07", 70, 60, enter_new ? "" : info.dsc, true));

	if (saveData(&info, CONFIG_FILE) != -1) {		
		printPipeCodeString("\r\n\r\n|07 BBS info saved |15successfully|07!\r\n\r\nPress any key...");
		int c = _getch();
	}
}

int main()
{
#if defined(WIN32) || defined(_MSC_VER)
	hCon = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hCon == INVALID_HANDLE_VALUE) return;
#endif

	char selection = ' ';

	while (selection != 'Q') {
		bool fullSetup = false;
		struct settings info;
		clearScreen();
		
		char dispStr[140] = "";
		_snprintf_s(dispStr, 140,-1,"|13 uMRC for %s Setup|07\r\n", PLATFORM);
		printPipeCodeString(dispStr);
		printPipeCodeString("|01 ==========================================================================|07\r\n");
		puts("");
		if (loadData(&info, CONFIG_FILE) == 0) {
						
			printPipeCodeString("|10 MRC SERVER INFO|07:\r\n");

			_snprintf_s(dispStr, 140,-1,"|08         Host|07: |09%s|07\r\n", info.host);
			printPipeCodeString(dispStr);
			_snprintf_s(dispStr, 140, -1, "|08         Port|07: |09%s|07\r\n", info.port);
			printPipeCodeString(dispStr);
			_snprintf_s(dispStr, 140, -1, "|08          SSL|07: |09%s|07\r\n", (info.ssl == true ? "|10Yes" : "No"));
			printPipeCodeString(dispStr);


			puts("");
			printPipeCodeString("|10 BBS INFO|07 - |02This is how your BBS appears in the MRC BBS list|07:\r\n");

			_snprintf_s(dispStr, 140, -1, "|08     BBS Name|07: |07%s|07\r\n", strReplace(info.name, "/", ""));
			printPipeCodeString(dispStr);
			_snprintf_s(dispStr, 140, -1, "|08     Platform|07: |07%s / %s.%s / %s.%s|07\r\n", strReplace(info.soft, "/", ""), PLATFORM, ARC, PROTOCOL_VERSION, UMRC_VERSION);
			printPipeCodeString(dispStr);
			_snprintf_s(dispStr, 140, -1, "|08          Web|07: |07%s|07\r\n", info.web);
			printPipeCodeString(dispStr);
			_snprintf_s(dispStr, 140, -1, "|08       Telnet|07: |07%s|07\r\n", info.tel);
			printPipeCodeString(dispStr);
			_snprintf_s(dispStr, 140, -1, "|08          SSH|07: |07%s|07\r\n", info.ssh);
			printPipeCodeString(dispStr);
			_snprintf_s(dispStr, 140, -1, "|08        Sysop|07: |07%s|07\r\n", info.sys);
			printPipeCodeString(dispStr);
			_snprintf_s(dispStr, 140, -1, "|08  Description|07: |07%s|07\r\n", info.dsc);
			printPipeCodeString(dispStr);

			puts("");
			printPipeCodeString("|01 ==========================================================================|07\r\n");

		} else {
			fullSetup = true;
		}
		
		printPipeCodeString(fullSetup == true ? "|15 1|08) |07Setup\r\n" : "|15 1|08) |07Redo setup\r\n");
		printPipeCodeString("|15 Q|08) |07Quit \r\n\r\n");
		puts("Make a selection: ");
		selection = toupper(_getch());
		if (selection == '1') {
			enterInfo(info, fullSetup);
		}
	}
    puts("\r\n");
}
