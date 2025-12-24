import socket
import threading

HOST = '127.0.0.1'  
PORT = 65432        
BUFFER_SIZE = 1024

def handle_client(conn, addr):
    """Функция обработки общения с клиентом в отдельном потоке"""
    print(f"Соединение установлено: {addr}")
    
    with conn:
        while True:
            data = conn.recv(BUFFER_SIZE)
            if not data:
                break
            message = data.decode('utf-8') 
            print(f"Получено от {addr}: {message}")
            response = f"Сервер получил: {message.upper()}"
            conn.sendall(response.encode('utf-8'))

    print(f"Соединение закрыто: {addr}")

def start_server():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        s.bind((HOST, PORT))
        print(f"Сервер запущен на {HOST}:{PORT}")
        s.listen(5)
        print("Ожидание входящих подключений (listen)...")
        
        while True:
            conn, addr = s.accept()
            client_thread = threading.Thread(target=handle_client, args=(conn, addr))
            client_thread.start()

if __name__ == "__main__":
    start_server()