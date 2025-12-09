/*
 
 uMRC Bridge

 This program maintains a TCP/IP connection between the BBS PC and the MRC
 host. It's responsible for passing message traffic between the MRC host and 
 all uMRC Client instances running on the BBS PC, so it must be running 
 continuously in order to use uMRC Client.

 


 */


#if defined(WIN32) || defined(_MSC_VER)
#define WIN32_LEAN_AND_MEAN

#include <winsock2.h>
#include <ws2tcpip.h>

// Link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

// Link with OpenSSL libraries
#if defined(__x86_64__) || defined(_M_X64)
#pragma comment(lib, "../lib/x64/libssl-43.lib")  // TODO: lots of compiler warnings with 64bit
#pragma comment(lib, "../lib/x64/libcrypto-41.lib")
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#pragma comment(lib, "../lib/x86/libssl-43.lib") 
#pragma comment(lib, "../lib/x86/libcrypto-41.lib")
#endif

#endif

#include "../common/common.h"
#include <sys/timeb.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define OK "|10OK|07\r\n"

const char* ACTIVITY[4] = {
    "NUL",
    "LOW",
    "MED",
    "HI"
};

#pragma region GLOBALS

bool gVerboseLogging = false;
bool gConnectionIsDown = true;
bool gReconnect = false;
int gRetry = 0;
#define MAX_RETRIES 10

SOCKET mrcHostSock = INVALID_SOCKET;
SOCKET clientSocks[MAX_CLIENTS];

struct pClientProc {
    SOCKET pSock;
    int pClientSlot;
};

bool usingSSL = false;
SSL* mrcHostSsl;
SSL_CTX* ctx;

// latency tracking
#define MAX_LATENCIES 50
double gLatency;
struct latencyTracker {
    int packetSum;
    clock_t timeSent;
    bool isServerCmd;
};
struct latencyTracker lt[MAX_LATENCIES];

#pragma endregion

void writeToLog(char* text) {
    char tm_str[32] = "";
    time_t rawtime;
    struct tm timeinfo;

    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);

    _snprintf_s(tm_str, 32, -1, "[%d/%02d/%02d %02d:%02d:%02d] ",
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);

    FILE* logfile;
    fopen_s(&logfile, LOG_FILE, "a");
    if (logfile != NULL) {
        int flag = 0;
        flag = fprintf(logfile, "%s %s\n", tm_str, strReplace(strReplace(text, "\r", ""), "\n", ""));
        fclose(logfile);
    }
}

char* getDateTimeStamp() {
    char tm_str[41] = "";
    time_t rawtime;
    struct tm timeinfo;

    time(&rawtime);
    localtime_s(&timeinfo, &rawtime);

    _snprintf_s(tm_str, 41, -1, "|07[|10%d/%02d/%02d %02d:%02d:%02d|07] ",
        timeinfo.tm_year + 1900,
        timeinfo.tm_mon + 1,
        timeinfo.tm_mday,
        timeinfo.tm_hour,
        timeinfo.tm_min,
        timeinfo.tm_sec);
    return _strdup(tm_str);
}

void printDateTimeStamp() {
    char dt[41] = "";
    strcpy_s(dt, 41, getDateTimeStamp());
    printPipeCodeString( getDateTimeStamp() );
}

/**
 * Returns the sum of the character values in a string.
 * Used for latency tracking, to identify the matching inbound
 * packet for the last outbound packet.
 */
int packetSum(char* str) {
    int sum = 0;
    for (int i = 0; i < (int)strlen(str); i++) {
        sum = sum + str[i];
    }
    return sum;
}

/**
 * Clears all captured structs for latency tracking.
 */
void initializeLt() {
    for (int j = 0; j < MAX_LATENCIES; j++) {
        lt[j].packetSum = -1;
        lt[j].timeSent = -1;
        lt[j].isServerCmd = false;
    }
}

/**
 * Sends SERVER command to the MRC host.
 */ 
bool sendCmdPacket(char* cmd, char* cmdArg) {

    int iResult;
    char cmdstr[MSG_LEN] = "";
    char packet[PACKET_LEN] = "";
    if (strlen(cmdArg) > 0) {
        _snprintf_s(cmdstr, MSG_LEN, -1, cmd, cmdArg);
    }
    else {
        strncpy_s(cmdstr, MSG_LEN, cmd, -1);
    }
    strncpy_s(packet, PACKET_LEN, createPacket("CLIENT", "", "", "SERVER", "", "", cmdstr), -1);

    if (gVerboseLogging) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendCmdPacket \"%s\" to host\r\n", packet);
        printDateTimeStamp();
        printf(logstring);
        writeToLog(logstring);
    }    
    iResult = usingSSL ? SSL_write(mrcHostSsl, packet, (int)strlen(packet)) : send(mrcHostSock, packet, (int)strlen(packet), 0);

    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendCmdPacket failed with error: %d\r\n", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError());
        printDateTimeStamp();        
        printf(logstring);
        writeToLog(logstring);
        return false;
    } else {
        for (int i = 0; i < MAX_LATENCIES; i++) {
            if (lt[i].packetSum == -1) {
                lt[i].packetSum = packetSum(packet);
                lt[i].timeSent = clock();
                lt[i].isServerCmd = true;
                break;
            }
        }
        return true;
    }
}

/**
 * Sends a chat message to the MRC host.
 */ 
bool sendMsgPacket(char* fromUser, char* fromSite, char* fromRoom, char* toUser, char* msgExt, char* toRoom, char* body) {

    int iResult;
    char packet[PACKET_LEN] = "";
    strncpy_s(packet, PACKET_LEN, createPacket(fromUser, fromSite, fromRoom, toUser, msgExt, toRoom, body), -1);
    
    if (gVerboseLogging) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendMsgPacket \"%s\" to host\r\n", packet);
        printDateTimeStamp();
        printf(logstring);
        writeToLog(logstring);
    }
    iResult = usingSSL ? SSL_write(mrcHostSsl, packet, (int)strlen(packet)) : send(mrcHostSock, packet, (int)strlen(packet), 0);

    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendMsgPacket failed with error: %d\r\n", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError());
        printDateTimeStamp();
        printf(logstring);
        writeToLog(logstring);
        return false;
    }
    else {
        for (int i = 0; i < MAX_LATENCIES; i++) {
            if (lt[i].packetSum == -1) {
                lt[i].packetSum = packetSum(packet);
                lt[i].timeSent = clock();
                break;
            }
        }
        return true;
    }
}

/**
 * Sends a packet to the MRC host from local clients.
 */
bool sendHostPacket(char* packet) {

    int iResult;

    if (gVerboseLogging) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendHostPacket \"%s\" to host\r\n", packet);
        printDateTimeStamp();
        printf(logstring);
        writeToLog(logstring);
    }
    iResult = usingSSL ? SSL_write(mrcHostSsl, packet, (int)strlen(packet)) : send(mrcHostSock, packet, (int)strlen(packet), 0);

    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendHostPacket failed with error: %d\r\n", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError());
        printDateTimeStamp();
        printf(logstring);
        writeToLog(logstring);
        return false;
    }
    else {
        for (int i = 0; i < MAX_LATENCIES; i++) {
            if (lt[i].packetSum == -1) {
                lt[i].packetSum = packetSum(packet);
                lt[i].timeSent = clock();
                lt[i].isServerCmd = strstr(packet, "~SERVER~") != NULL && strstr(packet, "~IAMHERE~") == NULL;
                break;
            }
        }
        return true;
    }
}

/**
 * Sends a packet to a specified local client SOCKET.
 */
bool sendClientPacket(SOCKET* sock, char* packet) {
    
    int iResult;
    if (gVerboseLogging) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendClientPacket \"%s\" to socket %d\r\n", packet, *sock);
        printDateTimeStamp();
        printf(logstring);
        writeToLog(logstring);
    }
    iResult = send(*sock, packet, (int)strlen(packet), 0);

    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendClientPacket to client #%d failed with error: %d\r\n", *sock, WSAGetLastError());
        printDateTimeStamp();
        printf(logstring);
        writeToLog(logstring);
        return false;
    }
    else {     
        return true;
    }
}

/**
 * Sends a packet string to all connected local client sockets.
 */
void sendToLocalClients(char* packet) {
    for (int c = 0; c < MAX_CLIENTS; c++) {
        if (clientSocks[c] != INVALID_SOCKET) {
            sendClientPacket(&clientSocks[c], packet);
        }
    }
}

DWORD WINAPI clientProcess(LPVOID lpArg) {

    bool cleanLogoff = false;
    int iResult = 0;
    char thisUser[30] = "";
    struct pClientProc p;
    p = *(struct pClientProc*)lpArg;

    // This process thread is for receiving data from the client on this socket.
    do {
        char clientPacket[PACKET_LEN] = "";
        iResult = recv(p.pSock, clientPacket, PACKET_LEN, 0);
        if (iResult > 0) {

            if (strlen(thisUser) == 0) {
                char** field;
                int fieldCount = split(clientPacket, '~', &field);

                if (fieldCount >= 7) {
                    strncpy_s(thisUser, 30, field[0], -1);
                    printDateTimeStamp();

                    char logstring[100] = "";
                    _snprintf_s(logstring, sizeof(logstring), -1, "%s entered chat (slot #%d occupied).\r\n", thisUser, p.pClientSlot);
                    printf(logstring);
                    if (gVerboseLogging) {
                        writeToLog(logstring);
                    }
                }
            }
                    
            if (strstr(clientPacket, "~LOGOFF~") != 0) {
                cleanLogoff = true;
            }

            // TODO: validate the packet before sending it out (?)
            //       i.e.: only send to intended recipients?
            //             the client handles that, so why bother?

            sendHostPacket(clientPacket);
        }

    } while (iResult > 0);

    // inform the server that the user was disconnected.
    if (strlen(thisUser) > 0 && !cleanLogoff) {
        char disconnMsg[MSG_LEN] = "";
        _snprintf_s(disconnMsg, MSG_LEN, -1, "|08- %s has disconnected.", thisUser);
        sendMsgPacket(thisUser, "", "", "NOTME", "", "", disconnMsg);
    }

    clientSocks[p.pClientSlot] = INVALID_SOCKET;
    printDateTimeStamp();

    char logstring[100] = "";
    _snprintf_s(logstring, sizeof(logstring), -1, "%s has left chat (slot #%d cleared).\r\n", thisUser, p.pClientSlot);
    printf(logstring);
    if (gVerboseLogging) {
        writeToLog(logstring);
    }

    return 0;
}

DWORD WINAPI waitProcess(LPVOID lpArg) {
    WSADATA wsaData;
    SOCKET listenSock = INVALID_SOCKET;
    struct addrinfo* liResult = NULL, * ptrLi = NULL, listener;
    char outboundPacket[PACKET_LEN] = "";
    int iResult; 
    char* port;
    port = *(char**)lpArg;

    HANDLE hClient[MAX_CLIENTS];
    DWORD clientThreadId[MAX_CLIENTS];

    while (gConnectionIsDown) { // Waits for a successful connection before proceeding.
        Sleep(0);
    }

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printDateTimeStamp();
        printf("WSAStartup failed with error: %d\r\n", iResult);
        return 1;
    }

    ZeroMemory(&listener, sizeof(listener));
    listener.ai_family = AF_UNSPEC;
    listener.ai_socktype = SOCK_STREAM;
    listener.ai_protocol = IPPROTO_TCP;
    listener.ai_flags = AI_PASSIVE;
    
    iResult = getaddrinfo("localhost" , port, &listener, &liResult);
    if (iResult != 0) {
        printf("getaddrinfo (clients) failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    listenSock = socket(liResult->ai_family, liResult->ai_socktype, liResult->ai_protocol);
    if (listenSock == INVALID_SOCKET) {
        printf("client socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(liResult);
        WSACleanup();
        return 1;
    }

    iResult = bind(listenSock, liResult->ai_addr, (int)liResult->ai_addrlen);
    if (iResult == SOCKET_ERROR) {       

        char logstring[50] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "client bind failed with error: %d\n", WSAGetLastError());
        printf(logstring);
        writeToLog(logstring);

        freeaddrinfo(liResult);
        closesocket(listenSock);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(liResult);

    printDateTimeStamp();
    printf("Ready for clients.\r\n");
    printDateTimeStamp();
    printf("To exit any time, press CTRL+C.\r\n");
    while ( listen(listenSock, SOMAXCONN) != SOCKET_ERROR /* && !gConnectionIsDown*/) {

        // Accept a client socket, and run it in its own thread
        SOCKET newSock = INVALID_SOCKET;
        newSock = accept(listenSock, NULL, NULL);
        if ((newSock != INVALID_SOCKET) && newSock != SOCKET_ERROR) {

            // Add the client in the first available empty slot.
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clientSocks[i] == INVALID_SOCKET) {
                    clientSocks[i] = newSock;
                    struct pClientProc *pC;
                    pC = (struct pClientProc*)malloc(sizeof(struct pClientProc));
                    pC->pClientSlot = i; // TODO: "Dereferencing NULL pointer" warning
                    pC->pSock = newSock;

                    hClient[i] = CreateThread(NULL, 0, clientProcess, pC, 0, &clientThreadId[i]);
                    break;
                }
            }
        }
    }
    return 0;
}

SSL* performSslHandshake(SOCKET* sock) {
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        // Handle error
        printDateTimeStamp();
        printf("!ctx...\r\n");
        writeToLog("!ctx...");
        return NULL;
    }

    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        // Handle error
        printDateTimeStamp();
        printf("!ssl...\r\n");
        writeToLog("!ssl...");
        SSL_CTX_free(ctx);
        return NULL;
    }

    SSL_set_fd(ssl, (int)*sock); 
    if (SSL_connect(ssl) <= 0) {
        // Handle error
        ERR_print_errors_fp(stderr);
        SSL_free(ssl);
        SSL_CTX_free(ctx);

        printDateTimeStamp();
        printf("SSL_connect failed.\r\n");
        writeToLog("SSL_connect failed");
        return NULL;
    }
    return ssl;
}

void mrcHostProcess(struct settings cfg) {

    WSADATA wsaData;
    struct addrinfo* mhResult = NULL, * ptrMh = NULL, mrcHost;
    int iResult;

    char handshake[PACKET_LEN] = "";
    _snprintf_s(handshake, PACKET_LEN, -1, "%s~%s/%s.%s/%s.%s", cfg.name, cfg.soft, PLATFORM, ARC, PROTOCOL_VERSION, UMRC_VERSION);

    printDateTimeStamp();
    printf("Starting up...\r\n");

    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printDateTimeStamp();
        char logstring[50] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "WSAStartup failed with error: %d\r\n", iResult);
        printf(logstring);
        writeToLog(logstring);
        return;
    }

    ZeroMemory(&mrcHost, sizeof(mrcHost));
    mrcHost.ai_family = AF_UNSPEC;
    mrcHost.ai_socktype = SOCK_STREAM;
    mrcHost.ai_protocol = IPPROTO_TCP;

    printDateTimeStamp();
    printf("Connecting to %s:%s...", cfg.host, cfg.port);
    iResult = getaddrinfo(cfg.host, cfg.port, &mrcHost, &mhResult);
    if (iResult != 0) {
        char logstring[50] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "getaddrinfo failed with error: %d\r\n", iResult);
        printf(logstring);
        writeToLog(logstring);
        WSACleanup();
        return;
    }

    // Attempt to connect to an address until one succeeds
    for (ptrMh = mhResult; ptrMh != NULL; ptrMh = ptrMh->ai_next) {
        mrcHostSock = socket(ptrMh->ai_family, ptrMh->ai_socktype, ptrMh->ai_protocol);
        if (mrcHostSock == INVALID_SOCKET) {            
            char logstring[50] = "";
            _snprintf_s(logstring, sizeof(logstring), -1, "socket failed with error: %d\r\n", WSAGetLastError());
            printf(logstring);
            writeToLog(logstring);
            WSACleanup();
            return;
        }

        iResult = connect(mrcHostSock, ptrMh->ai_addr, (int)ptrMh->ai_addrlen);
        if (iResult == SOCKET_ERROR) {            
            char logstring[50] = "";
            _snprintf_s(logstring, sizeof(logstring), -1, "connect failed with error: %d. ", WSAGetLastError());
            printf(logstring);
            writeToLog(logstring);
            closesocket(mrcHostSock);
            mrcHostSock = INVALID_SOCKET;
            continue;
        }else {
            printPipeCodeString(OK);
        }

        if (cfg.ssl) {
            printDateTimeStamp();
            printf("SSL...");   

            SSL_library_init();
            OpenSSL_add_all_algorithms();
            SSL_load_error_strings();

            mrcHostSsl = performSslHandshake(&mrcHostSock);

            if (mrcHostSsl != NULL) {
                usingSSL = true;
                printPipeCodeString(OK);
                if (gVerboseLogging) {
                    writeToLog("SSL...OK");
                }
            }
            else {
                printf("failed.");   
            }
        }
        break;
    }

    freeaddrinfo(mhResult);

    if (mrcHostSock == INVALID_SOCKET) {
        printf("Unable to connect to server!\r\n");
        writeToLog("Unable to connect to server!");
        WSACleanup();
        return;
    }

    printDateTimeStamp();
    printf("Sending handshake:\"%s\" ... ", handshake);
    
    iResult = usingSSL ? SSL_write(mrcHostSsl, handshake, (int)strlen(handshake)) : send(mrcHostSock, handshake, (int)strlen(handshake), 0);
    
    if (iResult == SOCKET_ERROR) {
        char logstring[50] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "send failed with error: %d\r\n", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError());
        printf(logstring);
        writeToLog(logstring);
        closesocket(mrcHostSock);
        WSACleanup();
        return;
    }
    else {
        printPipeCodeString(OK);
        gConnectionIsDown = false;
        gRetry = 0;
    }   

    char partialPacket[PACKET_LEN] = "";
    // Main loop...
    //
    do {        
        Sleep(0);

        iResult = 0;
        char inboundData[DATA_LEN] = "";
        int bytesread = 0;

        // If a partial packet was captured in the last loop, place it in 
        // front of the next inbound data we read from the host.
        // 
        if (strlen(partialPacket) > 0)
        {
            strcpy_s(inboundData, DATA_LEN, partialPacket);
            bytesread = strlen(partialPacket);
            strcpy_s(partialPacket, sizeof(partialPacket), "");
            printDateTimeStamp();
            //printf("partial packet recovered! \r\n");
        }

        iResult = usingSSL ? SSL_read(mrcHostSsl, inboundData + bytesread, DATA_LEN) : recv(mrcHostSock, inboundData + bytesread, DATA_LEN, 0);
              
        if (gConnectionIsDown) {
            break;
        }
        
        if (iResult > 0) {

            char** packets;
            int pktCount = split(inboundData, '\n', &packets); // inboundData often contains multiple packets
            
            for (int i = 0; i < pktCount; i++) {

                char packet[PACKET_LEN] = "";
                _snprintf_s(packet, PACKET_LEN, -1, "%s\n", packets[i]);

                // the last packet in the array should typically be empty (just a LF).. 
                // if it's not, then consider it a partial packet that needs to be 
                // prefixed to the next inboundData read
                if (pktCount > 1 && i == pktCount - 1 && strcmp("\n", packet) != 0) {
                    printDateTimeStamp();
                    //printf("partial packet detected: \"%s\"\r\n", packet);
                    strcpy_s(partialPacket, sizeof(partialPacket), strReplace(packet, "\n", "")); // strip out the LF
                    break;
                }

                if (strstr(packet, "~") == NULL) {
                    continue;
                }    

                char* fromUser, * fromSite, * fromRoom, * toUser, * msgExt, * toRoom, * body;
                processPacket(packet, &fromUser, &fromSite, &fromRoom, &toUser, &msgExt, &toRoom, &body);        

                for (int i = 0; i < MAX_LATENCIES; i++) {
                    if (lt[i].packetSum == packetSum(packet) || lt[i].isServerCmd && strcmp(fromUser, "SERVER")==0) {
                        gLatency = ((double)clock() - lt[i].timeSent) / CLOCKS_PER_SEC * 1000; // Latency is the difference                    
                        char ltccmd[20] = "";
                        _snprintf_s(ltccmd, 20, -1, "LATENCY:%.0f", gLatency);
                        sendToLocalClients(createPacket("SERVER", "", "", "CLIENT", "", "", ltccmd));
                        initializeLt();
                        break;
                    }
                } 

                if (countOfChars(packet, '~') < 6) { // needed?
                    printDateTimeStamp();
                    printf("partial packet detected: \"%s\"\r\n", packet);
                    continue;
                }  

                if (strlen(body) == 0) {
                    continue;
                }                 

                if (strcmp(fromUser, "SERVER") == 0) {
                    // Server acknowledged our connection, so send the INFO packets.
                    if (strcmp(body, "HELLO") == 0) { 
                        sendCmdPacket("INFOWEB:%s", cfg.web);
                        sendCmdPacket("INFOTEL:%s", cfg.tel);
                        sendCmdPacket("INFOSSH:%s", cfg.ssh);
                        sendCmdPacket("INFOSYS:%s", cfg.sys);
                        sendCmdPacket("INFODSC:%s", cfg.dsc);
                        sendCmdPacket("IMALIVE:%s", cfg.name);
                        char capStr[20] = "";
                        _snprintf_s(capStr, 20, -1, "%s%s%s", "MCI", (cfg.ssl ? " SSL" : ""), " CTCP");
                        sendCmdPacket("CAPABILITIES: %s", capStr);
                        Sleep(20);

                        // Inform the local clients that the reconnect occurred.
                        if (gReconnect) {
                            sendToLocalClients(createPacket("SERVER", "", "", "CLIENT", "", "", "RECONNECT"));

                            // reset the 
                            gReconnect = false;
                        }
                    }
                    // Server is checking whether we're still up, so let it know.
                    else if (_strcmpi(body, "ping") == 0) { 

                        sendCmdPacket("IMALIVE:%s", cfg.name);

                        // As long as we're letting the server know IMALIVE, might as well request stats.
                        sendCmdPacket("STATS", "");                        
                    }
                    else if (strncmp(body, "STATS:", 6) == 0) {

                        int act = 0;
                        char stats[30] = "";
                        strncpy_s(stats, sizeof(stats), body + 6, -1);
                        char* bbses, * rooms, * users, * activity;
                        parseStats(stats, &bbses, &rooms, &users, &activity);
                        act = atoi(activity);

                        FILE* mrcstats;
                        fopen_s(&mrcstats, MRC_STATS_FILE, "w+");
                        if (mrcstats != NULL) {
                            int flag = 0;
                            flag = fprintf(mrcstats, "%s %.0f", stats, gLatency);
                            fclose(mrcstats);
                        }
                    }
                    else {
                        // these are SERVER messages FROM MRC; should go to all active local clients
                        sendToLocalClients(packet);                            
                    }
                }
                else {
                    // these are CHAT messages FROM MRC; should go to all active local clients
                    sendToLocalClients(packet);
                }
            }
        }
        else if (iResult == 0) {
            int errcode = usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError();            
            printDateTimeStamp();
            char logstring[50] = "";
            _snprintf_s(logstring, sizeof(logstring), -1, "Connection closed with error: %d\r\n", errcode);
            printf(logstring);
            writeToLog(logstring);
        }
        else {
            int errcode = usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError();
            printDateTimeStamp();
            char logstring[50] = "";
            _snprintf_s(logstring, sizeof(logstring), -1, "recv failed with error: %d\r\n", errcode);
            printf(logstring);
            writeToLog(logstring);
        }
    } while (iResult > 0 && !gConnectionIsDown);

    if (!gConnectionIsDown) { // Consider it a "reconnect" if the connection was up
        gReconnect = true;
    }    

    gConnectionIsDown = true;

    // cleanup
    shutdown(mrcHostSock, SD_SEND);
    closesocket(mrcHostSock);
    WSACleanup();
    if (usingSSL) {
        EVP_cleanup();
        SSL_shutdown(mrcHostSsl);
        SSL_free(mrcHostSsl);
        SSL_CTX_free(ctx);
    }
}

int main(int argc, char** argv)
{
    HANDLE hClient;
    DWORD clientThreadId;
    struct settings cfg;

    char** clientport;
    clientport = (char**)malloc(sizeof(char*));


#if defined(WIN32) || defined(_MSC_VER)
    hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hCon == INVALID_HANDLE_VALUE) return;
#endif

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clientSocks[i] = INVALID_SOCKET;
    }

    initializeLt();

    // logo time...

    printf("\r\n\r\n|~| \\                          /|\\                           /|~|\r\n");
    printf("| |  |\\                     /|  |  |\\                     /|  | |\r\n");
    printf("| |  |  |\\               /|  |  |  |  |\\               /|  |  | |\r\n");
    printf("| |  |  |  |\\         /|  |  |  |  |  |  |\\         /|  |  |  | |\r\n");
    printf("| |  |  |  |  |\\   /|  |  |  |  |  |  |  |  |\\   /|  |  |  |  | |\r\n");
    printf("| |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  | |\r\n");
    printf("+-+------------===================================------------+-+\r\n");
    printf("|              <    u M R C - B R I D G E v%s   >              |\r\n", UMRC_VERSION);
    printf("+-+------------===================================------------+-+\r\n\r\n");


    for (int i = 0; i < argc; i++) {
        if (_strcmpi(argv[i], "-V") == 0) {
            gVerboseLogging = true;
            printDateTimeStamp();
            printf("Verbose logging ");
            printPipeCodeString(OK);
        }
    }

    if (loadData(&cfg, CONFIG_FILE) != 0) {
        printDateTimeStamp();
        printf("Invalid config. Run Config.exe.\r\n");
        if (gVerboseLogging) {
            writeToLog("Invalid config. Run Config.exe.");
        }
        return -1;
    }

    *clientport = cfg.port; // TODO: "Dereferencing NULL pointer" warning

    // create a new thread for waiting for client connections.
    hClient = CreateThread(NULL, 0, waitProcess, clientport, 0, &clientThreadId);

    while (gRetry < MAX_RETRIES) {
        mrcHostProcess(cfg);
        Sleep(5000);
        gRetry = gRetry + 1;
        printDateTimeStamp();
        char logstring[50] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "Retry %d of %d...\r\n", gRetry, MAX_RETRIES);
        printf(logstring);
        writeToLog(logstring);
    }

    return 0;
}
