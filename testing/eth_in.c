#include <stdio.h>
#include "driver.h"
#include "ethernet.h"
#include "debug_macros.h"

extern FILE *pcap_in;
extern FILE *pcap_out;
extern FILE *control_flow;
extern FILE *ip_fout;
extern FILE *arp_fout;
extern FILE *demo_log;
extern FILE *out_log;

int check_log();

FILE *open_file(char *path, char *name, char *mode);

buf_t buf;

int main(int argc, char *argv[]) {
  int ret;
  pcap_in = open_file(argv[1], "in.pcap", "r");
  pcap_out = open_file(argv[1], "out.pcap", "w");
  ip_fout = open_file(argv[1], "log", "w");
  if (pcap_in == 0 || pcap_out == 0 || ip_fout == 0) {
    if (pcap_in) fclose(pcap_in); else Err("Failed to open in.pcap");
    if (pcap_out) fclose(pcap_out); else Err("Failed to open out.pcap");
    if (ip_fout) fclose(ip_fout); else Err("Failed to open log");
    return -1;
  }
  arp_fout = ip_fout;
  control_flow = ip_fout;

  Log("Test start");
  net_init();
  int i = 1;
  while ((ret = driver_recv(&buf)) > 0) {
    Log("Feeding input %02d", i);
    fprintf(control_flow, "\nRound %02d -----------------------------\n", i++);
    ethernet_in(&buf);
  }
  if (ret < 0) {
    Err("Error occur on loading input,exiting");
  }
  driver_close();
  Ok("Sample input all processed, checking output");

  fclose(ip_fout);

  demo_log = open_file(argv[1], "demo_log", "r");
  out_log = open_file(argv[1], "log", "r");
  if (demo_log == 0 || out_log == 0) {
    if (demo_log) fclose(demo_log); else Err("Failed to open demo_log");
    if (out_log) fclose(out_log); else Err("Failed to open log");
    return -1;
  }
  ret = check_log();
  fclose(demo_log);
  fclose(out_log);
  return (ret == 0) ? 0 : -1;
}