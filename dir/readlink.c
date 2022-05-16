#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>

/*!
 * @brief     ファイルがシンボリックリンクであるか確認する。
 * @param[in] filepath  ファイルパス
 * @return     1        シンボリックリンクである
 * @return     0        シンボリックリンクでない
 * @return    -1        Error
 */
static int
is_symlink(char *filepath)
{
    struct stat sb = {0};
    int rc = 0;

    rc = lstat(filepath, &sb);
    if(rc < 0){
        printf("Error: stat() %s: %s\n", strerror(errno), filepath);
        return(-1);
    }

    if(S_ISLNK(sb.st_mode)) return(1);
    return(0);
}

static int
read_symlink(char *filepath)
{
    ssize_t len = 0;
    char buf[BUFSIZ] = {"\0"};
    int rc = 0;

    rc = is_symlink(filepath);
    if(rc != 1){
        printf("Error: Not symbolic link: %s\n", filepath);
        return(-1);
    }
    
    len = readlink(filepath, buf, sizeof(buf) - 1);
    if(len < 0){
        printf("Error: readlink() %s: %s\n", strerror(errno), filepath);
        return(-1);
    }
    buf[len] = '\0';

    printf("%s\n", buf);

    return(0);
}

int
main(int argc, char *argv[])
{
    int rc = 0;

    if(argc != 2){
        printf("Usage: %s <path>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    rc = read_symlink(argv[1]);
    if(rc != 0) exit(EXIT_FAILURE);

    exit(EXIT_SUCCESS);
}
