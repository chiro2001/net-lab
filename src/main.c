#include "net.h"
#include "udp.h"
#include "tcp.h"
#include "http.h"
#include "driver.h"
#include "time.h"
#include "debug_macros.h"

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wformat="
#pragma GCC diagnostic ignored "-Wformat-extra-args"


#ifdef UDP

void udp_handler(uint8_t *data, size_t len, uint8_t *src_ip, uint16_t src_port) {
  printf("recv udp packet from %s:%u len=%zu\n", iptos(src_ip), src_port, len);
  for (int i = 0; i < len; i++)
    putchar(data[i]);
  putchar('\n');
  udp_send(data, len, 60000, src_ip, src_port); //发送udp包
}

#endif

#ifdef TCP

void tcp_handler(tcp_connect_t *connect, connect_state_t state) {
  uint8_t buf[512];
  size_t len = tcp_connect_read(connect, buf, sizeof(buf) - 1);
  buf[len] = 0;
  Log("recv tcp packet from %s:%u len=%zu, content: %s",
      iptos(connect->ip), connect->remote_port, len, buf);
  // printf("%s\n", buf);
  if (len) tcp_connect_write(connect, buf, len);
  // else {
  //   const char start_msg[] = "hi there!";
  //   Log("tcp handler: sending msg %s", start_msg);
  //   tcp_connect_write(connect, (uint8_t *) start_msg, sizeof(start_msg));
  // }
}

#endif

int main(int argc, char const *argv[]) {
  srand(0x55aa);
  Log("Computer Networking Lab");
  if (net_init() != 0) {
    Err("net init failed.");
    return -1;
  }
#ifdef UDP
  udp_open(60000, udp_handler); //注册端口的udp监听回调
#endif
#ifdef TCP
  tcp_open(61000, tcp_handler); //注册端口的tcp监听回调
#endif
#ifdef HTTP
  http_server_open(62000);
#endif
  while (1) {
    //一次主循环
    net_poll(); //一次主循环
#ifdef HTTP
    http_server_run();
#endif
    // 节约用电
#ifndef _MSC_VER
    struct timespec sleepTime = {0, 1000000};
    nanosleep(&sleepTime, NULL);
#endif
  }

  return 0;
}

#pragma GCC diagnostic pop
