#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>

#define DIR_PATH "./tmp"

static void rm(const char *path);

int 
main(int argc,
     char *argv[])
{
  if (argc != 2) {
    fprintf(stderr, "require one argument");
    exit(EXIT_FAILURE);
  }

  // 1. ディレクトリを開く
  DIR *dir = opendir(argv[1]);
  if (!dir) {
    perror(argv[0]);
    exit(EXIT_FAILURE);
  }

  // 2. ディレクトリ内に存在するファイルを一覧表示する
  printf("ファイルを一覧表示\n");
  struct dirent *ent;
  while ((ent = readdir(dir))) {
    printf("%s\n", ent->d_name);
  }

  if (errno) {
    perror(argv[0]);
    exit(EXIT_FAILURE);
  }

  // 3. ディレクトリを閉じる
  if (closedir(dir)) {
    perror(argv[0]);
    exit(EXIT_FAILURE);
  }
/*
  rm(DIR_PATH);

  // 4. ディレクトリを作成する
  printf("%s を作成します\n", DIR_PATH);
  if (mkdir(DIR_PATH, 0755)) {
    perror(argv[0]);
    exit(EXIT_FAILURE);
  }
*/
  exit(EXIT_SUCCESS);
}

// 対象のパスにファイルが存在する場合、
// 普通のファイルまたはディレクトリであれば削除
void 
rm(const char *path)
{
  struct stat buf;
  if (!stat(path, &buf)) {
    // stat関数は失敗するとerrnoを変更する
    // 後続処理のためにリセット
    errno = 0;
    printf("%s を削除します\n", path);

    if (S_ISDIR(buf.st_mode)) {
      if (rmdir(path)) {
        perror(path);
        exit(EXIT_FAILURE);
      }
    } else if (S_ISREG(buf.st_mode)) {
      if (unlink(path)) {
        perror(path);
        exit(EXIT_FAILURE);
      }
    } else {
      fprintf(stderr, "削除できませんでした\n");
      exit(EXIT_FAILURE);
    }
  }
}
