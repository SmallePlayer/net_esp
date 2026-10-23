#pragma once

#include <unistd.h>          // close() для сокета
#include <sys/socket.h>      // socket(), connect(), recv()
#include <arpa/inet.h>       // htons(), inet_pton(), sockaddr_in

// инициализация WiFi в режиме станции (клиента)
void wifi_init_sta(const char *ssid, const char *password); 
// инициализация WiFi в режиме станции (сервера)
void wifi_init_ap(const char *ssid, const char *password);




//----------------------------------------------
//                  TCP
//----------------------------------------------
int create_socket_tcp(struct sockaddr_in *dest); // создаёт сокет и заполняет структуру sockaddr_in
int connect_socket_tcp(const char *host_ip, int host_port); // подключается к серверу, возвращает сокет

int recv_int_tcp(int sock, int *buf); // принимает ровно sizeof(int) байт
int recv_float_tcp(int sock, float *buf); // принимает ровно sizeof(float) байт
int recv_double_tcp(int sock, double *buf); // принимает ровно sizeof(double) байт
int recv_char_tcp(int sock, char *buf); // принимает ровно sizeof(char) байт
int recv_all(int sock, void *buf, int len); // принимает ровно len байт (сокет может вернуть меньше за один вызов)

int send_int_tcp(int sock, int *buf); // отправляет ровно sizeof(int) байт
int send_float_tcp(int sock, float *buf); // отправляет ровно sizeof(float) байт
int send_double_tcp(int sock, double *buf); // отправляет ровно sizeof(double)
int send_char_tcp(int sock, char *buf); // отправляет ровно sizeof(char) байт


//----------------------------------------------
//                  UDP
//----------------------------------------------
int create_socket_udp(struct sockaddr_in *dest); // создаёт UDP-сокет и заполняет структуру sockaddr_in
int connect_socket_udp(const char *host_ip, int host_port); // подключается к серверу, возвращает сокет

int send_int_udp(int sock, int *buf, struct sockaddr_in dest); // отправляет ровно sizeof(int) байт
int send_float_udp(int sock, float *buf, struct sockaddr_in dest); // отправляет ровно sizeof(float) байт
int send_double_udp(int sock, double *buf, struct sockaddr_in dest); // отправляет ровно sizeof(double) байт
int send_char_udp(int sock, char *buf, struct sockaddr_in dest); // отправляет ровно sizeof(char) байт

int recv_int_udp(int sock, int *buf, struct sockaddr_in *src); // принимает ровно sizeof(int) байт
int recv_float_udp(int sock, float *buf, struct sockaddr_in *src); // принимает ровно sizeof(float) байт
int recv_double_udp(int sock, double *buf, struct sockaddr_in *src); // принимает ровно sizeof(double) байт
int recv_char_udp(int sock, char *buf, struct sockaddr_in *src); // принимает ровно sizeof(char) байт  