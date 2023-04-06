#include "net.h"
#include "ip.h"
#include "arp.h"
#include "icmp.h"
#include "debug_macros.h"

static uint16_t ip_id = 0;

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 * @param src_mac 源mac地址
 */
void ip_in(buf_t *buf, uint8_t *src_mac) {
  Dbg("ip: in from %s", mactos(src_mac));
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
  if (p->version != IP_VERSION_4) {
    Log("ip: invalid version %d", p->version);
    return;
  }
  // check header length
  if (p->hdr_len < 5) {
    Log("ip: invalid header length %d", p->hdr_len);
    return;
  }
  // check DF bit
  if (p->flags_fragment16 & IP_DO_NOT_FRAGMENT && buf->len > ETHERNET_MAX_TRANSPORT_UNIT) {
    Log("ip: DF bit set, but it is a large frame");
    icmp_unreachable(buf, p->src_ip, ICMP_CODE_PROTOCOL_UNREACH);
    return;
  }
  // checksum
  uint16_t checksum_expected = p->hdr_checksum16;
  p->hdr_checksum16 = 0;
  uint16_t checksum_actual = checksum16((uint16_t *) buf->data, sizeof(ip_hdr_t));
  if (checksum_expected != checksum_actual) {
    Log("ip: checksum failed! expected: %x, actual: %x", checksum_expected, checksum_actual);
    return;
  }
  p->hdr_checksum16 = checksum_expected;
  uint16_t total_len = swap16(p->total_len16);
  Dbg("ip: before remove padding, len=%zu, total_len16=%d", buf->len, total_len);
  // removing paddings
  buf_remove_padding(buf, buf->len - total_len);
  Dbg("ip: after remove padding, len=%zu", buf->len);
  // remove ip header
  buf_remove_header(buf, sizeof(ip_hdr_t));
  if (net_in(buf, p->protocol, p->src_ip) < 0) {
    Log("ip: in, unrecognized protocol %d, send icmp protocol unreachable", p->protocol);
    buf_add_header(buf, sizeof(ip_hdr_t));
    icmp_unreachable(buf, p->src_ip, ICMP_CODE_PROTOCOL_UNREACH);
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
  Log("ip: ip_fragment_out ip=%s, id=%d, offset=%d, mf=%d, len=%zu", iptos(ip), id, offset, mf, buf->len);
  buf_add_header(buf, sizeof(ip_hdr_t));
  ip_hdr_t *p = (ip_hdr_t *) buf->data;
  p->version = IP_VERSION_4;
  p->hdr_len = sizeof(ip_hdr_t) >> 2;
  p->tos = 0;
  p->total_len16 = swap16(buf->len);
  p->id16 = swap16(id);
  p->flags_fragment16 = swap16((mf ? IP_MORE_FRAGMENT : 0) | (offset >> 3));
  p->ttl = IP_OUT_TTL;
  p->protocol = protocol;
  p->hdr_checksum16 = 0;
  // fill in ip data
  memcpy(p->dst_ip, ip, NET_IP_LEN);
  memcpy(p->src_ip, net_if_ip, NET_IP_LEN);
  // calculate checksum
  p->hdr_checksum16 = checksum16((uint16_t *) buf->data, sizeof(ip_hdr_t));
  // send package
  arp_out(buf, ip);
}

/**
 * @brief 处理一个要发送的ip数据包
 * 
 * @param buf 要处理的包
 * @param ip 目标ip地址
 * @param protocol 上层协议
 */
void ip_out(buf_t *buf, uint8_t *ip, net_protocol_t protocol) {
  Log("ip: out to %s", iptos(ip));
  // check if ip package larger than MTU - ip header
  const size_t ip_max_length = ETHERNET_MAX_TRANSPORT_UNIT - sizeof(ip_hdr_t);
  if (buf->len > ip_max_length) {
    Log("ip: handle large package(%zu bytes)", buf->len);
    // split this package to multy packages, backup buf data
    size_t original_len = buf->len;
    uint8_t *original_data = buf->data;
    buf->len = ip_max_length;
    size_t offset = 0;
    bool done = false;
    uint8_t backup[sizeof(ip_hdr_t)];
    // FIXME: this is a tricky way to handle large package
    //  which reduce data copy but require lower layers to see package as immutable
    while (!done) {
      if (offset + ip_max_length <= original_len) {
        // backup data in header area
        uint8_t *data_now = buf->data;
        memcpy(backup, data_now, sizeof(ip_hdr_t));
        ip_fragment_out(buf, ip, protocol, ip_id, offset, 1);
        // restore backup data
        memcpy(data_now, backup, sizeof(ip_hdr_t));
        offset += ip_max_length;
        buf->data = data_now + ip_max_length;
        // buf->len was changed in ip_fragment_out
        buf->len = ip_max_length;
      } else {
        buf->len = original_len - offset;
        // last len may be zero
        if (buf->len) ip_fragment_out(buf, ip, protocol, ip_id++, offset, 0);
        done = true;
      }
    }
    // restore this buf
    buf->data = original_data;
    buf->len = original_len;
  } else {
    Log("ip: handle small package(%zu bytes)", buf->len);
    ip_fragment_out(buf, ip, protocol, ip_id++, 0, 0);
  }
}

/**
 * @brief 初始化ip协议
 * 
 */
void ip_init() {
  net_add_protocol(NET_PROTOCOL_IP, ip_in);
}