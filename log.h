//#include <stdio.h>
//#include <stdlib.h>
//#include <time.h>
//#include <string.h>
//
//#define TEST_OK  0     /* テスト関数戻り値(正常)*/
//#define TEST_NG -1     /* テスト関数戻り値(異常)*/
//#define LOG_FILE     "LOG/log.txt"        /* ログディレクトリ(通常)  */
//#define ERR_LOG_FILE "LOG/err_log.txt"    /* ログディレクトリ(エラー)*/
//
//FILE *log_file;        /* 通常ログ */
//FILE *err_log_file;    /* 異常ログ */

/* 関数プロトタイプ宣言 */
int test_function(int num);                    /* テスト関数        */
void LOG_INTBIT(unsigned int); 
void LOG_PRINT(char log_txt[256], ...);        /* 通常ログ出力関数  */
void ERR_LOG_PRINT(char err_txt[256], ...);    /* エラーログ出力関数*/

