#include "net.h"
#include "icmp.h"
#include "ip.h"
#include "debug_macros.h"

/**
 * @brief 发送icmp响应
 * 
 * @param req_buf 收到的icmp请求包
 * @param src_ip 源ip地址
 */
static void icmp_resp(buf_t *req_buf, uint8_t *src_ip) {
  Log("icmp: resp, req_buf len %zu", req_buf->len);
  // init txbuf, icmp reply should copy request data
  // buf_copy(&txbuf, req_buf, 0);
  buf_init(&txbuf, 8);
  icmp_hdr_t *p = (icmp_hdr_t *) txbuf.data;
  // icmp_hdr_t *recv = (icmp_hdr_t *) req_buf->data;
  // only ping
  p->type = ICMP_TYPE_ECHO_REPLY;
  p->code = 0;
  p->checksum16 = 0;
  // p->id16 = recv->id16;
  // p->seq16 = recv->seq16;
  p->checksum16 = checksum16((uint16_t *) p, txbuf.len);
  ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}

/**
 * @brief 处理一个收到的数据包
 * 
 * @param buf 要处理的数据包
 * @param src_ip 源ip地址
 */
void icmp_in(buf_t *buf, uint8_t *src_ip) {
  Log("icmp: in from %s", iptos(src_ip));
  // check package length
  if (buf->len < sizeof(icmp_hdr_t)) return;
  // if it's an echo request, send an echo reply
  icmp_hdr_t *icmp_hdr = (icmp_hdr_t *) buf->data;
  if (icmp_hdr->type == ICMP_TYPE_ECHO_REQUEST) {
    Log("icmp: ping recv from %s, send ping reply", iptos(src_ip));
    icmp_resp(buf, src_ip);
  }
}

/**
 * @brief 发送icmp不可达
 * 
 * @param recv_buf 收到的ip数据包
 * @param src_ip 源ip地址
 * @param code icmp code，协议不可达或端口不可达
 */
void icmp_unreachable(buf_t *recv_buf, uint8_t *src_ip, icmp_code_t code) {
  Log("icmp: unreachable, send to %s, code=%d", iptos(src_ip), code);
  buf_init(&txbuf, sizeof(icmp_hdr_t) + sizeof(ip_hdr_t) + 8);
  icmp_hdr_t *p = (icmp_hdr_t *) txbuf.data;
  p->type = ICMP_TYPE_UNREACH;
  p->code = code;
  p->checksum16 = 0;
  p->id16 = 0;
  p->seq16 = 0;
  memcpy(txbuf.data + sizeof(icmp_hdr_t), recv_buf->data, sizeof(ip_hdr_t) + 8);
  p->checksum16 = checksum16((uint16_t *) txbuf.data, txbuf.len);
  ip_out(&txbuf, src_ip, NET_PROTOCOL_ICMP);
}

/**
 * @brief 初始化icmp协议
 * 
 */
void icmp_init() {
  net_add_protocol(NET_PROTOCOL_ICMP, icmp_in);
}