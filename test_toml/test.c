#ifdef NDEBUG
#undef NDEBUG
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdint.h>
#include <assert.h>
#include "toml.h"

/*
 * https://www.codetd.com/ja/article/12285063
 *
 */

typedef struct node_t node_t;
struct node_t {
    const char*   key;
    toml_table_t* tab;
};

node_t stack[20];
int stacktop = 0;

#define red_color "\E[1;31m"
#define color_suffix "\E[0m"
static void print_array_of_tables(toml_array_t* arr, const char* key, int lv);
static void print_array(toml_array_t* arr, int lv);
static void print_table(toml_table_t* curtab, int lv);
static void print_table_title(const char* arrname, int lv);
static indent( int lv);
    
static void parse_node(toml_table_t* curtab)
{
    int i;
    const char* key;
    const char* raw;
    toml_array_t* arr;
    toml_table_t* tab;
    int lv = 1;

    toml_datum_t direct = toml_string_in(curtab, "direct");
    printf("direct: %s\n", direct.u.s);

    if ( 0 == strcmp( direct.u.s ,"VIEW") )
    {
       toml_datum_t cwd = toml_string_in(curtab, "cwd");
       toml_datum_t exe = toml_string_in(curtab, "exe");
       printf("cwd: %s\n", cwd.u.s);
       printf("exe: %s\n", exe.u.s);
    }


    arr = toml_array_in(curtab, "CHILD");

    if (arr == 0 ) return;

    //print_array_of_tables(arr, key, 1);
    for (i = 0; 0 != (tab = toml_table_at(arr, i)); i++) {
	//print_table_title(key, 0);//[[key]]
	//print_table(tab, 0);
	parse_node(tab );
    }

}

static void parse_table(toml_table_t* curtab)
{
    int i;
    //const char* key;
    const char* raw;
    toml_array_t* arr;
    toml_table_t* tab;

    //printf("lv: %d\n",lv);
    arr = toml_array_in(curtab, "ROOT");
    //print_array_of_tables(arr, key, 1);
    for (i = 0; 0 != (tab = toml_table_at(arr, i)); i++) {
	//print_table_title(key, 0);//[[key]]
	//print_table(tab, 0);
       toml_datum_t index = toml_int_in(tab, "index");
       printf("ROOT index %d ", (int)index.u.i);
	parse_node(tab );
    }
}

    
static void parse_table_at(toml_table_t* curtab, int i)
{
    //const char* key;
    const char* raw;
    toml_array_t* arr;
    toml_table_t* tab;

    //printf("lv: %d\n",lv);
    arr = toml_array_in(curtab, "ROOT");
    //print_array_of_tables(arr, key, 1);
    tab = toml_table_at(arr, i);
	//print_table_title(key, 0);//[[key]]
	//print_table(tab, 0);
       toml_datum_t index = toml_int_in(tab, "index");
       printf("ROOT index %d ", (int)index.u.i);
	parse_node(tab );
    
}



static indent( int lv)
{
    for( int i = 0; i<=(lv-2); i++)
    {
        printf("    ");
    }

}
static void print_table_title(const char* arrname, int lv)
{
    int i;
    indent(lv);
    printf("*table_title:   %s\n", arrname );
}

static void print_table(toml_table_t* curtab, int lv)
{
    int i;
    const char* key;
    const char* raw;
    toml_array_t* arr;
    toml_table_t* tab;
    lv++;

    //printf("lv: %d\n",lv);

    for (i = 0; 0 != (key = toml_key_in(curtab, i)); i++) {
	if (0 != (raw = toml_raw_in(curtab, key))) {
            indent(lv);
	    printf(red_color"key = raw :"color_suffix);
	    printf("%s = %s\n", key, raw);
	} else if (0 != (arr = toml_array_in(curtab, key))) {
	    if (toml_array_kind(arr) == 't') {
            indent(lv);
	    	printf(red_color"array of table: "color_suffix);
		print_array_of_tables(arr, key, lv);
	    }
	    else {
            indent(lv);
		printf(red_color"array: "color_suffix);
		printf("%s = [\n", key);/*xx = [ss,sss]*/
		print_array(arr, lv);
		printf("    ]\n");
	    }
	} else if (0 != (tab = toml_table_in(curtab, key))) {
            indent(lv);
	    printf(red_color"table: "color_suffix);
	    stack[stacktop].key = key;
	    stack[stacktop].tab = tab;
	    stacktop++;
	    print_table_title(0, lv);
	    print_table(tab, lv);
	    stacktop--;
	} else {
	    abort();
	}
    }
}

static void print_array_of_tables(toml_array_t* arr, const char* key, int lv)
{
    int i;
    toml_table_t* tab;
	printf(red_color"tom_table_at\n"color_suffix);
    for (i = 0; 0 != (tab = toml_table_at(arr, i)); i++) {
	print_table_title(key, lv);//[[key]]
	print_table(tab, lv);
    }
}

static void print_array(toml_array_t* curarr, int lv)
{
    toml_array_t* arr;
    const char* raw;
    toml_table_t* tab;
    int i;

    switch (toml_array_kind(curarr)) {
    case 'v': 
	for (i = 0; 0 != (raw = toml_raw_at(curarr, i)); i++) {
	    printf("  %d: %s,\n", i, raw);
	}
	break;
    case 'a': 
	for (i = 0; 0 != (arr = toml_array_at(curarr, i)); i++) {
	    printf("  %d: \n", i);
	    print_array(arr,lv);
	}
	break;
	    
    case 't': 
	for (i = 0; 0 != (tab = toml_table_at(curarr, i)); i++) {
	    print_table(tab, lv);
	}
	printf("\n");
	break;
	
    case '\0':
	break;

    default:
	abort();
    }
}

static void
process(toml_table_t *tab) 
{
    //print_table(tab, 0);
    //parse_table(tab);
    parse_table_at(tab, 0);






    //print_array(tab);
    /*
    toml_table_t* Service = toml_table_in(tab, "Service");
    if (!Service) {
        error("missing [Service]", "");
    }
    toml_datum_t Port = toml_int_in(Service, "Port");
    if (!Port.ok) {
        error("cannot read Service.Port", "");
    }
    toml_datum_t StartupMsg = toml_string_in(Service, "StartupMsg");
    if (!StartupMsg.ok) {
        error("cannot read Service.StartMsg", "");
    }
*/
}
static void cat(FILE* fp)
{
    char  errbuf[200];
    toml_table_t* tab = toml_parse_file(fp, errbuf, sizeof(errbuf));
    if (!tab) {
	fprintf(stderr, "ERROR: %s\n", errbuf);
	return;
    }

    //stack[stacktop].tab = tab;
    //stack[stacktop].key = "";
    //stacktop++;
    //print_table(tab);
    //stacktop--;
    process(tab);
    toml_free(tab);
}

int main(int argc, const char* argv[])
{
    int i;
    if (argc == 1) {
	cat(stdin);
    } else {
	for (i = 1; i < argc; i++) {
	    FILE* fp = fopen(argv[i], "r");
	    if (!fp) {
		fprintf(stderr, "ERROR: cannot open %s: %s\n",
			argv[i], strerror(errno));
		exit(1);
	    }
	    cat(fp);
	    fclose(fp);
	}
    }
    return 0;
}
