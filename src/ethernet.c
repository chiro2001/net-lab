#include "ethernet.h"
#include "utils.h"
#include "driver.h"
#include "arp.h"
#include "ip.h"
#include "debug_macros.h"
#include "tcp.h"
#include "udp.h"
#include "icmp.h"

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 */
void ethernet_in(buf_t *buf) {
  ether_hdr_t *hdr = (ether_hdr_t *) buf->data;
  uint16_t length_type = swap16(hdr->protocol16);
  // if is broadcast or is for me, handle it
  if (memcmp(hdr->dst, net_if_mac, NET_MAC_LEN) == 0 ||
      memcmp(hdr->dst, net_broadcast_mac, NET_MAC_LEN) == 0) {
    Log("ethernet: package for me, dst=%s, src=%s, length/type=%x(%d)", mactos(buf->data),
        mactos(hdr->src), length_type, length_type);
    if (46 <= length_type && length_type <= 1500) {
      // this is a length field
    } else if (length_type >= 0x0600) {
      // this is a type field
      buf_remove_header(buf, sizeof(ether_hdr_t));
      net_in(buf, length_type, hdr->src);
    } else {
      // invalid length/type field, drop this packet
      return;
    }
  } else {
    Log("ethernet: package not for me, dst=%s, src=%s", mactos(buf->data), mactos(buf->data + NET_MAC_LEN));
  }
}

/**
 * @brief 处理一个要发送的数据包
 * 
 * @param buf 要处理的数据包
 * @param mac 目标MAC地址
 * @param protocol 上层协议
 */
void ethernet_out(buf_t *buf, const uint8_t *mac, net_protocol_t protocol) {
  buf_add_header(buf, sizeof(ether_hdr_t));
  ether_hdr_t *hdr = (ether_hdr_t *) buf->data;
  memcpy(hdr->src, net_if_mac, NET_MAC_LEN);
  memcpy(hdr->dst, mac, NET_MAC_LEN);
  hdr->protocol16 = protocol;
}

/**
 * @brief 初始化以太网协议
 * 
 */
void ethernet_init() {
  buf_init(&rxbuf, ETHERNET_MAX_TRANSPORT_UNIT + sizeof(ether_hdr_t));
}

/**
 * @brief 一次以太网轮询
 * 
 */
void ethernet_poll() {
  if (driver_recv(&rxbuf) > 0)
    ethernet_in(&rxbuf);
}
