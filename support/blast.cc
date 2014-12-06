#include <string>
#include <atomic>
#include <vector>
#include <unistd.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <signal.h>
#include <sys/socket.h>
#include <arpa/inet.h>

static int nc;
static int rpc;
static const char* url;
static struct sockaddr_in remote;
static std::atomic<uint64_t> num_responses = ATOMIC_VAR_INIT(0);
static struct timeval t0, t1;

static void print_stats()
{
  struct timeval t;
  gettimeofday(&t, 0);
  double tdiff0 = (t.tv_sec - t0.tv_sec) + 1e-6*(t.tv_usec - t0.tv_usec);
  double tdiff1 = (t.tv_sec - t1.tv_sec) + 1e-6*(t.tv_usec - t1.tv_usec);
  if (tdiff1 > 5) {
    printf("%d requests/sec\n", (int)(num_responses / tdiff0));
    t1 = t;
  }
}

static void send_query(int sock, const std::string& query)
{
  ssize_t bytes, tmp;
  for (bytes = 0; bytes < query.size(); bytes += tmp) {
    if ((tmp = send(sock, query.c_str() + bytes, query.size() - bytes, 0)) < 0) {
      perror("failed to send query");
      exit(1);
    }
  }
}

static std::vector<char> receive_response(int sock)
{
  std::vector<char> buf(64);
  for (ssize_t bytes = 0, tmp; ; ) {
    if ((tmp = recv(sock, &buf[0] + bytes, buf.size() - bytes, 0)) < 0) {
      perror("failed to recv response");
      exit(1);
    }
    bytes += tmp;
    if (bytes < buf.size())
      break;
    buf.resize(buf.size() * 2);
  }
  return buf;
}

static void* connection(void* arg)
{
  int id = (uintptr_t)arg;
  int sock;
  ssize_t bytes, tmp;

  if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
    perror("failed to create socket");
    exit(1);
  }

  if (connect(sock, (struct sockaddr*)&remote, sizeof(struct sockaddr)) < 0) {
    perror("failed to connect");
    exit(1);
  }

  std::string query = std::string("GET ") + url + " HTTP/1.1\r\n\r\n";

  for (int i = 0; i < rpc; i++)
    send_query(sock, query);

  while (1) {
    std::vector<char> buf = receive_response(sock);
    send_query(sock, query);
    std::atomic_fetch_add(&num_responses, uint64_t(1));

    if (id == 0)
      print_stats();
  }

  return NULL;
}

int main(int argc, char** argv)
{
  if (argc != 6) {
    printf("usage: blast <ip> <port> <url> <connections> <reqs per conn>\n");
    return 1;
  }

  remote.sin_family = AF_INET;
  remote.sin_port = htons(atoi(argv[2]));
  if (inet_pton(AF_INET, argv[1], &remote.sin_addr.s_addr) <= 0) {
    printf("can't set ip address %s\n", argv[1]);
    return 1;
  }

  url = argv[3];
  nc = atoi(argv[4]);
  rpc = atoi(argv[5]);
  gettimeofday(&t0, 0);
  t1 = t0;

  for (int i = 1; i < nc; i++) {
    pthread_t pt;
    if (pthread_create(&pt, 0, connection, (void*)(uintptr_t)i) < 0) {
      perror("can't make threads");
      return 1;
    }
  } 
  connection(0);
}
