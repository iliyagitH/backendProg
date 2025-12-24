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
    
    SOCKET server_fd, new_socket; 
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    char buffer[BUFFER_SIZE] = {0};
    
    int opt = 1; 

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        perror("Ошибка создания сокета");
        cleanup_winsock();
        exit(EXIT_FAILURE);
    }
    
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, (const char*)&opt, sizeof(opt))) {
        perror("Ошибка setsockopt(SO_REUSEADDR)");
        cleanup_winsock();
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; 
    address.sin_port = htons(PORT); 
    
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Ошибка привязки (bind)");
        cleanup_winsock();
        exit(EXIT_FAILURE);
    }
    
    if (listen(server_fd, 5) < 0) {
        perror("Ошибка прослушивания (listen)");
        cleanup_winsock();
        exit(EXIT_FAILURE);
    }
    
    printf("Сервер запущен. Ожидание подключений на порту %d...\n", PORT);
    
    if ((new_socket = accept(server_fd, (struct sockaddr *)&address, &addrlen)) == INVALID_SOCKET) {
        perror("Ошибка принятия соединения (accept)");
        cleanup_winsock();
        exit(EXIT_FAILURE);
    }
    
    printf("Соединение установлено.\n");
    
    int valread = recv(new_socket, buffer, BUFFER_SIZE, 0);
    if (valread > 0) {
        printf("Получено сообщение: %s\n", buffer);
        
        char *hello = "Сервер получил ваше сообщение!";
        send(new_socket, hello, strlen(hello), 0);
        printf("Ответ клиенту отправлен.\n");
    }
    
    close(new_socket);
    close(server_fd);
    cleanup_winsock(); 
    
    return 0;
}