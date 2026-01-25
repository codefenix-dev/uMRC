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
#include <sys/timeb.h>

#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")

#if defined(__x86_64__) || defined(_M_X64)
#pragma comment(lib, "../lib/x64/ssl.lib") 
#pragma comment(lib, "../lib/x64/crypto.lib")
#elif defined(i386) || defined(__i386__) || defined(__i386) || defined(_M_IX86)
#pragma comment(lib, "../lib/x86/ssl.lib") 
#pragma comment(lib, "../lib/x86/crypto.lib")
#endif

#else

#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
    
typedef uint32_t  DWORD;

#endif

#include "../common/common.h"
#include "openssl/ssl.h"
#include "openssl/err.h"

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
#define DEFAULT_MAX_RETRIES 10

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


#if defined(WIN32) || defined(_MSC_VER)  
clock_t currentTimeMillis() {
    return clock();
#else
int64_t currentTimeMillis() {
  struct timeval time;
  gettimeofday(&time, NULL);
  int64_t s1 = (int64_t)(time.tv_sec) * 1000;
  int64_t s2 = (time.tv_usec / 1000);
  return s1 + s2;
#endif
}

void writeToLog(char* text) {
    char tm_str[32] = "";
    time_t rawtime;
    time(&rawtime);
#if defined(WIN32) || defined(_MSC_VER)  
    struct tm timeinfo; 
    localtime_s(&timeinfo, &rawtime);
#else
    struct tm *timeinfo; 
    timeinfo = localtime(&rawtime);
#endif

    _snprintf_s(tm_str, 32, -1, "[%d/%02d/%02d %02d:%02d:%02d] ",
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
        fprintf(logfile, "%s %s\n", tm_str, strReplace(strReplace(text, "\r", ""), "\n", ""));
        fclose(logfile);
    }
}

char* getDateTimeStamp() {
    char tm_str[41] = "";
    time_t rawtime;
    time(&rawtime);
#if defined(WIN32) || defined(_MSC_VER)  
    struct tm timeinfo; 
    localtime_s(&timeinfo, &rawtime);
#else
    struct tm *timeinfo;
    timeinfo = localtime(&rawtime);
#endif

    _snprintf_s(tm_str, 41, -1, "|07[|10%d/%02d/%02d %02d:%02d:%02d|07] ",
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
    return _strdup(tm_str);
}

void printDateTimeStamp() {
    printPipeCodeString(getDateTimeStamp());
}

/**
 * Returns the sum of the character values in a string.
 * Used for latency tracking, to identify the matching inbound
 * packet for the last outbound packet.
 */
int packetSum(const char* str) {
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
bool sendCmdPacket(const char* cmd, const char* cmdArg) {
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
        _snprintf_s(logstring, sizeof(logstring), -1, "sendCmdPacket \"%s\" to host", packet);
        printDateTimeStamp();
        puts(logstring);
        writeToLog(logstring);
    }    
    iResult = usingSSL ? SSL_write(mrcHostSsl, packet, (int)strlen(packet)) : send(mrcHostSock, packet, (int)strlen(packet), 0);
    Sleep(10);

    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
#if defined(WIN32) || defined(_MSC_VER)  
        _snprintf_s(logstring, sizeof(logstring), -1, "sendCmdPacket failed with error: %d", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError());
#else
        _snprintf_s(logstring, sizeof(logstring), -1, "sendCmdPacket failed with error: %d", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : errno);
#endif
        printDateTimeStamp();        
        puts(logstring);
        writeToLog(logstring);
        return false;
    } else {
        for (int i = 0; i < MAX_LATENCIES; i++) {
            if (lt[i].packetSum == -1) {
                lt[i].packetSum = packetSum(packet);
                lt[i].timeSent = currentTimeMillis();
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
        _snprintf_s(logstring, sizeof(logstring), -1, "sendMsgPacket \"%s\" to host", packet);
        printDateTimeStamp();
        puts(logstring);
        writeToLog(logstring);
    }
    iResult = usingSSL ? SSL_write(mrcHostSsl, packet, (int)strlen(packet)) : send(mrcHostSock, packet, (int)strlen(packet), 0);
    Sleep(10);

    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
#if defined(WIN32) || defined(_MSC_VER)  
        _snprintf_s(logstring, sizeof(logstring), -1, "sendMsgPacket failed with error: %d", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError());
#else
        _snprintf_s(logstring, sizeof(logstring), -1, "sendMsgPacket failed with error: %d", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : errno);
#endif
        printDateTimeStamp();
        puts(logstring);
        writeToLog(logstring);
        return false;
    }
    else {
        for (int i = 0; i < MAX_LATENCIES; i++) {
            if (lt[i].packetSum == -1) {
                lt[i].packetSum = packetSum(packet);
                lt[i].timeSent = currentTimeMillis(); 
                break;
            }
        }
        return true;
    }
}

/**
 * Sends a packet to the MRC host from local clients.
 */
bool sendHostPacket(const char* packet) {
    int iResult;
    if (gVerboseLogging) {
        char logstring[1024] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "sendHostPacket \"%s\" to host", packet);
        printDateTimeStamp();
        puts(logstring);
        writeToLog(logstring);
    }
    iResult = usingSSL ? SSL_write(mrcHostSsl, packet, (int)strlen(packet)) : send(mrcHostSock, packet, (int)strlen(packet), 0);
    Sleep(10);

    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
#if defined(WIN32) || defined(_MSC_VER)  
        _snprintf_s(logstring, sizeof(logstring), -1, "sendHostPacket failed with error: %d", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError());
#else        
        _snprintf_s(logstring, sizeof(logstring), -1, "sendHostPacket failed with error: %d", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : errno);
#endif
        printDateTimeStamp();
        puts(logstring);
        writeToLog(logstring);
        return false;
    }
    else {
        for (int i = 0; i < MAX_LATENCIES; i++) {
            if (lt[i].packetSum == -1) {
                lt[i].packetSum = packetSum(packet);
                lt[i].timeSent = currentTimeMillis();
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
        _snprintf_s(logstring, sizeof(logstring), -1, "sendClientPacket \"%s\" to socket %d", packet, (int)*sock);
        printDateTimeStamp();
        puts(logstring);
        writeToLog(logstring);
    }
    iResult = send(*sock, packet, (int)strlen(packet), 0);
    Sleep(5);

    if (iResult == SOCKET_ERROR) {
        char logstring[1024] = "";
#if defined(WIN32) || defined(_MSC_VER) 
        _snprintf_s(logstring, sizeof(logstring), -1, "sendClientPacket to client #%d failed with error: %d", (int)*sock, WSAGetLastError());
#else        
        _snprintf_s(logstring, sizeof(logstring), -1, "sendClientPacket to client #%d failed with error: %d", *sock, errno);
#endif
        printDateTimeStamp();
        puts(logstring);
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

#if defined(WIN32) || defined(_MSC_VER)    
DWORD WINAPI clientProcess(LPVOID lpArg) {
#else
void* clientProcess(void* lpArg) {
#endif
    
    bool cleanLogoff = false;
    SOCKET pSock;
    int pSlot;
    int iResult = 0;
    char thisUser[30] = "";
#if defined(WIN32) || defined(_MSC_VER)    
    struct pClientProc p;
    p = *(struct pClientProc*)lpArg;
    pSock = p.pSock;
    pSlot = p.pClientSlot;
#else   
    struct pClientProc *p = lpArg;
    pSock = p->pSock;
    pSlot = p->pClientSlot;
#endif

    // This process thread is for receiving data from the client on this socket.
    do {
        char clientPacket[PACKET_LEN] = "";
        iResult = recv(pSock, clientPacket, PACKET_LEN, 0);
        if (iResult > 0) {

            if (strlen(thisUser) == 0) {
                char** field;
                int fieldCount = split(clientPacket, '~', &field);

                if (fieldCount >= 7) {
                    strncpy_s(thisUser, 30, field[0], -1);
                    printDateTimeStamp();

                    char logstring[100] = "";
                    _snprintf_s(logstring, sizeof(logstring), -1, "%s entered chat (slot #%d occupied).", thisUser, pSlot);
                    puts(logstring);
                    if (gVerboseLogging) {
                        writeToLog(logstring);
                    }
                }
            }
                    
            if (strstr(clientPacket, "~LOGOFF~") != 0) {
                cleanLogoff = true;
            }

            sendHostPacket(clientPacket);
        }

    } while (iResult > 0);

    // inform the server that the user was disconnected.
    if (strlen(thisUser) > 0 && !cleanLogoff) {
        char disconnMsg[MSG_LEN] = "";
        _snprintf_s(disconnMsg, MSG_LEN, -1, "|08- %s has disconnected.", thisUser);
        sendMsgPacket(thisUser, "", "", "NOTME", "", "", disconnMsg);
    }

    clientSocks[pSlot] = INVALID_SOCKET;
    printDateTimeStamp();

    char logstring[100] = "";
    _snprintf_s(logstring, sizeof(logstring), -1, "%s has left chat (slot #%d cleared).", thisUser, pSlot);
    puts(logstring);
    if (gVerboseLogging) {
        writeToLog(logstring);
    }

    return 0;
}

#if defined(WIN32) || defined(_MSC_VER)    
DWORD WINAPI waitProcess(LPVOID lpArg) {
    WSADATA wsaData;
#else
void* waitProcess(void* lpArg) {
#endif

    SOCKET listenSock = INVALID_SOCKET;
    struct addrinfo* liResult = NULL, * ptrLi = NULL, listener;
    char outboundPacket[PACKET_LEN] = "";
    int iResult; 
    char* port;
#if defined(WIN32) || defined(_MSC_VER)  
    HANDLE hClient[MAX_CLIENTS];  
    port = *(char**)lpArg;
#else
    pthread_t hClient[MAX_CLIENTS];  
    port = (char*)lpArg;
#endif

    DWORD clientThreadId[MAX_CLIENTS];

    while (gConnectionIsDown) { // Waits for a successful connection before proceeding.    
        Sleep(0);
    }

#if defined(WIN32) || defined(_MSC_VER)    
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printDateTimeStamp();
        printf("WSAStartup failed with error: %d\r\n", iResult);
        return 1;
    }
	
    ZeroMemory(&listener, sizeof(listener));
#else
    memset(&listener, 0, sizeof listener);
#endif

    listener.ai_family = AF_UNSPEC;
    listener.ai_socktype = SOCK_STREAM;
    listener.ai_protocol = IPPROTO_TCP;
    listener.ai_flags = AI_PASSIVE;
    
    iResult = getaddrinfo("localhost" , port, &listener, &liResult);
    if (iResult != 0) {
        printf("getaddrinfo (clients) failed with error: %d\n", iResult);
#if defined(WIN32) || defined(_MSC_VER)  
        WSACleanup();
        return 1;
#else
		return (void*)1;
#endif
    }

    listenSock = socket(liResult->ai_family, liResult->ai_socktype, liResult->ai_protocol);
    if (listenSock == INVALID_SOCKET) {
#if defined(WIN32) || defined(_MSC_VER)  
        printf("client socket failed with error: %ld\n", WSAGetLastError());
#else
        printf("client socket failed with error: %d\n", errno);
#endif
        freeaddrinfo(liResult);
#if defined(WIN32) || defined(_MSC_VER)  
        WSACleanup();
        return 1;
#else
		return (void*)1;
#endif
    }

    iResult = bind(listenSock, liResult->ai_addr, (int)liResult->ai_addrlen);
    if (iResult == SOCKET_ERROR) {       

        char logstring[50] = "";
#if defined(WIN32) || defined(_MSC_VER)  
        _snprintf_s(logstring, sizeof(logstring), -1, "client bind failed with error: %d", WSAGetLastError());
#else
        _snprintf_s(logstring, sizeof(logstring), -1, "client bind failed with error: %d", errno);
#endif        
        puts(logstring);
        writeToLog(logstring);

        freeaddrinfo(liResult);
#if defined(WIN32) || defined(_MSC_VER)    
        closesocket(listenSock);
        WSACleanup();
        return 1;
#else
        close(listenSock);
		return (void*)1;
#endif        
    }

    freeaddrinfo(liResult);

    printDateTimeStamp();
    puts("Ready for clients.");
    printDateTimeStamp();
    puts("To exit any time, press CTRL+C.");
    while (listen(listenSock, SOMAXCONN) != SOCKET_ERROR) {

        // Accept a client socket, and run it in its own thread
        SOCKET newSock = INVALID_SOCKET;
        newSock = accept(listenSock, NULL, NULL);
        if ((newSock != INVALID_SOCKET) && newSock != SOCKET_ERROR) {
            
            // Add the client in the first available empty slot.
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (clientSocks[i] == INVALID_SOCKET) {
                    clientSocks[i] = newSock;
                    struct pClientProc *pC;

#if defined(WIN32) || defined(_MSC_VER)    
                    pC = (struct pClientProc*)malloc(sizeof(struct pClientProc));
                    pC->pClientSlot = i; // TODO: "Dereferencing NULL pointer" warning
                    pC->pSock = newSock;
                    hClient[i] = CreateThread(NULL, 0, clientProcess, pC, 0, &clientThreadId[i]);
#else
                    pC = malloc(sizeof *pC);
                    pC->pClientSlot = i;
                    pC->pSock = newSock;
                    pthread_create(&hClient[i], NULL, clientProcess, (void*)pC);
#endif   
                    break;
                }
            }
        }
    }
#if defined(WIN32) || defined(_MSC_VER)  
    return 0;
#else
	return (void*)0;
#endif   
}

SSL* performSslHandshake(SOCKET* sock) {
    ctx = SSL_CTX_new(TLS_client_method());
    if (!ctx) {
        // Handle error
        printDateTimeStamp();
        puts("!ctx...");
        writeToLog("!ctx...");
        return NULL;
    }

    SSL* ssl = SSL_new(ctx);
    if (!ssl) {
        // Handle error
        printDateTimeStamp();
        puts("!ssl...");
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
        puts("SSL_connect failed.");
        writeToLog("SSL_connect failed");
        return NULL;
    }
    return ssl;
}

void mrcHostProcess(struct settings cfg) {
#if defined(WIN32) || defined(_MSC_VER)    
    WSADATA wsaData;
#endif    
    struct addrinfo* mhResult = NULL, * ptrMh = NULL, mrcHost;
    int iResult;

    char handshake[PACKET_LEN] = "";
    _snprintf_s(handshake, PACKET_LEN, -1, "%s~%s/%s.%s/%s.%s", cfg.name, cfg.soft, PLATFORM, ARC, PROTOCOL_VERSION, UMRC_VERSION);

    printDateTimeStamp();
    puts("Starting up...");

#if defined(WIN32) || defined(_MSC_VER)    
   iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printDateTimeStamp();
        char logstring[50] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "WSAStartup failed with error: %d", iResult);
        puts(logstring);
        writeToLog(logstring);
        return;
    }	
    ZeroMemory(&mrcHost, sizeof(mrcHost));
#else
    memset(&mrcHost, 0, sizeof mrcHost);
#endif
    mrcHost.ai_family = AF_UNSPEC;
    mrcHost.ai_socktype = SOCK_STREAM;
    mrcHost.ai_protocol = IPPROTO_TCP;

    printDateTimeStamp();
    printf("Connecting to %s:%s...", cfg.host, cfg.port);
    iResult = getaddrinfo(cfg.host, cfg.port, &mrcHost, &mhResult);
    if (iResult != 0) {
        char logstring[50] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "getaddrinfo failed with error: %d", iResult);
        puts(logstring);
        writeToLog(logstring);
#if defined(WIN32) || defined(_MSC_VER)
        WSACleanup();
#endif
        return;
    }

    // Attempt to connect to an address until one succeeds
    for (ptrMh = mhResult; ptrMh != NULL; ptrMh = ptrMh->ai_next) {
        mrcHostSock = socket(ptrMh->ai_family, ptrMh->ai_socktype, ptrMh->ai_protocol);
        if (mrcHostSock == INVALID_SOCKET) {            
            char logstring[50] = "";
#if defined(WIN32) || defined(_MSC_VER)  
            _snprintf_s(logstring, sizeof(logstring), -1, "socket failed with error: %d", WSAGetLastError());
#else
            _snprintf_s(logstring, sizeof(logstring), -1, "socket failed with error: %d", errno);
#endif            
            puts(logstring);
            writeToLog(logstring);
#if defined(WIN32) || defined(_MSC_VER)    
            WSACleanup();
#endif            
            return;
        }

        iResult = connect(mrcHostSock, ptrMh->ai_addr, (int)ptrMh->ai_addrlen);
        if (iResult == SOCKET_ERROR) {            
            char logstring[50] = "";
#if defined(WIN32) || defined(_MSC_VER)  
            closesocket(mrcHostSock);
            _snprintf_s(logstring, sizeof(logstring), -1, "connect failed with error: %d. ", WSAGetLastError());
#else
            close(mrcHostSock);
            _snprintf_s(logstring, sizeof(logstring), -1, "connect failed with error: %d. ", errno);
#endif            
            puts(logstring);
            writeToLog(logstring);
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
                puts("failed.");   
            }
        }
        break;
    }

    freeaddrinfo(mhResult);

    if (mrcHostSock == INVALID_SOCKET) {
        puts("Unable to connect to server!");
        writeToLog("Unable to connect to server!");
#if defined(WIN32) || defined(_MSC_VER)
        WSACleanup();
#endif
        return;
    }

    printDateTimeStamp();
    printf("Sending handshake:\"%s\" ... ", handshake);
    
    iResult = usingSSL ? SSL_write(mrcHostSsl, handshake, (int)strlen(handshake)) : send(mrcHostSock, handshake, (int)strlen(handshake), 0);
    
    if (iResult == SOCKET_ERROR) {
        char logstring[50] = "";
#if defined(WIN32) || defined(_MSC_VER)  
        closesocket(mrcHostSock);
        _snprintf_s(logstring, sizeof(logstring), -1, "send failed with error: %d\r\n", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError());
#else        
        close(mrcHostSock);
        _snprintf_s(logstring, sizeof(logstring), -1, "send failed with error: %d\r\n", usingSSL ? SSL_get_error(mrcHostSsl, iResult) : errno);
#endif        
        puts(logstring);
        writeToLog(logstring);
#if defined(WIN32) || defined(_MSC_VER)    
        WSACleanup();
#endif        
        return;
    }
    else {
        printPipeCodeString(OK);
        gConnectionIsDown = false;
        if (gRetry > 0) {
            writeToLog("Reconnection successful");
        }
        gRetry = 0;
    }   

    char partialPacket[PACKET_LEN] = "";
    // Main loop...
    //
    do {             
        Sleep(1);

        iResult = 0;
        char inboundData[DATA_LEN] = "";
        size_t bytesread = 0;

        // If a partial packet was captured in the last loop, place it in 
        // front of the next inbound data we read from the host.
        // 
        if (strlen(partialPacket) > 0)
        {
            strcpy_s(inboundData, DATA_LEN, partialPacket);
            bytesread = strlen(partialPacket);
            if (gVerboseLogging) {
                printDateTimeStamp();
                printf("partial packet recovered: %s\r\n", partialPacket);
                writeToLog("partial packet recovered:");
                writeToLog(partialPacket);
            }
            strcpy_s(partialPacket, sizeof(partialPacket), "");
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
                    if (gVerboseLogging) {
                        printDateTimeStamp();
                        printf("partial packet detected: \"%s\"\r\n", packet);
                        writeToLog("partial packet detected:");
                        writeToLog(packet);
                    }
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
                        gLatency = currentTimeMillis() - lt[i].timeSent;      
                        char ltccmd[20] = "";
                        _snprintf_s(ltccmd, 20, -1, "LATENCY:%.0f", gLatency);
                        sendToLocalClients(createPacket("SERVER", "", "", "CLIENT", "", "", ltccmd));
                        initializeLt();
                        break;
                    }
                } 

                if (countOfChars(packet, '~') < 6) { // invalid packets have fewer than 6 tildes
                    if (gVerboseLogging) {
                        printDateTimeStamp();
                        printf("WARNING: invalid packet: \"%s\"\r\n", packet);
                    }
                    writeToLog("WARNING: invalid packet:");
                    writeToLog(packet);
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
                            gReconnect = false;
                        }
                    }
                    // Server is checking whether we're still up, so let it know.
                    else if (_stricmp(body, "ping") == 0) { 
                        sendCmdPacket("IMALIVE:%s", cfg.name);                        
                        sendCmdPacket("STATS", ""); // As long as we're letting the server know IMALIVE, might as well request stats.             
                    }
                    else if (strncmp(body, "STATS:", 6) == 0) {
                        int act = 0;
                        char stats[30] = "";
                        strncpy_s(stats, sizeof(stats), body + 6, -1);
                        char* bbses, * rooms, * users, * activity;
                        parseStats(stats, &bbses, &rooms, &users, &activity);
                        act = atoi(activity);

                        FILE* mrcstats;
#if defined(WIN32) || defined(_MSC_VER)
                        fopen_s(&mrcstats, MRC_STATS_FILE, "w+");
#else
                        mrcstats=fopen(MRC_STATS_FILE, "w+");
#endif
                        if (mrcstats != NULL) {
                            fprintf(mrcstats, "%s %.0f", stats, gLatency);
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
#if defined(WIN32) || defined(_MSC_VER)  
            int errcode = usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError();            
#else
            int errcode = usingSSL ? SSL_get_error(mrcHostSsl, iResult) : errno;            
#endif
            printDateTimeStamp();
            char logstring[50] = "";
            _snprintf_s(logstring, sizeof(logstring), -1, "Connection closed with error: %d", errcode);
            puts(logstring);
            writeToLog(logstring);
			if (usingSSL) {
				unsigned long err_code = ERR_get_error();
				char err_msg[256];
                ERR_error_string_n(err_code, err_msg, sizeof(err_msg));
                printDateTimeStamp();
                puts(err_msg);
				ERR_clear_error();
			}
        }
        else {
#if defined(WIN32) || defined(_MSC_VER)  
            int errcode = usingSSL ? SSL_get_error(mrcHostSsl, iResult) : WSAGetLastError();            
#else
            int errcode = usingSSL ? SSL_get_error(mrcHostSsl, iResult) : errno;            
#endif
            printDateTimeStamp();
            char logstring[50] = "";
            _snprintf_s(logstring, sizeof(logstring), -1, "recv failed with error: %d", errcode);
            puts(logstring);
            writeToLog(logstring);
			if (usingSSL) {
				unsigned long err_code = ERR_get_error();
				char err_msg[256];
                ERR_error_string_n(err_code, err_msg, sizeof(err_msg));
				printDateTimeStamp();
				puts(err_msg);				
				ERR_clear_error();
			}
        }
    } while (iResult > 0 && !gConnectionIsDown);

    if (!gConnectionIsDown) { // Consider it a "reconnect" if the connection was up
        gReconnect = true;
    }    

    gConnectionIsDown = true;

    // cleanup
#if defined(WIN32) || defined(_MSC_VER)    
    shutdown(mrcHostSock, SD_SEND);
    closesocket(mrcHostSock);
    WSACleanup();
#else
    shutdown(mrcHostSock, SHUT_WR);
    close(mrcHostSock);
#endif    
    if (usingSSL) {
        EVP_cleanup();
        SSL_shutdown(mrcHostSsl);
        SSL_free(mrcHostSsl);
        SSL_CTX_free(ctx);
    }
}

int main(int argc, char** argv)
{
    int maxRetries = DEFAULT_MAX_RETRIES;
    struct settings cfg;
    char* clientport;

#if defined(WIN32) || defined(_MSC_VER)
    HANDLE hClient;
    DWORD clientThreadId;
    hCon = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hCon == INVALID_HANDLE_VALUE) return;
#else 
    pthread_t hClient;
#endif

    for (int i = 0; i < MAX_CLIENTS; i++) {
        clientSocks[i] = INVALID_SOCKET;
    }

    initializeLt();

    // logo time...

    puts("\r\n\r\n|~|\\                           /|\\                           /|~|");
    puts("| |  |\\                     /|  |  |\\                     /|  | |");
    puts("| |  |  |\\               /|  |  |  |  |\\               /|  |  | |");
    puts("| |  |  |  |\\         /|  |  |  |  |  |  |\\         /|  |  |  | |");
    puts("| |  |  |  |  |\\   /|  |  |  |  |  |  |  |  |\\   /|  |  |  |  | |");
    puts("| |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  |  | |");
    puts("+-+------------===================================------------+-+");
    printf("|              <    u M R C - B R I D G E v%s   >              |\r\n", UMRC_VERSION);
    puts("+-+------------===================================------------+-+");

    for (int i = 0; i < argc; i++) {

        if (_strnicmp(argv[i], "-?", 2) == 0) {
            puts("\r\nuMRC-Bridge options:\r\n");
            puts("-V     Enable verbose logging. Display and log all packet strings.");
            puts("-R[n]  Maximum connection retry attempts. Default=10. 0=Infinite.");
            return 0;
        }

        else if (_stricmp(argv[i], "-V") == 0) {
            gVerboseLogging = true;
            printDateTimeStamp();
            puts("Verbose logging ");
            printPipeCodeString(OK);
        }

        else if(_strnicmp(argv[i], "-R", 2) == 0) {
            maxRetries = atoi(argv[i] + 2);
            printDateTimeStamp();
            if (maxRetries == 0) {
                puts("Max connection retries: Infinite");
            }
            else {
                printf("Max connection retries: %d\r\n", maxRetries);
            }
        } 
    }
    
    if (loadData(&cfg, CONFIG_FILE) != 0) {
        printDateTimeStamp();
        puts("Invalid config. Run setup.");
        if (gVerboseLogging) {
            writeToLog("Invalid config. Run setup.");
        }
        return -1;
    }

    // create a new thread for waiting for client connections.
    clientport = cfg.port;
#if defined(WIN32) || defined(_MSC_VER)
    hClient = CreateThread(NULL, 0, waitProcess, &clientport, 0, &clientThreadId);
#else
    pthread_create(&hClient, NULL, waitProcess, (void*)clientport);
#endif
    
    while (maxRetries > 0 ? gRetry < maxRetries : true) {
        mrcHostProcess(cfg);    
        Sleep(5000);
        gRetry = gRetry + 1;
        printDateTimeStamp();
        char logstring[50] = "";
        _snprintf_s(logstring, sizeof(logstring), -1, "Retry %d of %d...\r\n", gRetry, maxRetries);
        puts(logstring);
        writeToLog(logstring);
    }

    return 0;
}
