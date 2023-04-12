#include <stdio.h>
#include <string.h>
#include "driver.h"
#include "ethernet.h"
#include "debug_macros.h"

extern FILE *pcap_in;
extern FILE *pcap_out;
extern FILE *pcap_demo;
extern FILE *control_flow;
extern FILE *demo_log;
extern FILE *out_log;

int check_log();

int check_pcap();

char *print_mac(uint8_t *mac);

FILE *open_file(char *path, char *name, char *mode);

buf_t buf, buf2;

int main(int argc, char *argv[]) {
  int ret;
  pcap_in = open_file(argv[1], "in.pcap", "r");
  pcap_out = open_file(argv[1], "out.pcap", "w");
  control_flow = open_file(argv[1], "log", "w");
  if (pcap_in == 0 || pcap_out == 0 || control_flow == 0) {
    if (pcap_in) fclose(pcap_in); else Err("Failed to open in.pcap");
    if (pcap_out)fclose(pcap_out); else Err("Failed to open out.pcap");
    if (control_flow) fclose(control_flow); else Err("Failed to open log");
    return -1;
  }

  Log("Test start");
  net_init();
  int i = 1;
  while ((ret = driver_recv(&buf)) > 0) {
    Log("Feeding input %02d", i);
    fprintf(control_flow, "\nRound %02d -----------------------------\n", i++);
    buf_copy(&buf2, &buf, 0);
    memset(buf.data, 0, sizeof(ether_hdr_t));
    buf_remove_header(&buf, sizeof(ether_hdr_t));
    int proto = buf2.data[12];
    proto <<= 8;
    proto |= buf2.data[13];
    ethernet_out(&buf, buf2.data, proto);
  }
  if (ret < 0) {
    Err("Error occur on loading input, exiting");
  }
  driver_close();
  Ok("Sample input all processed, checking output");

  fclose(control_flow);

  demo_log = open_file(argv[1], "demo_log", "r");
  out_log = open_file(argv[1], "log", "r");
  pcap_out = open_file(argv[1], "out.pcap", "r");
  pcap_demo = open_file(argv[1], "demo_out.pcap", "r");
  if (demo_log == 0 || out_log == 0 || pcap_out == 0 || pcap_demo == 0) {
    if (demo_log) fclose(demo_log); else Err("Failed to open demo_log");
    if (out_log) fclose(out_log); else Err("Failed to open log");
    if (pcap_demo) fclose(pcap_demo); else Err("Failed to open demo_out.pcap");
    if (pcap_out) fclose(pcap_out); else Err("Failed to open out.pcap");
    return -1;
  }
  ret = check_log() ? 1 : 0;
  ret = check_pcap() ? 1 : ret;
  fclose(demo_log);
  fclose(out_log);
  return ret ? -1 : 0;
}