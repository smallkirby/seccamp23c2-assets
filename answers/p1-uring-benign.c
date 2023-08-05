#include <assert.h>
#include <fcntl.h>
#include <liburing.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define UNIMPLEMENTED() assert(0 && "unimplemented")
#define ulong unsigned long

/**
 * /proc/self/mapsを開く
 *
 * @return int /proc/self/mapsのfd
 */
int open_maps(void) {
  int fd = open("/proc/self/maps", O_RDONLY);
  assert(fd >= 2);
  return fd;
}

/**
 * @brief io_uringをセットアップする
 *
 * @param ring io_uring
 */
void init_uring(struct io_uring *ring) { io_uring_queue_init(0x100, ring, 0); }

/**
 * @brief io_uringを介してreadする
 *
 * @param ring io_uring
 * @param fd readしたいファイルのfd
 * @param buf read先のバッファ
 * @param size readするサイズ
 * @param offset readするオフセット
 */
void ring_submit_read(struct io_uring *ring, int fd, char *buf, size_t size,
                      ulong offset) {
  struct io_uring_sqe *sqe = NULL;

  // 利用可能なSQEを取得する
  sqe = io_uring_get_sqe(ring);
  // SQEにリクエストを詰める
  io_uring_prep_read(sqe, fd, buf, size, offset);
  // kernelに通知する
  io_uring_submit(ring);
}

/**
 * @brief CQにレスポンスが返ってくるまで待つ
 *
 * @param ring io_uring
 */
void ring_wait_read(struct io_uring *ring) {
  struct io_uring_cqe *cqe = NULL;

  // レスポンスが返ってきたCQEを取得する
  io_uring_wait_cqe(ring, &cqe);
  assert(cqe->res >= 0);
  io_uring_cqe_seen(ring, cqe);
}

int main(void) {
#define BUF_SIZE 0x50
  char buf1[BUF_SIZE + 1] = {0};
  char buf2[BUF_SIZE + 1] = {0};
  char buf3[BUF_SIZE + 1] = {0};

  struct io_uring ring;
  struct io_uring_sqe *sqe = NULL;
  struct io_uring_cqe *cqe = NULL;
  int fd = open_maps();

  // ringの初期化
  init_uring(&ring);

  // SQにリクエストを投げる
  ring_submit_read(&ring, fd, buf1, BUF_SIZE, 0 * BUF_SIZE);
  ring_submit_read(&ring, fd, buf2, BUF_SIZE, 1 * BUF_SIZE);
  ring_submit_read(&ring, fd, buf3, BUF_SIZE, 2 * BUF_SIZE);

  // CQからリクエストの完了を待つ
  for (int ix = 0; ix != 3; ++ix) ring_wait_read(&ring);

  // readしたバッファを出力する
  printf("%s%s%s\n", buf1, buf2, buf3);

  return 0;
}
