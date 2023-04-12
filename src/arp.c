#include <string.h>
#include <stdio.h>
#include "net.h"
#include "arp.h"
#include "ethernet.h"
#include "debug_macros.h"
#include "queue.h"

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
 * @brief arp buffer，<ip,buf_t>的容器, map_t(queue_t)
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
  Log("%s | %s | %s", iptos(ip), mactos(mac), timetos(*timestamp));
}

/**
 * @brief 打印整个arp表
 * 
 */
void arp_print() {
  Log("===ARP TABLE BEGIN===");
  map_foreach(&arp_table, arp_entry_print);
  Log("===ARP TABLE  END ===");
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
  p->opcode16 = constswap16(ARP_REQUEST);
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
  memcpy(p->target_mac, target_mac, NET_MAC_LEN);
  memcpy(p->sender_ip, net_if_ip, NET_IP_LEN);
  memcpy(p->sender_mac, net_if_mac, NET_MAC_LEN);
  p->opcode16 = constswap16(ARP_REPLY);
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
  Dbg("arp: in from %s", mactos(src_mac));
  // check package length
  if (buf->len < sizeof(arp_pkt_t)) {
    Log("arp: invalid package length");
    return;
  }
  // check package
  arp_pkt_t *p = (arp_pkt_t *) buf->data;
  if (p->pro_type16 == constswap16(NET_PROTOCOL_IP) &&
      p->hw_len == NET_MAC_LEN && p->pro_len == NET_IP_LEN) {
    // handle arp reply
    Log("arp in: arp package from mac %s; sender ip=%s, sender mac=%s, target ip=%s, target mac=%s, hw_type=%d, opcode=%d",
        mactos(src_mac), iptos(p->sender_ip), mactos(p->sender_mac), iptos(p->target_ip),
        mactos(p->target_mac), swap16(p->hw_type16), swap16(p->opcode16));
    if (p->opcode16 == constswap16(ARP_REPLY) && memcmp(p->target_ip, net_if_ip, NET_IP_LEN) == 0 &&
        memcmp(p->target_mac, net_if_mac, NET_MAC_LEN) == 0) {
      Log("arp in: this is a arp reply");
      map_set(&arp_table, p->sender_ip, p->sender_mac);
      arp_print();
      // flush pending buffer
      queue_t *pending_queue = (queue_t *) map_get(&arp_buf, p->sender_ip);
      if (pending_queue) {
        Log("arp in: re-send the pending packet");
        buf_t *queued_buf;
        while ((queued_buf = queue_pop(pending_queue)) != NULL)
          ethernet_out(queued_buf, p->sender_mac, NET_PROTOCOL_IP);
        // remove this item in pending buffer
        map_delete(&arp_buf, p->sender_ip);
        // items in this queue was constructed by malloc, so can free them
        // but queue struct is map data, cannot free
        queue_free_data(pending_queue, true);
      }
    } else {
      // handle arp request
      if (p->opcode16 == constswap16(ARP_REQUEST) && memcmp(p->target_ip, net_if_ip, NET_IP_LEN) == 0) {
        Log("arp in: this is a arp request, from ip=%s, mac=%s", iptos(p->sender_ip), mactos(p->sender_mac));
        // update arp table
        map_set(&arp_table, p->sender_ip, p->sender_mac);
        // send reply
        arp_resp(p->sender_ip, p->sender_mac);
      }
    }
  } else {
    Log("arp in: invalid package! pro_type=%x, target ip=%s, target mac=%s", swap16(p->pro_type16), iptos(p->target_ip),
        mactos(p->target_mac));
  }
}

/**
 * @brief 处理一个要发送的数据包
 * 
 * @param buf 要处理的数据包
 * @param ip 目标ip地址
 */
void arp_out(buf_t *buf, uint8_t *ip) {
  if (ip[0] == 0) Err("arp: out to %s", iptos(ip));
  else
    Dbg("arp: out to %s", iptos(ip));
  // find mac in arp table
  uint8_t *mac = (uint8_t *) map_get(&arp_table, ip);
  if (!mac) {
    arp_print();
    Log("arp: %s not found, see if there is a pending request...", iptos(ip));
    queue_t *pending_queue = (queue_t *) map_get(&arp_buf, ip);
    if (pending_queue) {
      Log("arp: a pending request queue found, push this request to queue");
      buf_t *copy = malloc(sizeof(buf_t));
      buf_copy(copy, buf, 0);
      queue_push(pending_queue, copy);
    } else {
      Log("arp: %s was added to arp buffer, and a request was sent", iptos(ip));
      // add to pending buffer
      buf_t *copy = malloc(sizeof(buf_t));
      buf_copy(copy, buf, 0);
      queue_t *q = queue_new(copy);
      map_set(&arp_buf, ip, q);
      // queue indexes was copied to map data, free the queue struct
      free(q);
      // not found, send a request
      arp_req(ip);
    }
  } else {
    // found, send the packet
    ethernet_out(buf, mac, NET_PROTOCOL_IP);
  }
}

/**
 * @brief 初始化arp协议
 * 
 */
void arp_init() {
  map_init(&arp_table, NET_IP_LEN, NET_MAC_LEN, 0, ARP_TIMEOUT_SEC, NULL);
  map_init(&arp_buf, NET_IP_LEN, sizeof(queue_t), 0, 0, queue_copy);
  net_add_protocol(NET_PROTOCOL_ARP, arp_in);
  // send a gratuitous arp packet
  arp_req(net_if_ip);
}