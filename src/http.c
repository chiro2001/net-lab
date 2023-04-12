#include "http.h"
#include "tcp.h"
#include "net.h"
#include "assert.h"
#include "debug_macros.h"

#define TCP_FIFO_SIZE 40

typedef struct http_fifo {
  tcp_connect_t *buffer[TCP_FIFO_SIZE];
  uint8_t front, tail, count;
} http_fifo_t;

static http_fifo_t http_fifo_v;

static void http_fifo_init(http_fifo_t *fifo) {
  fifo->count = 0;
  fifo->front = 0;
  fifo->tail = 0;
}

static int http_fifo_in(http_fifo_t *fifo, tcp_connect_t *tcp) {
  if (fifo->count >= TCP_FIFO_SIZE) {
    return -1;
  }
  fifo->buffer[fifo->front] = tcp;
  fifo->front++;
  if (fifo->front >= TCP_FIFO_SIZE) {
    fifo->front = 0;
  }
  fifo->count++;
  return 0;
}

static tcp_connect_t *http_fifo_out(http_fifo_t *fifo) {
  if (fifo->count == 0) {
    return NULL;
  }
  tcp_connect_t *tcp = fifo->buffer[fifo->tail];
  fifo->tail++;
  if (fifo->tail >= TCP_FIFO_SIZE) {
    fifo->tail = 0;
  }
  fifo->count--;
  return tcp;
}

static size_t get_line(tcp_connect_t *tcp, char *buf, size_t size) {
  size_t i = 0;
  while (i < size) {
    char c;
    if (tcp_connect_read(tcp, (uint8_t *) &c, 1) > 0) {
      if (c == '\n') {
        break;
      }
      if (c != '\n' && c != '\r') {
        buf[i] = c;
        i++;
      }
    }
    net_poll();
  }
  buf[i] = '\0';
  return i;
}

static size_t http_send(tcp_connect_t *tcp, const char *buf, size_t size) {
  size_t send = 0;
  while (send < size) {
    send += tcp_connect_write(tcp, (const uint8_t *) buf + send, size - send);
    net_poll();
    Dbg("http: write %zu, target size=%zu", send, size);
  }
  return send;
}

static void http_send_content(tcp_connect_t *tcp, const char *content_type, const char *content, size_t size) {
  char tx_buffer[4096];
  sprintf(tx_buffer, "HTTP/1.1 200 OK\n"
                     "Content-Length: %zu\n"
                     "Content-Type: %s\n"
                     "Server: ChiServer\n", size, content_type);
  size_t len = strlen(tx_buffer);
  memcpy(tx_buffer + len, content, size);
  http_send(tcp, tx_buffer, len + size);
}

static void close_http(tcp_connect_t *tcp) {
  tcp_connect_close(tcp);
  Log("http closed.");
}

static bool send_local_file(tcp_connect_t *tcp, FILE *f, const char *content_type) {
  if (!f) {
    Err("http: Not Found!");
    return false;
  }
  char tx_buffer[1024];
  fseek(f, 0, SEEK_END);
  size_t filesize = ftell(f);
  fseek(f, 0, SEEK_SET);
  sprintf(tx_buffer, "HTTP/1.1 200 OK\n"
                     "Content-Length: %zu\n"
                     "Content-Type: %s\n"
                     "Server: ChiServer/0.1\n\n", filesize, content_type);
  size_t len = strlen(tx_buffer);
  Assert(http_send(tcp, tx_buffer, len) == len, "Cannot write http headers!");
  size_t sz;
  Log("http: header size %zu, file size %zu", len, filesize);
  do {
    // sz = fread(tx_buffer + len, 1, sizeof(tx_buffer) - len, f);
    // if (sz) http_send(tcp, tx_buffer, sz + len);
    sz = fread(tx_buffer, 1, sizeof(tx_buffer), f);
    Dbg("http: read static file for %zu bytes", sz);
    if (sz) http_send(tcp, tx_buffer, sz);
  } while (sz);
  return true;
}

bool str_endswith(const char *s, const char *patten) {
  if (patten == NULL || s == NULL) return false;
  int len = strlen(patten);
  s = s + strlen(s) - len;
  while (*s && *patten && *s == *patten) {
    s++;
    patten++;
  }
  return *s == '\0' && *patten == '\0';
}

static void send_file(tcp_connect_t *tcp, const char *url) {
  // FILE *file;
  // uint32_t size;
  const char *static_path = XHTTP_DOC_DIR;
  char file_path[255];
  const char content_404[] = "HTTP/1.1 404 NOT FOUND\n"
                             "Content-Type: text/html\n"
                             "Content-Length: 233\n"
                             "Server: ChiServer/0.1\n"
                             "\n"
                             "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">\n"
                             "<title>404 Not Found</title>\n"
                             "<h1>Not Found</h1>\n"
                             "<p>The requested URL was not found on the server.  If you entered the URL manually please check your spelling and try again.</p>";

  /*
  解析url路径，查看是否是查看XHTTP_DOC_DIR目录下的文件
  如果不是，则发送404 NOT FOUND
  如果是，则用HTTP/1.0协议发送

  注意，本实验的WEB服务器网页存放在XHTTP_DOC_DIR目录中
  */

  char *content_type = "text/html";
  if (!*url) return;
  if (*url == '/' && *(url + 1) == '\0') {
    sprintf(file_path, "%s/%s", static_path, "index.html");
  } else {
    if (*url == '/') sprintf(file_path, "%s/%s", static_path, url + 1);
    else sprintf(file_path, "%s/%s", static_path, url);
  }
  FILE *f = fopen(file_path, "rb");
  if (str_endswith(file_path, ".jpg")) {
    content_type = "image/jpeg";
  } else if (str_endswith(file_path, ".css")) {
    content_type = "text/css";
  }
  Log("http: static file %s, content_type %s", file_path, content_type);
  if (!send_local_file(tcp, f, content_type)) {
    http_send(tcp, content_404, sizeof(content_404));
  }
}

static void http_handler(tcp_connect_t *tcp, connect_state_t state) {
  if (state == TCP_CONN_CONNECTED) {
    http_fifo_in(&http_fifo_v, tcp);
    Ok("http conntected.");
  } else if (state == TCP_CONN_DATA_RECV) {
  } else if (state == TCP_CONN_CLOSED) {
    Log("http closed.");
  } else {
    assert(0);
  }
}


// 在端口上创建服务器。

int http_server_open(uint16_t port) {
  if (!tcp_open(port, http_handler)) {
    return -1;
  }
  http_fifo_init(&http_fifo_v);
  return 0;
}

// 从FIFO取出请求并处理。新的HTTP请求时会发送到FIFO中等待处理。

void http_server_run(void) {
  tcp_connect_t *tcp;
  char rx_buffer[1024];

  while ((tcp = http_fifo_out(&http_fifo_v)) != NULL) {
    /*
    1、调用get_line从rx_buffer中获取一行数据，如果没有数据，则调用close_http关闭tcp，并继续循环
    */

    if (get_line(tcp, rx_buffer, sizeof(rx_buffer)) == 0) {
      close_http(tcp);
      continue;
    }
    Dbg("http: first line %s", rx_buffer);

    /*
    2、检查是否有GET请求，如果没有，则调用close_http关闭tcp，并继续循环
    */

    char *p = strstr(rx_buffer, "GET");
    if (p == NULL) {
      close_http(tcp);
      continue;
    }

    /*
    3、解析GET请求的路径，注意跳过空格，找到GET请求的文件，调用send_file发送文件
    */

    p += 3;
    while (*p && *p == ' ') p++;
    char *path = p;
    while (*p && *p != ' ') p++;
    *p = '\0';
    Dbg("http: got path %s", path);
    send_file(tcp, path);

    /*
    4、调用close_http关掉连接
    */

    close_http(tcp);
    continue;

    Err("!! final close\n");
  }
}
