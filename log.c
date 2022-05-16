#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <time.h>
#include <string.h>
#include <limits.h>

#define TEST_OK  0     /* テスト関数戻り値(正常)*/
#define TEST_NG -1     /* テスト関数戻り値(異常)*/
#define LOG_FILE     "./_log.txt"        /* ログディレクトリ(通常)  */
#define ERR_LOG_FILE "./_error_log.txt"    /* ログディレクトリ(エラー)*/

FILE *log_file;        /* 通常ログ */
FILE *err_log_file;    /* 異常ログ */

/* 関数プロトタイプ宣言 */
//int test_function(int num);                    /* テスト関数        */
//void LOG_PRINT(char log_txt[256], ...);        /* 通常ログ出力関数  */
//void ERR_LOG_PRINT(char err_txt[256], ...);    /* エラーログ出力関数*/

/*
int main(void)
{

        int ret = TEST_OK;
        int num = 0;

        LOG_PRINT(">>> TEST START !!! \n");

        ret = test_function(num);
        if(ret != TEST_OK)
        {
                ERR_LOG_PRINT("test_function() NG!\n");
                LOG_PRINT("<<< TEST NG END !!!\n");
                return TEST_NG;
        }

        LOG_PRINT("<<< TEST END !!!\n");

        return TEST_OK;
}
*/

int test_function(int num)
{

        int ret = TEST_OK;
        LOG_PRINT(">>>>>> test_function() START !!! \n");

        if(num <= 0)
        {
                ret = TEST_NG;
        }

        LOG_PRINT("<<<<<< test_func() END !!!\n");
        return ret;

}


void LOG_INTBIT(unsigned int v) {
  char log_txt[256];
  unsigned int mask = (int)1 << (sizeof(v) * CHAR_BIT - 1);
  int i = 0;
  do {
	  log_txt[i] = mask & v ? '1' : '0';
	  i++;
  } while (mask >>= 1);
  log_txt[i] = '\0';

  LOG_PRINT("%s\n",log_txt);
}

//void LOG_PRINT(char log_txt[256], ...)
void LOG_PRINT(char fmt[256], ...)
{
   char log_txt[256];

   va_list arg_ptr;                                                             
                                                                                
   va_start(arg_ptr, fmt);                                                      
   vsprintf(log_txt, fmt, arg_ptr);                                              
   va_end(arg_ptr);

        time_t timer;
        struct tm *date;
        char str[256];

        /* 時間取得 */
        timer = time(NULL);
        date = localtime(&timer);
        strftime(str, sizeof(str), "[%Y/%x %H:%M:%S] ", date);

        if ((log_file = fopen(LOG_FILE, "a")) == NULL) {
                ERR_LOG_PRINT("log file open error!!\n");
                exit(EXIT_FAILURE);        /* エラーの場合は通常、異常終了する */
        }

        /* 文字列結合 */
        strcat(str,log_txt);

        fputs(str, log_file);
        fclose(log_file); 

        return;

}

//void ERR_LOG_PRINT(char err_txt[256], ...)
void ERR_LOG_PRINT(char fmt[256], ...)
{
   char err_txt[256];

   va_list arg_ptr;                                                             
                                                                                
   va_start(arg_ptr, fmt);                                                      
   vsprintf(err_txt, fmt, arg_ptr);                                              
   va_end(arg_ptr);

        time_t timer;
        struct tm *date;
        char str[256];

        /* 時間取得 */
        timer = time(NULL);
        date = localtime(&timer);
        strftime(str, sizeof(str), "[%Y/%x %H:%M:%S] ", date);

        if ((err_log_file = fopen(ERR_LOG_FILE, "a")) == NULL) {
                printf("ERR_LOG_FILE OPENERROR !!\n");
                exit(EXIT_FAILURE);        /* エラーの場合は通常、異常終了する */
        }

        /* 文字列結合 */
        strcat(str,err_txt);

        fputs(str, err_log_file);
        fclose(err_log_file); 

        return;

}
