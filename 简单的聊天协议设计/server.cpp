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
map<SOCKET, int> user_map;  // ���е�int���Ϊ1��Ϊ����
queue<string> message_queue; /*���������Ϣ���������Σ���Ҫ�����Ϣ����
                               ����һ����������Ϣת�����߳�*/

// Ϊÿһ�����ӵ��˶˿ڵ��û�����һ���߳�
DWORD WINAPI handlerRequest(LPVOID lparam)
{
	SOCKET ClientSocket = (SOCKET)(LPVOID)lparam;
    user_map[ClientSocket] = 1;
    cout << "��ӭ�û� " << ClientSocket << " �������죡" << endl;

    // ѭ�����ܿͻ�������
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
            strcat_s(sendBuf, ClientID.data()); // data����ֱ��ת��Ϊchar*
            strcat_s(sendBuf, ": ");
            strcat_s(sendBuf, recvBuf);
            // message_queue.push(sendBuf); //���ｫ��Ϣ�洢��������

            cout << ClientSocket << " ˵��" << recvBuf << endl;
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

    cout << ClientSocket << " �뿪�����죨quq��" << endl;
	closesocket(ClientSocket);
	return 0;
}

int main()
{

    //----------------------
    // ��ʼ��Winsock
    WSADATA wsaData;
    int iResult;
    iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != NO_ERROR) {
        cout << "WSAStartup failed with error: " << iResult << endl;
        return 1;
    }
    //----------------------
    // ����һ��������SOCKET
    // �����connect��������´���һ���߳�
    SOCKET ListenSocket;
    ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (ListenSocket == INVALID_SOCKET) {
        cout << "socket failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }
    //----------------------
    // ����bind�����󶨵�IP��ַ�Ͷ˿ں�
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
    // �������������������ź�
    if (listen(ListenSocket, 5) == SOCKET_ERROR) {
        cout << "listen failed with error: " << WSAGetLastError() << endl;
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }
    //----------------------
    // ����ÿ���µ�����ʹ�ö��̴߳���
    cout << "Waiting for client to connect..." << endl;

    while (1) {
        sockaddr_in addrClient;
        int len = sizeof(sockaddr_in);
        // ���ܳɹ�������clientͨѶ��Socket
        SOCKET AcceptSocket = accept(ListenSocket, (SOCKADDR*)&addrClient, &len);
        if (AcceptSocket == INVALID_SOCKET) {
            cout << "accept failed with error: " << WSAGetLastError() << endl;
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }
        else{
            // �����̣߳����Ҵ�����clientͨѶ���׽���
            HANDLE hThread = CreateThread(NULL, 0, handlerRequest, (LPVOID)AcceptSocket, 0, NULL);
            CloseHandle(hThread); // �رն��̵߳�����
        }
    }

    // �رշ����SOCKET
    iResult = closesocket(ListenSocket);
    if (iResult == SOCKET_ERROR) {
        cout << "close failed with error: " << WSAGetLastError() << endl;
        WSACleanup();
        return 1;
    }

    WSACleanup();
    return 0;
}
