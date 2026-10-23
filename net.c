//source /Users/pes/.espressif/tools/activate_idf_v6.1.sh
//idf.py -p /dev/cu.usbmodem101 flash monitor

#include <string.h>          // работа со строками и памятью
#include <unistd.h>          // close() для сокета
#include <sys/socket.h>      // socket(), connect(), recv()
#include <arpa/inet.h>       // htons(), inet_pton(), sockaddr_in
#include "freertos/FreeRTOS.h"   // базовые типы FreeRTOS
#include "freertos/task.h"       // vTaskDelay (паузы задачи)
#include "esp_wifi.h"            // функции драйвера WiFi
#include "esp_event.h"           // система событий (подключение/отключение)
#include "esp_log.h"             // ESP_LOGI (логирование)
#include "nvs_flash.h"           // инициализация NVS-хранилища
#include "esp_netif.h"           // сетевой стек (TCP/IP поверх WiFi)

#include "net.h"




#define WIFI_SSID "kaban2"     // имя Wi-Fi сети (2.4 ГГц)
#define WIFI_PASS "1234567890"        // пароль
#define HOST_IP "192.168.10.94"    // IP компьютера (хоста)
#define HOST_PORT 8090                // порт подключения


// обработчик событий WiFi
static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                               int32_t event_id, void* event_data)
{
    // при старте и при отключении — подключаемся заново
    if (event_base == WIFI_EVENT &&
        (event_id == WIFI_EVENT_STA_START || event_id == WIFI_EVENT_STA_DISCONNECTED))
        esp_wifi_connect();
}

// инициализация WiFi в режиме станции (клиента)
void wifi_init_sta(const char *ssid, const char *password)
{
    nvs_flash_init(); 
    esp_netif_init();                       // инициализация сетевого стека
    esp_event_loop_create_default();        // цикл обработки событий
    esp_netif_create_default_wifi_sta();    // интерфейс "станция"

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();  // конфиг по умолчанию
    esp_wifi_init(&cfg);                    // инициализация драйвера WiFi

    // регистрируем обработчик на все события WiFi
    esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);

    wifi_config_t wifi_config = {0};
    strncpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    strncpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    esp_wifi_set_mode(WIFI_MODE_STA);       // режим клиента
    esp_wifi_set_config(WIFI_IF_STA, &wifi_config);  // задаём SSID и пароль
    esp_wifi_start();                       // запускаем WiFi
}

//инициализация WiFi в режиме станции (сервера)
void wifi_init_ap(const char *ssid, const char *password)
{
    nvs_flash_init();
    esp_netif_init();                       // инициализация сетевого стека
    esp_event_loop_create_default();        // цикл обработки событий
    esp_netif_create_default_wifi_ap();     // интерфейс "точка доступа"
    wifi_config_t wifi_config = {
        .ap = {
            .ssid_len = strlen(ssid),
            .max_connection = 4,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        },
    };
    strncpy((char *)wifi_config.ap.ssid, ssid, sizeof(wifi_config.ap.ssid));
    strncpy((char *)wifi_config.ap.password, password, sizeof(wifi_config.ap.password));
    if (strlen(password) == 0) {
        wifi_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();  // конфиг по умолчанию
    esp_wifi_init(&cfg);                    // инициализация драйвера WiFi
    esp_wifi_set_mode(WIFI_MODE_AP);        // режим точки доступа
    esp_wifi_set_config(WIFI_IF_AP, &wifi_config);  // задаём SSID и пароль
    esp_wifi_start();                       // запускаем WiFi
}

// принимает ровно len байт (сокет может вернуть меньше за один вызов)
int recv_all(int sock, void *buf, int len)
{
    int total = 0;              // сколько байт уже принято
    char *p = (char *)buf;      // буфер как массив байт
    while (total < len) {       // пока не набрали нужное количество
        int n = recv(sock, p + total, len - total, 0);
        if (n <= 0) return -1;  // 0 = хост закрыл соединение, <0 = ошибка
        total += n;
    }
    return total;
}


//----------------------------------------------
//                  TCP
//----------------------------------------------


int create_socket_tcp(struct sockaddr_in *dest){
    *dest = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port   = htons(HOST_PORT),   // порт в сетевой порядок байт
    };
    inet_pton(AF_INET, HOST_IP, &dest->sin_addr);  // строку IP в бинарный вид
    return socket(AF_INET, SOCK_STREAM, 0);  // TCP-сокет IPv4
}

int connect_socket_tcp(const char *host_ip, int host_port){
    struct sockaddr_in dest = {0};
    int i = 0;
    while (i++ < 10) {
        int sock = create_socket_tcp(&dest);
        if (sock >= 0 &&
            connect(sock, (struct sockaddr *)&dest, sizeof(dest)) == 0)
            return sock;
        if (sock >= 0) close(sock);              // мёртвый сокет — закрываем
        vTaskDelay(pdMS_TO_TICKS(3000));         // ждём 3 секунды и пробуем снова
    }
}

int recv_int_tcp(int sock, int *buf){
    return recv_all(sock, buf, sizeof(int));
}
int recv_float_tcp(int sock, float *buf){
    return recv_all(sock, buf, sizeof(float));
}
int recv_double_tcp(int sock, double *buf){
    return recv_all(sock, buf, sizeof(double));
}
int recv_char_tcp(int sock, char *buf){
    return recv_all(sock, buf, sizeof(char));
}


int send_int_tcp(int sock, int *buf){
    return send(sock, buf, sizeof(int), 0);
}
int send_float_tcp(int sock, float *buf){
    return send(sock, buf, sizeof(float), 0);
}
int send_double_tcp(int sock, double *buf){
    return send(sock, buf, sizeof(double), 0);
}
int send_char_tcp(int sock, char *buf){
    return send(sock, buf, sizeof(char), 0);
}




//----------------------------------------------
//                  UDP
//----------------------------------------------

int create_socket_udp(struct sockaddr_in *dest){
    *dest = (struct sockaddr_in){
        .sin_family = AF_INET,
        .sin_port   = htons(HOST_PORT),   // порт в сетевой порядок байт
    };
    inet_pton(AF_INET, HOST_IP, &dest->sin_addr);  // строку IP в бинарный вид
    return socket(AF_INET, SOCK_DGRAM, 0);  // UDP-сокет IPv4
}

int connect_socket_udp(const char *host_ip, int host_port){
    struct sockaddr_in dest = {0};
    int i = 0;
    while (i++ < 10) {
        int sock = create_socket_udp(&dest);
        if (sock >= 0 &&
            connect(sock, (struct sockaddr *)&dest, sizeof(dest)) == 0)
            return sock;
        if (sock >= 0) close(sock);              // мёртвый сокет — закрываем
        vTaskDelay(pdMS_TO_TICKS(3000));         // ждём 3 секунды и пробуем снова
    }
}

int send_int_udp(int sock, int *buf, struct sockaddr_in dest){
    return sendto(sock, buf, sizeof(int), 0, (struct sockaddr *)&dest, sizeof(dest));
}
int send_float_udp(int sock, float *buf, struct sockaddr_in dest){
    return sendto(sock, buf, sizeof(float), 0, (struct sockaddr *)&dest, sizeof(dest));
}
int send_double_udp(int sock, double *buf, struct sockaddr_in dest){
    return sendto(sock, buf, sizeof(double), 0, (struct sockaddr *)&dest, sizeof(dest));
}
int send_char_udp(int sock, char *buf, struct sockaddr_in dest){
    return sendto(sock, buf, sizeof(char), 0, (struct sockaddr *)&dest, sizeof(dest));
}

int recv_int_udp(int sock, int *buf, struct sockaddr_in *src){
    socklen_t src_len = sizeof(*src);
    return recvfrom(sock, buf, sizeof(int), 0, (struct sockaddr *)src, &src_len);
}
int recv_float_udp(int sock, float *buf, struct sockaddr_in *src){
    socklen_t src_len = sizeof(*src);
    return recvfrom(sock, buf, sizeof(float), 0, (struct sockaddr *)src, &src_len);
}
int recv_double_udp(int sock, double *buf, struct sockaddr_in *src){
    socklen_t src_len = sizeof(*src);
    return recvfrom(sock, buf, sizeof(double), 0, (struct sockaddr *)src, &src_len);
}
int recv_char_udp(int sock, char *buf, struct sockaddr_in *src){
    socklen_t src_len = sizeof(*src);
    return recvfrom(sock, buf, sizeof(char), 0, (struct sockaddr *)src, &src_len);
}

