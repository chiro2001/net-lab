#include <string.h>
#include <stdio.h>
#include "net.h"
#include "arp.h"
#include "ethernet.h"

/**
 * @brief 初始的arp包
 * 
 */
static const arp_pkt_t arp_init_pkt = {
    .hw_type16 = constswap16(ARP_HW_ETHER),
    .pro_type16 = constswap16(NET_PROTOCOL_IP),
    .hw_len = NET_MAC_LEN,
    .pro_len = NET_IP_LEN,
    .sender_ip = NET_IF_IP,
    .sender_mac = NET_IF_MAC,
    .target_mac = {0}};

/**
 * @brief arp地址转换表，<ip,mac>的容器
 * 
 */
map_t arp_table;

/**
 * @brief arp buffer，<ip,buf_t>的容器
 * 
 */
map_t arp_buf;

/**
 * @brief 打印一条arp表项
 * 
 * @param ip 表项的ip地址
 * @param mac 表项的mac地址
 * @param timestamp 表项的更新时间
 */
void arp_entry_print(void *ip, void *mac, time_t *timestamp) {
  printf("%s | %s | %s\n", iptos(ip), mactos(mac), timetos(*timestamp));
}

/**
 * @brief 打印整个arp表
 * 
 */
void arp_print() {
  printf("===ARP TABLE BEGIN===\n");
  map_foreach(&arp_table, arp_entry_print);
  printf("===ARP TABLE  END ===\n");
}

/**
 * @brief 发送一个arp请求
 * 
 * @param target_ip 想要知道的目标的ip地址
 */
void arp_req(uint8_t *target_ip) {
  // init txbuf
  buf_init(&txbuf, sizeof(arp_pkt_t) + ARP_PADDING_SIZE);
  arp_pkt_t *p = (arp_pkt_t *) txbuf.data;
  // fill in padding
  memset(txbuf.data + sizeof(arp_pkt_t), 0, ARP_PADDING_SIZE);
  // fill in default data
  memcpy(p, &arp_init_pkt, sizeof(arp_pkt_t));
  // fill in target ip
  memcpy(p->target_ip, target_ip, NET_IP_LEN);
  // call ethernet layer
  ethernet_out(&txbuf, net_broadcast_mac, NET_PROTOCOL_ARP);
}

/**
 * @brief 发送一个arp响应
 * 
 * @param target_ip 目标ip地址
 * @param target_mac 目标mac地址
 */
void arp_resp(uint8_t *target_ip, uint8_t *target_mac) {
  // init txbuf
  buf_init(&txbuf, sizeof(arp_pkt_t) + ARP_PADDING_SIZE);
  arp_pkt_t *p = (arp_pkt_t *) txbuf.data;
  // fill in padding
  memset(txbuf.data + sizeof(arp_pkt_t), 0, ARP_PADDING_SIZE);
  // fill in default data
  memcpy(p, &arp_init_pkt, sizeof(arp_pkt_t));
  // fill in target ip, target mac, sender ip, sender mac
  memcpy(p->target_ip, target_ip, NET_IP_LEN);
  memcpy(p->target_mac, target_mac, NET_IP_LEN);
  memcpy(p->sender_ip, net_if_ip, NET_MAC_LEN);
  memcpy(p->sender_mac, net_if_mac, NET_MAC_LEN);
  // call ethernet layer
  ethernet_out(&txbuf, target_mac, NET_PROTOCOL_ARP);
}

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void arp_in(buf_t *buf, uint8_t *src_mac) {
  // check package length
  if (buf->len < sizeof(arp_pkt_t)) {
    return;
  }
  // check package
  arp_pkt_t *p = (arp_pkt_t *) buf->data;
  if (p->pro_type16 == constswap16(NET_PROTOCOL_ARP) &&
      p->hw_len == NET_MAC_LEN && p->pro_len == NET_IP_LEN &&
      memcmp(p->target_ip, net_if_ip, NET_MAC_LEN) == 0 &&
      memcmp(p->target_mac, net_if_mac, NET_MAC_LEN) == 0) {
    // handle arp reply
    if (p->hw_type16 == constswap16(ARP_REPLY)) {
      // update arp table
      map_set(&arp_table, p->sender_ip, p->sender_mac);
      // flush pending buffer
      buf_t *pending = (buf_t *) map_get(&arp_buf, p->sender_ip);
      if (pending) {
        // re-send this pending packet
        ethernet_out(pending, p->sender_mac, NET_PROTOCOL_ARP);
        // remove this item in pending buffer
        map_delete(&arp_buf, p->target_ip);
      }
    } else {
      // handle arp request
      if (p->hw_type16 == constswap16(ARP_REQUEST)) {
        // update arp table
        map_set(&arp_table, p->sender_ip, p->sender_mac);
        // send reply
        arp_resp(p->sender_ip, p->sender_mac);
      }
    }
  }
}

/**
 * @brief 处理一个要发送的数据包
 * 
 * @param buf 要处理的数据包
 * @param ip 目标ip地址
 */
void arp_out(buf_t *buf, uint8_t *ip) {
  // find mac in arp table
  uint8_t *mac = (uint8_t *) map_get(&arp_table, ip);
  if (!mac) {
    // not found, see if there is a pending request
    buf_t *pending = (buf_t *) map_get(&arp_buf, ip);
    if (pending) {
      // found, drop this request
    } else {
      // add to pending buffer
      map_set(&arp_buf, ip, buf);
      // not found, send a request
      arp_req(ip);
    }
    // TODO: larger pending buffer
  } else {
    // found, send the packet
    ethernet_out(buf, mac, NET_PROTOCOL_ARP);
  }
}

/**
 * @brief 初始化arp协议
 * 
 */
void arp_init() {
  map_init(&arp_table, NET_IP_LEN, NET_MAC_LEN, 0, ARP_TIMEOUT_SEC, NULL);
  map_init(&arp_buf, NET_IP_LEN, sizeof(buf_t), 0, ARP_MIN_INTERVAL, buf_copy);
  net_add_protocol(NET_PROTOCOL_ARP, arp_in);
  // send a gratuitous arp packet
  arp_req(net_if_ip);
}