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
    cout << "欢迎用户 " << ClientSocket << " 加入聊天！" << endl;

    // 循环接受客户端数据
    int recvResult;
    int sendResult;
    int flag = 1;
    do{
        char recvBuf[DEFAULT_BUFLEN] = "";
        char sendBuf[DEFAULT_BUFLEN] = "";
        recvResult = recv(ClientSocket, recvBuf, DEFAULT_BUFLEN, 0);
        if (recvResult > 0) {
            strcpy_s(sendBuf, "user ");
            string ClientID = to_string(ClientSocket);
            strcat_s(sendBuf, ClientID.data()); // data函数直接转换为char*
            strcat_s(sendBuf, ": ");
            strcat_s(sendBuf, recvBuf);
            // message_queue.push(sendBuf); //这里将消息存储到队列中

            cout << ClientSocket << " 说：" << recvBuf << endl;
            for (auto it : user_map) {
                if (it.first != ClientSocket && it.second == 1) {
                    sendResult = send(it.first, sendBuf, DEFAULT_BUFLEN, 0);
                    if(sendResult == SOCKET_ERROR)
                        cout << "send failed with error: " << WSAGetLastError() << endl;
                }
            }
        }
        else {
            flag = 0;
        }
    } while (recvResult != SOCKET_ERROR && flag != 0);

    cout << ClientSocket << " 离开了聊天（quq）" << endl;
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
    inet_pton(AF_INET, "127.0.0.1", &service.sin_addr.s_addr);
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
        else{
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
