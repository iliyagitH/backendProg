import socket
HOST = '127.0.0.1' 
PORT = 65432        
BUFFER_SIZE = 1024

def start_client():
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            s.connect((HOST, PORT))
            print(f"Подключено к серверу {HOST}:{PORT}")

            while True:
                message = input("Введите сообщение (или 'exit'): ")
                if message.lower() == 'exit':
                    break
                
                s.sendall(message.encode('utf-8'))
                data = s.recv(BUFFER_SIZE)
                if not data:
                    break
                
                print(f"Ответ сервера: {data.decode('utf-8')}")
                
        except ConnectionRefusedError:
            print("Ошибка: Сервер не запущен или недоступен.")
        except Exception as e:
            print(f"Произошла ошибка: {e}")

if __name__ == "__main__":
    start_client()