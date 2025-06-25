#include "InetPrd.hpp"
#include <Windows.h>
#include <process.h>


InetPrd::InetPrd()
: newDataEvent_(CreateEvent(NULL, TRUE, FALSE, NULL))
{
//    printf("ok HERE #1\n");
    InitializeCriticalSection(&mutex_);
    WSADATA wsaData;
    WSAStartup(MAKEWORD(1, 1), &wsaData);
    udpSock_ = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
struct sockaddr_in addr;
memset(&addr, 0, sizeof(addr));    /* create & zero struct */
addr.sin_family = AF_INET;    /* select internet protocol */
addr.sin_port = htons(12321);         /* set the port # */
addr.sin_addr.s_addr = *(long*)gethostbyname("Athlon5000X2")->h_addr_list[0]; /* set the addr */
//connect(sd, &addr, sizeof(addr));         /* connect! */
    connect(udpSock_, (sockaddr*)&addr, sizeof(addr));
    send(udpSock_, "hello", 5, 0);

    runnerThread_ = (HANDLE)_beginthreadex(NULL, 0, InetPrd::Runner, this, 0, NULL);

//    printf("ok HERE\n");
}


InetPrd::~InetPrd()
{
    WSACleanup();
    DeleteCriticalSection(&mutex_);
}


struct InetDataPack
{
    char id[4];
    unsigned long timeStampHi;
    unsigned long timeStampLo;
    char data;
//    double timeStamp;
//    double data;
};


unsigned __stdcall InetPrd::Runner(void* self)
{
    InetPrd* obj = (InetPrd*)self;

    InetDataPack dataPack;

    while (1)
    {
//        printf("*****");
        recv(obj->udpSock_, (char*)&dataPack, sizeof(dataPack), 0);
//        printf("%s\n", (char*)&dataPack.id);
//        for (int i=0; i<13; ++i)
//            printf("%x ", (unsigned)dataPack.id[i]);
        
        unsigned long long timeStamp = (unsigned long long)(ntohl(dataPack.timeStampHi))*4294967296 + ntohl(dataPack.timeStampLo);
//        printf("%I64u \n", timeStamp);
//        printf("time: %I64\n", dataPack.timeStamp);

        if ((memcmp(dataPack.id, "PSI", 3) == 0) && (timeStamp > obj->timeStamp_))
        {
            EnterCriticalSection(&obj->mutex_);
            obj->timeStamp_ = timeStamp;
            obj->data_ = (int)dataPack.data;//htonl(dataPack.data[0]);
//            printf("*** %i  ***", (int)dataPack.data);

//            obj->data_ = (double)dataPack.data[0];
//            printf("*%i*\n", (int)dataPack.data);
            SetEvent(obj->newDataEvent_);
            LeaveCriticalSection(&obj->mutex_);
            obj->timeStamp_ = obj->timeStamp_;
        }
    }
}


double InetPrd::Generate()
{
    while (WaitForSingleObject(newDataEvent_, INFINITE) != WAIT_OBJECT_0);
    EnterCriticalSection(&mutex_);
    ResetEvent(newDataEvent_);
    
    double retVal = data_;

    LeaveCriticalSection(&mutex_);
    

    return retVal;
}


void InetPrd::ClearBuffers()
{
}


std::string InetPrd::GetId()
{
    return "InetPrd";
}
