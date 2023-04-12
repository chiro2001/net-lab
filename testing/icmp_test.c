#include <stdio.h>
#include <string.h>
#include "driver.h"
#include "ethernet.h"
#include "arp.h"
#include "ip.h"
#include "debug_macros.h"

extern FILE *pcap_in;
extern FILE *pcap_out;
extern FILE *pcap_demo;
extern FILE *control_flow;
extern FILE *udp_fout;
extern FILE *demo_log;
extern FILE *out_log;
extern FILE *arp_log_f;

char *print_ip(uint8_t *ip);

char *print_mac(uint8_t *mac);

uint8_t my_mac[] = NET_IF_MAC;
uint8_t boardcast_mac[] = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

int check_log();

int check_pcap();

FILE *open_file(char *path, char *name, char *mode);

void log_tab_buf();

buf_t buf;

int main(int argc, char *argv[]) {
  int ret;
  Log("Test begin.");
  pcap_in = open_file(argv[1], "in.pcap", "r");
  pcap_out = open_file(argv[1], "out.pcap", "w");
  control_flow = open_file(argv[1], "log", "w");
  if (pcap_in == 0 || pcap_out == 0 || control_flow == 0) {
    if (pcap_in) fclose(pcap_in); else Err("Failed to open in.pcap");
    if (pcap_out)fclose(pcap_out); else Err("Failed to open out.pcap");
    if (control_flow) fclose(control_flow); else Err("Failed to open log");
    return -1;
  }
  udp_fout = control_flow;
  arp_log_f = control_flow;

  net_init();
  log_tab_buf();
  int i = 1;
  while ((ret = driver_recv(&buf)) > 0) {
    Log("Feeding input %02d", i);
    fprintf(control_flow, "\nRound %02d -----------------------------\n", i++);
    if (memcmp(buf.data, my_mac, 6) && memcmp(buf.data, boardcast_mac, 6)) {
      buf_t buf2;
      buf_copy(&buf2, &buf, 0);
      memset(buf2.data, 0, sizeof(ether_hdr_t));
      buf_remove_header(&buf2, sizeof(ether_hdr_t));
      int len = (buf2.data[0] & 0xf) << 2;
      uint8_t *ip = buf.data + 30;
      net_protocol_t pro = buf2.data[9];
      memset(buf2.data, 0, sizeof(len));
      buf_remove_header(&buf2, len);
      ip_out(&buf2, ip, pro);
    } else {
      ethernet_in(&buf);
    }
    log_tab_buf();
  }
  if (ret < 0) {
    Err("Error occur on loading input,exiting");
  }
  driver_close();
  Log("Sample input all processed, checking output");

  fclose(control_flow);

  demo_log = open_file(argv[1], "demo_log", "r");
  out_log = open_file(argv[1], "log", "r");
  pcap_out = open_file(argv[1], "out.pcap", "r");
  pcap_demo = open_file(argv[1], "demo_out.pcap", "r");
  if (demo_log == 0 || out_log == 0 || pcap_out == 0 || pcap_demo == 0) {
    if (demo_log) fclose(demo_log); else Log("Failed to open demo_log");
    if (out_log) fclose(out_log); else Log("Failed to open log");
    if (pcap_demo) fclose(pcap_demo); else Log("Failed to open demo_out.pcap");
    if (pcap_out) fclose(pcap_out); else Log("Failed to open out.pcap");
    return -1;
  }
  check_log();
  ret = check_pcap() ? 1 : 0;
  Ok("For this test, log is only a reference. "
     "Your implementation is OK if your pcap file is the same to the demo pcap file.");
  fclose(demo_log);
  fclose(out_log);
  return ret ? -1 : 0;
}