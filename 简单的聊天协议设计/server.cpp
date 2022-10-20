#include<iostream>
#include<string.h>
#include<string>
#include<map>
#include<queue>
#include<WinSock2.h>
#include<WS2tcpip.h>
#pragma comment(lib,"ws2_32.lib")

#include <typeinfo>


#define DEFAULT_BUFLEN 100
#define DEFAULT_PORT 27015

using namespace std;
map<SOCKET, int> user_map;  // 其中的int如果为1则为在线
queue<string> message_queue; /*如果遇到消息并发的情形，需要添加消息队列
                               单开一个处理并发消息转发的线程*/

// 为每一个连接到此端口的用户创建一个线程
DWORD WINAPI handlerRequest(LPVOID lparam)
{
    SOCKET ClientSocket = (SOCKET)(LPVOID)lparam;
    user_map[ClientSocket] = 1;

    char user_name[10];
    strcpy_s(user_name, to_string(ClientSocket).data());
    send(ClientSocket, user_name, 10, 0);

    SYSTEMTIME systime = { 0 };
    GetLocalTime(&systime);
    cout << endl << systime.wYear << "年" << systime.wMonth << "月" << systime.wDay << "日";
    cout << systime.wHour << "时" << systime.wMinute << "分" << systime.wSecond << "秒" << endl;
    cout << "Log：用户--" << ClientSocket << "--加入聊天！" << endl;
    cout << "-----------------------------------------------------" << endl;

    // 循环接受客户端数据
    int recvResult;
    int sendResult;
    int flag = 1; // 控制是否退出该Socket的接受循环
    int judge_flag = 1; // 默认群发
    do {
        char recvBuf[DEFAULT_BUFLEN] = "";
        char sendBuf[DEFAULT_BUFLEN] = "";
        char special_message[DEFAULT_BUFLEN + 50] = "";
        recvResult = recv(ClientSocket, recvBuf, DEFAULT_BUFLEN, 0);
        judge_flag = 1;
        if (recvResult > 0) {
            char special_user_name[10] = "";
            for (int i = 0; i < DEFAULT_BUFLEN; i++) {
                if (recvBuf[i] == '|') {
                    judge_flag = 0;
                }
            }

            // temp_flag为0表明要单独发送，判断是否要单发
            if (judge_flag == 0) {
                int j;
                for (j = 0; j < DEFAULT_BUFLEN; j++) {
                    if (recvBuf[j] == '|') {
                        special_user_name[j] = '\0';
                        for (int z = j + 1; z < DEFAULT_BUFLEN; z++) {
                            special_message[z - j - 1] = recvBuf[z];
                        }
                        strcat_s(special_message, "( from ");
                        strcat_s(special_message, to_string(ClientSocket).data());
                        strcat_s(special_message, ")\0");
                        break;
                    }
                    else {
                        special_user_name[j] = recvBuf[j];
                    }
                }
            }

            strcpy_s(sendBuf, "用户--");
            string ClientID = to_string(ClientSocket);
            strcat_s(sendBuf, ClientID.data()); // data函数直接转换为char*
            strcat_s(sendBuf, "--: ");
            strcat_s(sendBuf, recvBuf);
            // message_queue.push(sendBuf); //这里将消息存储到队列中

            SYSTEMTIME Logtime = { 0 };
            GetLocalTime(&Logtime);
            cout << endl << Logtime.wYear << "年" << Logtime.wMonth << "月" << Logtime.wDay << "日";
            cout << Logtime.wHour << "时" << Logtime.wMinute << "分" << Logtime.wSecond << "秒" << endl;
            cout << "Log：用户--" << ClientSocket << "--的消息：" << recvBuf << endl;
            cout << "-----------------------------------------------------" << endl;

            if (judge_flag == 1) {
                for (auto it : user_map) {
                    if (it.first != ClientSocket && it.second == 1) {
                        sendResult = send(it.first, sendBuf, DEFAULT_BUFLEN, 0);
                        if (sendResult == SOCKET_ERROR)
                            cout << "send failed with error: " << WSAGetLastError() << endl;
                    }
                }
            }
            else {
                for (auto it : user_map) {
                    if (to_string(it.first) == string(special_user_name) && it.second == 1) {
                        sendResult = send(it.first, special_message, DEFAULT_BUFLEN, 0);
                        if (sendResult == SOCKET_ERROR)
                            cout << "send failed with error: " << WSAGetLastError() << endl;
                    }
                }
            }
        }
        else {
            flag = 0;
        }
    } while (recvResult != SOCKET_ERROR && flag != 0);

    GetLocalTime(&systime);
    cout << endl << systime.wYear << "年" << systime.wMonth << "月" << systime.wDay << "日";
    cout << systime.wHour << "时" << systime.wMinute << "分" << systime.wSecond << "秒" << endl;
    cout << "Log：用户--" << ClientSocket << "--离开了聊天（quq）" << endl;
    cout << "-----------------------------------------------------" << endl;

    closesocket(ClientSocket);
    return 0;
}

int main()
{

    //----------------------
    // 初始化Winsock
    WSADATA wsaData;
    int iResult;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }
    //----------------------
    // 创建一个监听的SOCKET
    // 如果有connect的请求就新创建一个线程
    SOCKET ListenSocket;
    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        cout << "socket failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }
    //----------------------
    // 用于bind函数绑定的IP地址和端口号
    sockaddr_in service;
    service.sin_family = AF_INET;
    inet_pton(AF_INET, "10.130.151.45", &service.sin_addr.s_addr);
    service.sin_port = htons(27015);
    iResult = bind(ListenSocket, (SOCKADDR*)&service, sizeof(service));
    if (iResult == SOCKET_ERROR) {
        wprintf(L"bind failed with error: %ld\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    //----------------------
    // 监听即将到来的请求信号
    if (listen(ListenSocket, 5) == SOCKET_ERROR) {
        cout << "listen failed with error: " << WSAGetLastError() << endl;
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    //----------------------
    // 对于每个新的请求使用多线程处理
    cout << "Waiting for client to connect..." << endl;

    while (1) {
        sockaddr_in addrClient;
        int len = sizeof(sockaddr_in);
        // 接受成功返回与client通讯的Socket
        SOCKET AcceptSocket = accept(ListenSocket, (SOCKADDR*)&addrClient, &len);
        if (AcceptSocket == INVALID_SOCKET) {
            cout << "accept failed with error: " << WSAGetLastError() << endl;
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }
        else {
            // 创建线程，并且传入与client通讯的套接字
            HANDLE hThread = CreateThread(NULL, 0, handlerRequest, (LPVOID)AcceptSocket, 0, NULL);
            CloseHandle(hThread); // 关闭对线程的引用
        }
    }

    // 关闭服务端SOCKET
    iResult = closesocket(ListenSocket);
    if (iResult == SOCKET_ERROR) {
        cout << "close failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    WSACleanup();
    return 0;
}
