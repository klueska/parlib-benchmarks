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

struct sample {
  uint64_t tag;
  double recv_start;
  double recv_stop;
  double send_start;
  double send_stop;
};

static int nc;
static int rpc;
static const char* url;
static struct sockaddr_in remote;
static std::atomic<uint64_t> num_tags = ATOMIC_VAR_INIT(0);
static double t0, t1;
static uint64_t max_samples;

static double gettime()
{
  struct timeval t;
  gettimeofday(&t, 0);
  return t.tv_sec + 1e-6*t.tv_usec;
}

static void print_stats()
{
  double t = gettime();
  if (t - t1 >= 5) {
    printf("%d requests/sec\n", (int)(num_tags / (t - t0)));
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

  std::vector<sample>& samples = *(new std::vector<sample>(rpc));
  for (int i = 0; i < rpc; i++) {
    samples[i].tag = std::atomic_fetch_add(&num_tags, uint64_t(1));
    samples[i].send_start = gettime();
    send_query(sock, query);
    samples[i].send_stop = gettime();
  }

  for (uint64_t i = rpc; ; i++) {
    samples[i - rpc].recv_start = gettime();
    std::vector<char> buf = receive_response(sock);
    samples[i - rpc].recv_stop = gettime();
    if (max_samples && samples[i - rpc].tag >= max_samples)
      break;

    samples.resize(i + 1);
    samples[i].tag = std::atomic_fetch_add(&num_tags, uint64_t(1));
    samples[i].send_start = samples[i - rpc].recv_stop;
    send_query(sock, query);
    samples[i].send_stop = gettime();

    if (!max_samples && id == 0)
      print_stats();
  }

  return &samples;
}

int main(int argc, char** argv)
{
  if (argc < 6 || argc > 7) {
    printf("usage: blast <ip> <port> <url> <connections> <reqs per conn> [total reqs]\n");
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
  if (argc >= 7)
    max_samples = atoll(argv[6]);

  t0 = t1 = gettime();

  std::vector<pthread_t> threads(nc);
  for (int i = 0; i < nc; i++) {
    if (pthread_create(&threads[i], 0, connection, (void*)(uintptr_t)i) < 0) {
      perror("can't make threads");
      return 1;
    }
  }

  for (int i = 0; i < nc; i++) {
    std::vector<sample>* samples;
    pthread_join(threads[i], (void**)&samples);
    for (sample& x : *samples) if (x.tag < max_samples) {
      printf("EV_CALL_SEND_START:%lld:%f\n", (long long)x.tag, x.send_start);
      printf("EV_CALL_SEND_STOP:%lld:%f\n", (long long)x.tag, x.send_stop);
      printf("EV_CALL_RECV_START:%lld:%f\n", (long long)x.tag, x.recv_start);
      printf("EV_CALL_DESTROYED:%lld:%f\n", (long long)x.tag, x.recv_stop);
    }
  }

  return 0;
}
