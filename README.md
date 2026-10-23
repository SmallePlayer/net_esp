# net_esp — сетевая мини-библиотека для ESP-IDF

Компактная библиотека на C для ESP32 (ESP-IDF), которая объединяет:

- инициализацию Wi-Fi в режиме клиента (**STA**) и точки доступа (**AP**);
- TCP-клиент с автоматическим переподключением;
- UDP-клиент;
- удобные обёртки `send`/`recv` для базовых типов (`int`, `float`, `double`, `char`);
- гарантированный приём ровно `N` байт (`recv_all`), потому что `recv()` может вернуть меньше за один вызов.

Библиотека рассчитана на взаимодействие ESP32 с компьютером (например, с Python- или C++-сервером), который отправляет и принимает бинарные структуры.

Репозиторий содержит только два файла:

```
net.h   — публичный API (объявления функций)
net.c   — реализация (Wi-Fi + TCP/UDP + сокеты)
```

---

## Содержание

- [Требования](#требования)
- [Состав файлов](#состав-файлов)
- [Быстрый старт](#быстрый-старт)
- [Подключение к проекту ESP-IDF](#подключение-к-проекту-esp-idf)
- [Настройка параметров](#настройка-параметров)
- [API: Wi-Fi](#api-wi-fi)
- [API: TCP](#api-tcp)
- [API: UDP](#api-udp)
- [Форматы данных и порядок байт](#форматы-данных-и-порядок-байт)
- [Примеры использования](#примеры-использования)
- [Взаимодействие с хостом](#взаимодействие-с-хостом)
- [Обработка ошибок](#обработка-ошибок)
- [Ограничения и важные замечания](#ограничения-и-важные-замечания)
- [Отладка](#отладка)
- [Лицензия](#лицензия)

---

## Требования

- **ESP-IDF** (проект разрабатывался на `v6.1`; подойдёт и более ранняя версия с `esp_netif`);
- **FreeRTOS** (идёт в составе ESP-IDF);
- компоненты IDF: `esp_wifi`, `nvs_flash`, `esp_netif`, `esp_event`, `lwip`;
- Wi-Fi сеть **2.4 ГГц** (ESP32 не работает в 5 ГГц).

---

## Состав файлов

| Файл | Назначение |
|------|------------|
| `net.h` | Объявления всех функций, подключает системные заголовки сокетов (`sys/socket.h`, `arpa/inet.h`, `unistd.h`). |
| `net.c` | Реализация Wi-Fi (STA/AP), TCP- и UDP-сокетов, обёрток отправки/приёма. |

---

## Быстрый старт

```c
#include "net.h"

void app_main(void)
{
    // 1. Поднимаем Wi-Fi как клиент
    wifi_init_sta("MyWiFi", "MyPassword");

    // 2. Подключаемся к серверу по TCP (IP и порт задаются в net.c)
    int sock = connect_socket_tcp("192.168.1.100", 8090);
    if (sock < 0) {
        // сюда попадём, если 10 попыток подключения не удались
        return;
    }

    // 3. Отправляем и принимаем значения
    double left = 50.0, right = -50.0;
    send_double_tcp(sock, &left);
    send_double_tcp(sock, &right);

    double answer;
    if (recv_double_tcp(sock, &answer) > 0) {
        // используем answer
    }

    close(sock);
}
```

> **Внимание!** `wifi_init_sta()` только запускает подключение и не ждёт получения IP-адреса. Если сразу после неё вызвать `connect_socket_tcp()`, первые попытки могут не пройти — встроенный цикл ретраев (10 × 3 с) это компенсирует.

---

## Подключение к проекту ESP-IDF

Скопируйте `net.c` и `net.h` в папку компонента (обычно `main/`).

В `main/CMakeLists.txt` добавьте исходник и зависимости:

```cmake
idf_component_register(
    SRCS "net.c" "ваш_файл.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES esp_wifi nvs_flash esp_netif esp_event
)
```

Если вы используете UDP/TCP — дополнительных зависимостей не нужно: `lwip` подключается автоматически через `esp_netif`.

Подключение в коде:

```c
#include "net.h"
```

Сборка и прошивка:

```bash
idf.py set-target esp32s3
idf.py build
idf.py -p /dev/cu.usbmodem101 flash monitor
```

---

## Настройка параметров

В начале `net.c` заданы жёстко прописанные константы:

```c
#define WIFI_SSID "kaban2"        // имя сети
#define WIFI_PASS "1234567890"    // пароль
#define HOST_IP   "192.168.10.94" // IP компьютера-сервера
#define HOST_PORT 8090            // порт сервера
```

**Важно:** `create_socket_tcp()` и `create_socket_udp()` берут адрес и порт именно из этих макросов, а **не** из аргументов `connect_socket_tcp()`. Аргументы `host_ip` и `host_port` в текущей реализации игнорируются. Чтобы подключаться к произвольному адресу, замените в `create_socket_tcp()`/`create_socket_udp()`:

```c
.dest.sin_port = htons(HOST_PORT);
inet_pton(AF_INET, HOST_IP, &dest->sin_addr);
```

на значения из аргументов (или передавайте их в `create_socket_*`).

`wifi_init_sta()` и `wifi_init_ap()` используют переданные `ssid`/`password`, поэтому их можно вызывать с любыми значениями.

---

## API: Wi-Fi

### `void wifi_init_sta(const char *ssid, const char *password)`

Инициализирует Wi-Fi в режиме **станции (клиента)**.

Что делает по шагам:

1. `nvs_flash_init()` — хранилище настроек;
2. `esp_netif_init()` — сетевой стек;
3. `esp_event_loop_create_default()` — цикл событий;
4. `esp_netif_create_default_wifi_sta()` — интерфейс станции;
5. `esp_wifi_init()` с конфигурацией по умолчанию;
6. регистрирует обработчик `wifi_event_handler`, который **автоматически переподключается** при старте и при разрыве соединения;
7. задаёт `WIFI_MODE_STA`, SSID и пароль;
8. `esp_wifi_start()`.

Поведение при обрыве: событие `WIFI_EVENT_STA_DISCONNECTED` вызывает `esp_wifi_connect()`, то есть переподключение происходит бесконечно.

### `void wifi_init_ap(const char *ssid, const char *password)`

Инициализирует Wi-Fi в режиме **точки доступа (AP)**.

- SSID и пароль берутся из аргументов;
- максимум подключений: `4`;
- тип шифрования: `WIFI_AUTH_WPA_WPA2_PSK`;
- если пароль пустой (`""`), сеть становится **открытой** (`WIFI_AUTH_OPEN`).

> Не вызывайте `wifi_init_sta()` и `wifi_init_ap()` одновременно в одном приложении: обе функции заново инициализируют `nvs_flash`, `esp_netif` и обработчики событий. Для одновременной работы нужен режим `WIFI_MODE_APSTA` (в этой библиотеке не реализован).

---

## API: TCP

### `int create_socket_tcp(struct sockaddr_in *dest)`

Создаёт TCP-сокет (`SOCK_STREAM`) и заполняет `dest` адресом `HOST_IP:HOST_PORT` из макросов.

Возвращает файловый дескриптор сокета или `-1` при ошибке.

### `int connect_socket_tcp(const char *host_ip, int host_port)`

Создаёт сокет и пытается подключиться к серверу.

- делает до **10 попыток**, между ними пауза **3 секунды**;
- при неудаче закрывает сокет и пробует снова;
- **возвращает сокет только при успехе**. Если все 10 попыток провалились — функция **не возвращает значение** (в конце тела нет `return`). Это неопределённое поведение: формально нужно добавить `return -1;` после цикла. Учитывайте это в своём коде или исправьте библиотеку.

Аргументы `host_ip`/`host_port` сейчас не используются (см. раздел «Настройка параметров»).

### Приём (TCP)

| Функция | Что делает |
|---------|------------|
| `int recv_all(int sock, void *buf, int len)` | Принимает **ровно `len` байт**, повторяя `recv()` в цикле. Возвращает число принятых байт или `-1` при ошибке/закрытии соединения. |
| `int recv_int_tcp(int sock, int *buf)` | `recv_all(sock, buf, sizeof(int))` |
| `int recv_float_tcp(int sock, float *buf)` | `recv_all(sock, buf, sizeof(float))` |
| `int recv_double_tcp(int sock, double *buf)` | `recv_all(sock, buf, sizeof(double))` |
| `int recv_char_tcp(int sock, char *buf)` | `recv_all(sock, buf, sizeof(char))` |

`recv_all` — ключевая функция: TCP является потоком, и один вызов `recv()` может вернуть меньше запрошенного. `recv_all` крутится, пока не наберёт нужное количество байт. При `n <= 0` возвращает `-1` (0 означает, что хост закрыл соединение).

### Отправка (TCP)

| Функция | Что делает |
|---------|------------|
| `int send_int_tcp(int sock, int *buf)` | Отправляет `sizeof(int)` байт |
| `int send_float_tcp(int sock, float *buf)` | Отправляет `sizeof(float)` байт |
| `int send_double_tcp(int sock, double *buf)` | Отправляет `sizeof(double)` байт |
| `int send_char_tcp(int sock, char *buf)` | Отправляет `sizeof(char)` байт |

Все они — тонкие обёртки над `send()`. Возвращают число отправленных байт или `-1` при ошибке. **Не гарантируют** отправку всех байт за один вызов (в отличие от приёма): для маленьких значений (до нескольких КБ) на практике этого достаточно, но для больших структур надёжнее писать свой `send_all`.

### Пример: отправка структуры

```c
typedef struct {
    double left;
    double right;
} Power;

Power p = { .left = 70.0, .right = -40.0 };
send(sock, &p, sizeof(p), 0);   // или дважды send_double_tcp
```

> При передаче структуры по сети помните о выравнивании (`padding`) и порядке байт. ESP32 — little-endian.

---

## API: UDP

UDP не устанавливает соединение, поэтому «клиент» здесь — это просто сокет с заданным адресом назначения. После `connect()` на UDP-сокете можно пользоваться `send()`/`recv()`, что и делает библиотека.

### `int create_socket_udp(struct sockaddr_in *dest)`

Создаёт UDP-сокет (`SOCK_DGRAM`) и заполняет `dest` адресом `HOST_IP:HOST_PORT`.

### `int connect_socket_udp(const char *host_ip, int host_port)`

Аналог TCP-версии: до 10 попыток с паузой 3 секунды, возвращает сокет. Как и в TCP-версии, при полном провале нет `return` (см. замечания).

### Отправка (UDP)

| Функция | Что делает |
|---------|------------|
| `int send_int_udp(int sock, int *buf, struct sockaddr_in dest)` | `sendto(... sizeof(int) ...)` |
| `int send_float_udp(int sock, float *buf, struct sockaddr_in dest)` | `sendto(... sizeof(float) ...)` |
| `int send_double_udp(int sock, double *buf, struct sockaddr_in dest)` | `sendto(... sizeof(double) ...)` |
| `int send_char_udp(int sock, char *buf, struct sockaddr_in dest)` | `sendto(... sizeof(char) ...)` |

Здесь адрес получателя передаётся явно в `dest`, поэтому функции можно использовать и без `connect()`.

### Приём (UDP)

| Функция | Что делает |
|---------|------------|
| `int recv_int_udp(int sock, int *buf, struct sockaddr_in *src)` | `recvfrom(...)`, в `src` записывается адрес отправителя |
| `int recv_float_udp(int sock, float *buf, struct sockaddr_in *src)` | то же для `float` |
| `int recv_double_udp(int sock, double *buf, struct sockaddr_in *src)` | то же для `double` |
| `int recv_char_udp(int sock, char *buf, struct sockaddr_in *src)` | то же для `char` |

`src` может быть `NULL`, если адрес отправителя не нужен... **нет**, в текущей реализации `recvfrom()` всегда получает `src` и `&src_len`, а `src_len = sizeof(*src)` разыменовывает указатель. Передавайте корректный указатель на `struct sockaddr_in`.

> Для UDP нет автоматического «дочитывания» пакета: один вызов `recvfrom()` = один датаграмма. Если датаграмма больше буфера, лишнее отбрасывается.

---

## Форматы данных и порядок байт

- Все `send_*`/`recv_*` работают с **сырыми байтами памяти** переданного значения.
- Порядок байт **не преобразуется**: ESP32 (Xtensa) — little-endian.
- Если сервер на Python, используйте `struct.pack("<...")` / `struct.unpack("<...")`, где `<` — little-endian:

| Тип C | Размер (ESP32) | Python `struct` |
|-------|----------------|-----------------|
| `char` | 1 | `"<b"` |
| `int` | 4 | `"<i"` |
| `float` | 4 | `"<f"` |
| `double` | 8 | `"<d"` |
| структура из двух `double` | 16 | `"<dd"` |

Пример Python-сервера, который отправляет два `double`:

```python
import socket, struct

s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
s.bind(("0.0.0.0", 8090))
s.listen(1)

conn, addr = s.accept()
conn.sendall(struct.pack("<dd", 100.0, -100.0))  # left, right
```

---

## Примеры использования

### 1. STA + TCP-клиент

```c
#include "net.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    wifi_init_sta("kaban2", "1234567890");
    vTaskDelay(pdMS_TO_TICKS(3000));   // дать время получить IP

    int sock = connect_socket_tcp("192.168.10.94", 8090);

    while (1) {
        double cmd[2];
        if (recv_all(sock, cmd, sizeof(cmd)) != sizeof(cmd))
            break;                     // соединение закрыто

        // cmd[0] — левый мотор, cmd[1] — правый
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    close(sock);
}
```

### 2. Отправка телеметрии на сервер

```c
typedef struct {
    float x;
    float y;
    float angle;
} Telemetry;

Telemetry t = { .x = 1.5f, .y = 2.0f, .angle = 90.0f };
send(sock, &t, sizeof(t), 0);
```

### 3. UDP-обмен

```c
struct sockaddr_in dest;
int sock = create_socket_udp(&dest);

double value = 42.0;
send_double_udp(sock, &value, dest);

struct sockaddr_in from;
double incoming;
if (recv_double_udp(sock, &incoming, &from) > 0) {
    // обработали пакет
}
```

### 4. Точка доступа

```c
wifi_init_ap("Robot-AP", "12345678");
// Подключитесь к сети Robot-AP и узнайте IP платы (обычно 192.168.4.1)
```

---

## Взаимодействие с хостом

Типичная схема: **ESP32 — TCP-клиент**, **компьютер — TCP-сервер**.

1. На компьютере запускается сервер на порту `8090`.
2. ESP32 подключается к `HOST_IP`.
3. Сервер отправляет команды (например, два `double` — мощности моторов), ESP32 их принимает.
4. ESP32 может отправлять обратно телеметрию.

Убедитесь, что:

- компьютер и ESP32 в одной подсети;
- в файрволе компьютера открыт порт `8090`;
- `HOST_IP` в `net.c` — это IP компьютера (не `127.0.0.1`).

---

## Обработка ошибок

- `create_socket_*` → `-1` при ошибке.
- `connect_socket_tcp/udp` → сокет при успехе; при неудаче всех попыток поведение неопределено (нет `return -1`).
- `recv_*` TCP → число байт (для `recv_all` это всегда `len`) или `-1`.
- `recv_*` UDP → размер датаграммы или `-1`.
- `send_*` → число отправленных байт или `-1`.

Рекомендуется всегда проверять возвращаемые значения и закрывать сокет при разрыве:

```c
int n = recv_double_tcp(sock, &value);
if (n <= 0) {
    close(sock);
    sock = connect_socket_tcp(HOST_IP, HOST_PORT);  // переподключение
}
```

---

## Ограничения и важные замечания

1. **Жёстко заданные `HOST_IP`/`HOST_PORT`** — `create_socket_*` игнорируют аргументы `connect_socket_*`.
2. **Нет `return` после цикла ретраев** в `connect_socket_tcp`/`connect_socket_udp` — формально UB. Добавьте `return -1;`.
3. **`recv_all` не различает таймаут и закрытие** — любой `n <= 0` даёт `-1`.
4. **TCP-отправка без цикла** — `send_*` может отправить меньше, чем запрошено.
5. **Нет таймаутов на сокетах** — `recv()` может блокироваться бесконечно. Для неблокирующего режима используйте `fcntl`/`setsockopt(SO_RCVTIMEO)`.
6. **Нет проверки ошибок** `nvs_flash_init`, `esp_netif_init`, `esp_wifi_init` и т. д. — при сбое инициализации приложение может вести себя непредсказуемо.
7. **`wifi_init_ap` не задаёт IP-адрес/канал/DNS** — используются значения по умолчанию IDF.
8. **Только IPv4.**
9. **Пароли в исходниках** — не коммитьте реальные учётные данные в публичный репозиторий; вынесите их в `menuconfig`, NVS или отдельный конфиг.

---

## Отладка

Включите подробные логи Wi-Fi и TCP/IP:

```c
esp_log_level_set("wifi", ESP_LOG_DEBUG);
esp_log_level_set("tcpip_adapter", ESP_LOG_DEBUG);
```

Проверьте IP-адрес ESP32:

```c
#include "esp_netif.h"
esp_netif_t *netif = esp_netif_get_handle_from_ifkey("WIFI_STA_DEF");
esp_netif_ip_info_t ip;
esp_netif_get_ip_info(netif, &ip);
ESP_LOGI("net", "IP: " IPSTR, IP2STR(&ip.ip));
```

Монитор порта:

```bash
idf.py -p /dev/cu.usbmodem101 monitor
```

Выход из монитора — `Ctrl-]`.

---

## Лицензия

Лицензия не указана. Если планируете распространять библиотеку, добавьте файл `LICENSE` (например, MIT).
