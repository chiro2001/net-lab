#include <stdbool.h>
#include "udp.h"
#include "ip.h"
#include "icmp.h"
#include "debug_macros.h"

/**
 * @brief udp处理程序表
 * 
 */
map_t udp_table;

/**
 * @brief udp伪校验和计算
 * 
 * @param buf 要计算的包
 * @param src_ip 源ip地址
 * @param dst_ip 目的ip地址
 * @return uint16_t 伪校验和
 */
static uint16_t udp_checksum(buf_t *buf, uint8_t *src_ip, uint8_t *dst_ip) {
  // peso-header area in buf is mutable, must backup-restore it
  uint8_t backup_ip_header[sizeof(ip_hdr_t)];
  memcpy(backup_ip_header, buf->data - sizeof(ip_hdr_t), sizeof(ip_hdr_t));
  ip_hdr_t *ip_header = (ip_hdr_t *) backup_ip_header;
  uint16_t udp_length = buf->len;
  // generate peso-header
  buf_add_header(buf, sizeof(udp_peso_hdr_t));
  udp_peso_hdr_t *peso = (udp_peso_hdr_t *) buf->data;
  memcpy(peso->src_ip, src_ip, NET_IP_LEN);
  memcpy(peso->dst_ip, dst_ip, NET_IP_LEN);
  peso->placeholder = 0;
  peso->protocol = ip_header->protocol;
  peso->total_len16 = swap16(udp_length);
  bool has_one_padding = false;
  if (buf->len & 1) {
    Log("udp: checksum, data odd length (%zu), add 1 byte padding",
        buf->len - sizeof(udp_peso_hdr_t) - sizeof(udp_hdr_t));
    Assert(buf_add_padding(buf, 1) == 0, "Cannot add buf padding");
    has_one_padding = true;
  }
  // calculate checksum
  uint16_t checksum = checksum16((uint16_t *) buf->data, buf->len);
  if (has_one_padding) buf_remove_padding(buf, 1);
  // restore backup
  buf_remove_header(buf, sizeof(udp_peso_hdr_t));
  memcpy(buf->data - sizeof(ip_hdr_t), backup_ip_header, sizeof(ip_hdr_t));
  return checksum;
}

/**
 * @brief 处理一个收到的udp数据包
 * 
 * @param buf 要处理的包
 * @param src_ip 源ip地址
 */
void udp_in(buf_t *buf, uint8_t *src_ip) {
  // check package length
  if (buf->len < sizeof(udp_hdr_t)) {
    Log("udp: too short package! len(%zu) < udp_header_size(%llu)", buf->len, sizeof(udp_hdr_t));
    return;
  }
  udp_hdr_t *p = (udp_hdr_t *) buf->data;
  uint16_t total_len = swap16(p->total_len16);
  if (buf->len < total_len) {
    Log("udp: too short package! len(%zu) < total_len(%d)", buf->len, total_len);
    return;
  }
  uint16_t port = swap16(p->dst_port16);
  if (port != 60000) {
    Dbg("udp: ignored port %d", port);
    return;
  } else {
    Log("udp: recv target port package");
  }
  uint16_t checksum_expected = p->checksum16;
  if (checksum_expected == 0) {
    Log("udp: ignore checksum");
  } else {
    p->checksum16 = 0;
    uint16_t checksum_actual = udp_checksum(buf, src_ip, net_if_ip);
    if (checksum_expected != checksum_actual) {
      Log("udp: checksum error! expected=%x, actual=%x", checksum_expected, checksum_actual);
      return;
    }
    p->checksum16 = checksum_expected;
  }
  // check port handler
  udp_handler_t handler = map_get(&udp_table, &port);
  if (handler) {
    Log("udp: successfully call handler for port %d", port);
    handler(buf->data + sizeof(udp_hdr_t), buf->len - sizeof(udp_hdr_t), src_ip, swap16(p->src_port16));
  } else {
    Log("udp: no handler for port %d!", swap16(p->dst_port16));
  }
}

/**
 * @brief 处理一个要发送的数据包
 * 
 * @param buf 要处理的包
 * @param src_port 源端口号
 * @param dst_ip 目的ip地址
 * @param dst_port 目的端口号
 */
void udp_out(buf_t *buf, uint16_t src_port, uint8_t *dst_ip, uint16_t dst_port) {
  // add udp header
  buf_add_header(buf, sizeof(udp_hdr_t));
  udp_hdr_t *p = (udp_hdr_t *) buf->data;
  p->src_port16 = swap16(src_port);
  p->dst_port16 = swap16(dst_port);
  p->total_len16 = swap16(buf->len);
  p->checksum16 = 0;
  p->checksum16 = udp_checksum(buf, net_if_ip, dst_ip);
  // send to ip layer
  ip_out(buf, dst_ip, NET_PROTOCOL_UDP);
}

/**
 * @brief 初始化udp协议
 * 
 */
void udp_init() {
  map_init(&udp_table, sizeof(uint16_t), sizeof(udp_handler_t), 0, 0, NULL);
  net_add_protocol(NET_PROTOCOL_UDP, udp_in);
}

/**
 * @brief 打开一个udp端口并注册处理程序
 * 
 * @param port 端口号
 * @param handler 处理程序
 * @return int 成功为0，失败为-1
 */
int udp_open(uint16_t port, udp_handler_t handler) {
  return map_set(&udp_table, &port, &handler);
}

/**
 * @brief 关闭一个udp端口
 * 
 * @param port 端口号
 */
void udp_close(uint16_t port) {
  map_delete(&udp_table, &port);
}

/**
 * @brief 发送一个udp包
 * 
 * @param data 要发送的数据
 * @param len 数据长度
 * @param src_port 源端口号
 * @param dst_ip 目的ip地址
 * @param dst_port 目的端口号
 */
void udp_send(uint8_t *data, uint16_t len, uint16_t src_port, uint8_t *dst_ip, uint16_t dst_port) {
  buf_init(&txbuf, len);
  memcpy(txbuf.data, data, len);
  udp_out(&txbuf, src_port, dst_ip, dst_port);
}