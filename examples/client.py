import socket

# 1. Настройки сервера, к которому подключаемся
HOST = '127.0.0.1'  # IP-адрес сервера
PORT = 65432        # Порт сервера
BUFFER_SIZE = 1024

def start_client():
    # 1. Создание сокета
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        try:
            # 2. Подключение (connect) к серверу
            s.connect((HOST, PORT))
            print(f"✅ Подключено к серверу {HOST}:{PORT}")

            while True:
                # 3. Отправка данных
                message = input("Введите сообщение (или 'exit'): ")
                if message.lower() == 'exit':
                    break
                
                s.sendall(message.encode('utf-8')) # Кодируем строку в байты перед отправкой

                # 4. Прием ответа от сервера
                data = s.recv(BUFFER_SIZE)
                if not data:
                    break
                
                print(f"⬅️ Ответ сервера: {data.decode('utf-8')}")
                
        except ConnectionRefusedError:
            print("❌ Ошибка: Сервер не запущен или недоступен.")
        except Exception as e:
            print(f"❌ Произошла ошибка: {e}")

if __name__ == "__main__":
    start_client()