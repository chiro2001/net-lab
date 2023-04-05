#include "net.h"
#include "ip.h"
#include "ethernet.h"
#include "arp.h"
#include "icmp.h"
#include "debug_macros.h"

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void ip_in(buf_t *buf, uint8_t *src_mac) {
  // check package length
  if (buf->len < sizeof(ip_hdr_t)) {
    Log("ip: package too short");
    return;
  }
  ip_hdr_t *p = (ip_hdr_t *) buf->data;
  if (buf->len < p->hdr_len << 2) {
    Log("ip: package shorter than header expected");
    return;
  }
  // check version, support ipv4 only
  if (p->version != 4) {
    Log("ip: invalid version %d", p->version);
    return;
  }
  // check header length
  if (p->hdr_len < 5) {
    Log("ip: invalid header length %d", p->hdr_len);
    return;
  }

}

/**
 * @brief 处理一个要发送的ip分片
 * 
 * @param buf 要发送的分片
 * @param ip 目标ip地址
 * @param protocol 上层协议
 * @param id 数据包id
 * @param offset 分片offset，必须被8整除
 * @param mf 分片mf标志，是否有下一个分片
 */
void ip_fragment_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol, int id, uint16_t offset, int mf) {
  // TODO
}

/**
 * @brief 处理一个要发送的ip数据包
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol) {
  // TODO
}

/**
 * @brief 初始化ip协议
 * 
 */
void ip_init() {
  net_add_protocol(NET_PROTOCOL_IP, ip_in);
}