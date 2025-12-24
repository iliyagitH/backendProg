#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#define PORT 65432
#define BUFFER_SIZE 1024

#define close(x) closesocket(x)

void init_winsock() {
    WSADATA wsa;
    printf("Инициализация Winsock... ");
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        printf("Ошибка. Код ошибки: %d\n", WSAGetLastError());
        exit(EXIT_FAILURE);
    }
    puts("OK.");
}

void cleanup_winsock() {
    WSACleanup();
}

int main(int argc, char const *argv[]) {

    init_winsock();
    
    SOCKET sock = 0; 
    struct sockaddr_in serv_addr;
    char buffer[BUFFER_SIZE] = {0};
    
    char *message_to_send = "Привет от C-клиента!";
    
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("\n Ошибка создания сокета: %d \n", WSAGetLastError());
        cleanup_winsock();
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);
    
    if (inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("\n Недопустимый адрес / Адрес не поддерживается \n");
        close(sock);
        cleanup_winsock();
        return -1;
    }
    
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == SOCKET_ERROR) {
        printf("Ошибка подключения (connect): %d\n", WSAGetLastError());
        close(sock);
        cleanup_winsock();
        return -1;
    }
    
    printf("✅ Подключено к серверу. \n");
    
    send(sock, message_to_send, strlen(message_to_send), 0);
    printf("✉️ Сообщение отправлено: %s\n", message_to_send);
    
    int valread = recv(sock, buffer, BUFFER_SIZE, 0);
    if (valread > 0) {
        printf("⬅️ Ответ сервера: %s\n", buffer);
    }
    
    close(sock);
    cleanup_winsock();
    
    return 0;
}