#include	<stdio.h>
#include	<unistd.h>
#include	<time.h>
#include	<sys/types.h>
#include	<sys/stat.h>
 
struct	stat	buf;
struct	tm	*ts;
 
int main(ac,av)
int	ac;
char	**av;
{
	int	r, size, mode;
	char	*path, tf[128];
 
	if ( ac < 2 ) {
		fprintf(stderr,"No arguments.\n");
		return(-1);
	}
 
	path = av[1];	// パス名を取得
	fprintf(stderr,"path: %s\n",path);
 
//	r = stat(path,&buf);	// パスの属性を取得
	r = lstat(path,&buf);	// パスの属性を取得
	if ( r != 0 ) {		// エラー処理
		fprintf(stderr,"error: stat = %d\n",r);
		return(-1);
	}
 
//	ts = localtime(&(buf.st_atime));	// 最終アクセス時刻を取得
	ts = gmtime(&(buf.st_atime));		// 最終アクセス時刻を取得
//	strftime(tf,sizeof(tf),"%Y/%m/%d(%a) %H:%M:%S(%z)",ts);
	strftime(tf,sizeof(tf),"%Y/%m/%d(%a) %H:%M:%S",ts);
	printf("Last access(st_atime)       = %s\n",tf);	// 表示
 
//	ts = localtime(&(buf.st_mtime));	// 最終修正時刻を取得
	ts = gmtime(&(buf.st_mtime));		// 最終修正時刻を取得
//	strftime(tf,sizeof(tf),"%Y/%m/%d(%a) %H:%M:%S(%z)",ts);
	strftime(tf,sizeof(tf),"%Y/%m/%d(%a) %H:%M:%S",ts);
	printf("Last modified(st_mtime)     = %s\n",tf);	// 表示
 
//	ts = localtime(&(buf.st_ctime));	// 最終状態変更時刻を取得
	ts = gmtime(&(buf.st_ctime));		// 最終状態変更時刻を取得
//	strftime(tf,sizeof(tf),"%Y/%m/%d(%a) %H:%M:%S(%z)",ts);
	strftime(tf,sizeof(tf),"%Y/%m/%d(%a) %H:%M:%S",ts);
	printf("Last state change(st_ctime) = %s\n",tf);	// 表示
 
	size = (int)(buf.st_size);	// サイズを取得
	printf("size: %d (bytes)\n",size);	// 表示
 
	mode = (int)(buf.st_mode);	// アクセス属性の取得
	printf("mode: %o\n",mode);	// 表示
 
	// ファイル／ディレクトリ／シンボリックリンクの判定
	printf("type: /");
	if ( S_ISREG(buf.st_mode) ) {
		printf("regular file/");
	}
	if ( S_ISDIR(buf.st_mode) ) {
		printf("directory/");
	}
	if ( S_ISLNK(buf.st_mode) ) {
		printf("simlink/");
	}
	printf("\n");
}
