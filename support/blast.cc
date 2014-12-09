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
static int burst;
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
    fprintf(stderr, "%d requests/sec\n", (int)(num_tags / (t - t0)));
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
  int iter = 0;
  std::vector<sample>& samples = *(new std::vector<sample>(0));

  while (1) {
    if ((sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP)) < 0) {
      perror("failed to create socket");
      continue; exit(1);
    }

    int yes = 1;
    if(setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes))) {
      perror("could not set sockopts");
      continue; exit(1);
    }

    if (connect(sock, (struct sockaddr*)&remote, sizeof(struct sockaddr)) < 0) {
      perror("failed to connect");
      continue; exit(1);
    }

    std::string query = std::string("GET ") + url + " HTTP/1.1\r\n" 
	                    + "User-Agent: httperf/0.9.1\r\n"
	                    + "Host: " + inet_ntoa(remote.sin_addr) + "\r\n\r\n";

    samples.resize(samples.size() + burst);
    for (int i = 0; i < burst; i++) {
      uint64_t idx = iter*rpc + i;
      samples[idx].send_start = gettime();
      send_query(sock, query);
      samples[idx].send_stop = gettime();
    }

    for (uint64_t i = 0; i < rpc; i++) {
      uint64_t idx = iter*rpc + i;
      samples[idx].recv_start = gettime();
      std::vector<char> buf = receive_response(sock);
      samples[idx].recv_stop = gettime();
      samples[idx].tag = std::atomic_fetch_add(&num_tags, uint64_t(1));

      if (max_samples && samples[idx].tag >= max_samples) {
        samples.resize(idx);
        return &samples;
      }

      if (i == (rpc - 1))
        break;

      if (samples.size() <= idx + 1)
        samples.resize(idx + 2);
      samples[idx + 1].send_start = samples[idx].recv_stop;
      send_query(sock, query);
      samples[idx + 1].send_stop = gettime();

      if (id == 0)
        print_stats();
    }
    close(sock);
    iter++;
  }

  return &samples;
}

int main(int argc, char** argv)
{
  if (argc < 7 || argc > 8) {
    printf("usage: blast <ip> <port> <url> <connections> <reqs per conn> <burst length> [total reqs]\n");
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
  rpc = atol(argv[5]);
  burst = atoi(argv[6]);
  if (argc >= 8)
    max_samples = atoll(argv[7]);

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
