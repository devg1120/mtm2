// tree.c
#include <stdlib.h>
#include <string.h>
#include "tree.h"

typedef struct attr{
    int id;
    //enum direct;
    direct_t  direct;
    char *cwd;
    char *exe;
}attr_t;

typedef struct node{
    struct attr* pAttr;
    struct node* pParentNode;
    struct node* pLeftNode;
    struct node* pRightNode;
    struct node* pUpNode;
    struct node* pDownNode;
}node_t;

typedef struct tree{
    node_t* pRootNode;
}tree_t;

void printfNode(node_t* pNode){
    if(pNode == NULL){
        printf("No node\n");
        return;
    }
    
    printf("%d\n", pNode->pAttr->id);
    printf("Left  ");
    printfNode(pNode->pLeftNode);
    printf("Right ");
    printfNode(pNode->pRightNode);
    
    return;
}

void printfTree(tree_t* pTree){
    if(pTree == NULL){
        printf("arg chk error");
        return;
    }
    printf("Root  ");
    printfNode(pTree->pRootNode);
    printf("\n");
    
    return;
}

tree_t* initTree(){
    tree_t* pTree = NULL;
    
    pTree = (tree_t*)malloc(sizeof(tree_t));
    if(pTree == NULL){
        printf("malloc error\n");
        return NULL;
    }
    pTree->pRootNode = NULL;
    
    return pTree;
}

node_t* getRootNode(tree_t * t){
    return t->pRootNode;
}

void setRootNode(tree_t* t, node_t* n){
     t->pRootNode = n;
}

node_t* makeNode(){
    node_t* pNode = NULL;
    attr_t* pAttr = NULL;
    
    pNode = (node_t*)malloc(sizeof(node_t));
    pAttr = (attr_t*)malloc(sizeof(attr_t));
    if(pNode == NULL || pAttr == NULL){
        printf("malloc error\n");
        return NULL;
    }
    pNode->pAttr = pAttr;
    pNode->pAttr->id = -1;
    pNode->pAttr->direct = NONE;
    pNode->pAttr->cwd = NULL;
    pNode->pAttr->exe = NULL;
    pNode->pParentNode = NULL;
    pNode->pLeftNode = NULL;
    pNode->pRightNode = NULL;
    pNode->pUpNode = NULL;
    pNode->pDownNode = NULL;
    
    return pNode;
}

void setChildNode(node_t* p, node_t* c1, node_t* c2)
{
   if (p->pAttr->direct == HORIZONTAL)
   {
	   p->pLeftNode  = c1;
	   p->pRightNode = c2;

   } else if (p->pAttr->direct == VERTICAL) 
   {
	   p->pUpNode    = c1;
	   p->pDownNode  = c2;
   }
   c1->pParentNode = p;
   c2->pParentNode = p;

}

void freeNode(node_t* pNode){
    if(pNode == NULL){
        return;
    }
    freeNode(pNode->pLeftNode);
    freeNode(pNode->pRightNode);
    freeNode(pNode->pUpNode);
    freeNode(pNode->pDownNode);
    free(pNode->pAttr);
    free(pNode);
    
    return;
}

int freeNodeInTree(tree_t* pTree){
    if(pTree == NULL){
        printf("arg chk error");
        return -1;
    }
    freeNode(pTree->pRootNode);
    pTree->pRootNode = NULL;
    //printfTree(pTree);
    
    return 0;
}

int walk_node(node_t* n, int c, int lv) {

   char *indent_fmt[256];
   char *indent[256];
   lv ++;
   sprintf(indent_fmt, "%s%d%s", "%-", lv*4 , "s");
   sprintf(indent, indent_fmt," ");


   char *direct[16];
   char *cwd[16];
   switch(n->pAttr->direct) {
       case HORIZONTAL:
	       sprintf(direct,"%s","HORIZONTAL");
	       break;
       case VERTICAL:
	       sprintf(direct,"%s","VERTICAL");
	       break;
       case VIEW:
	       sprintf(direct,"%s","VIEW");
	       sprintf(cwd,"%s",n->pAttr->cwd);
	       break;
       case NONE:
	       sprintf(direct,"%s","none");
	       break;
       default:
	       sprintf(direct,"%s","-");
   }

   switch(n->pAttr->direct) {
       case VIEW:
           if( n->pParentNode != NULL )
           {
             printf("id: %-7d %s %-10s parent:%d cwd:%s\n",
                	     n->pAttr->id,indent, direct, n->pParentNode->pAttr->id, cwd);
           } else {
             printf("id: %-7d %s %-10s cwd:%s\n",n->pAttr->id,indent, direct, cwd );

           }
	   break;
       default:
           if( n->pParentNode != NULL )
           {
             printf("id: %-7d %s %-10s parent:%d\n",
                	     n->pAttr->id,indent, direct, n->pParentNode->pAttr->id);
           } else {
             printf("id: %-7d %s %-10s \n",n->pAttr->id,indent, direct );

           }
	   break;
   }
   
   c++;

   if ( n->pLeftNode != NULL ) {
            c = walk_node( n->pLeftNode  ,c, lv);
   } 
   if ( n->pRightNode != NULL ) {
            c = walk_node( n->pRightNode ,c, lv);
   } 
   if ( n->pUpNode    != NULL ) {
            c = walk_node( n->pUpNode    ,c, lv);
   } 
   if ( n->pDownNode  != NULL ) {
            c = walk_node( n->pDownNode  ,c, lv);
   }
   return c;
}

int printTree(tree_t* pTree){
    node_t* pNode = NULL;
    int level = 1;
    
    if(pTree == NULL){
        printf("arg chk error");
        return -1;
    }
    if(pTree->pRootNode == NULL){
        printf("No node\n");
        return -1;
    }
    
    pNode = pTree->pRootNode;
    int n ;
    n = walk_node(pNode, 0,-1);
    printf("nodes=%d \n", n);

/*
id: 1         HORIZONTAL 
id: 11           VERTICAL   parent:1
id: 111              VIEW       parent:11            1 root
id: 112              VIEW       parent:11            3 root:c1:split(V)
id: 12           VERTICAL   parent:1
id: 121              HORIZONTAL parent:12
id: 1211                 VIEW       parent:121       2 root:split(H)
id: 1212                 VIEW       parent:121       5 root:c2:c1:split(H)
id: 122              VIEW       parent:12            4 root:c2:split(V)

nodes=9 
ALL:9
VIEW:5
HORIZONTAL:2
VERTICAL:2
DEPTH:4
*/
    
    return -1;
}

node_t* find_2nd_child( node_t* n);
node_t* find_node(node_t* n, direct_t d);

int gen_node(node_t* n, int c, int lv) {

   char *indent_fmt[256];
   char *indent[256];
   lv ++;
   sprintf(indent_fmt, "%s%d%s", "%-", lv*4 , "s");
   sprintf(indent, indent_fmt," ");


   char *direct[16];
   switch(n->pAttr->direct) {
       case HORIZONTAL:
	       sprintf(direct,"%s","HORIZONTAL");
	       break;
       case VERTICAL:
	       sprintf(direct,"%s","VERTICAL");
	       break;
       case VIEW:
	       sprintf(direct,"%s","VIEW");
	       break;
       case NONE:
	       sprintf(direct,"%s","none");
	       break;
       default:
	       sprintf(direct,"%s","-");
   }

   node_t* split_n = NULL;
   if (n->pAttr->direct == HORIZONTAL || n->pAttr->direct == VERTICAL)
   {
         split_n = find_2nd_child(n);
         if (split_n->pAttr->cwd == NULL)
         {
           split_n = find_node(split_n, VIEW);
         }
            if( n->pParentNode != NULL )
            {
              printf("id: %-7d %s %-10s parent:%d \n",
                 	     n->pAttr->id,indent, direct, n->pParentNode->pAttr->id );
            } else {
              printf("id: %-7d %s %-10s \n",n->pAttr->id,indent, direct );

            }
	     printf("                            cwd:%s\n", split_n->pAttr->cwd);
   }
/*
   switch(n->pAttr->direct) {
       case HORIZONTAL:
       case VERTICAL:

            if( n->pParentNode != NULL )
            {
              printf("id: %-7d %s %-10s parent:%d \n",
                 	     n->pAttr->id,indent, direct, n->pParentNode->pAttr->id );
            } else {
              printf("id: %-7d %s %-10s \n",n->pAttr->id,indent, direct );

            }
	     printf("                            cwd:%s\n", split_n->pAttr->cwd);
    }
    */

   c++;

   if ( n->pLeftNode != NULL ) {
            c = gen_node( n->pLeftNode  ,c, lv);
   } 
   if ( n->pRightNode != NULL ) {
            c = gen_node( n->pRightNode ,c, lv);
   } 
   if ( n->pUpNode    != NULL ) {
            c = gen_node( n->pUpNode    ,c, lv);
   } 
   if ( n->pDownNode  != NULL ) {
            c = gen_node( n->pDownNode  ,c, lv);
   }
   return c;
}

int genTree(tree_t* pTree){
    node_t* pNode = NULL;
    int level = 1;
    
    if(pTree == NULL){
        printf("arg chk error");
        return -1;
    }
    if(pTree->pRootNode == NULL){
        printf("No node\n");
        return -1;
    }
    
    pNode = pTree->pRootNode;
    int n ;
    n = gen_node(pNode, 0,-1);
    printf("nodes=%d \n", n);
}

int count_node(node_t* n, int c, direct_t d) {
   if ( d == NONE)
   {
         c++;
   } else if ( n->pAttr->direct == d )
   {
         c++;
   }
   if ( n->pLeftNode != NULL ) {
            c = count_node( n->pLeftNode  ,c, d);
   } 
   if ( n->pRightNode != NULL ) {
            c = count_node( n->pRightNode ,c, d);
   } 
   if ( n->pUpNode    != NULL ) {
            c = count_node( n->pUpNode    ,c, d);
   } 
   if ( n->pDownNode  != NULL ) {
            c = count_node( n->pDownNode  ,c, d);
   }
   return c;
}

int count_depth(node_t* n, int c ) {
   int c1 = 0;
   int c2 = 0;
   int c3 = 0;
   int c4 = 0;
   int max = 0;
   c++;
   max = c;

   if ( n->pLeftNode != NULL ) {
            c1 = count_depth( n->pLeftNode  ,c);
	    if ( c1 > max ) max = c1;
   } 
   if ( n->pRightNode != NULL ) {
            c2 = count_depth( n->pRightNode ,c);
	    if ( c2 > max ) max = c2;
   } 
   if ( n->pUpNode    != NULL ) {
            c3 = count_depth( n->pUpNode    ,c);
	    if ( c3 > max ) max = c3;
   } 
   if ( n->pDownNode  != NULL ) {
            c4 = count_depth( n->pDownNode  ,c);
	    if ( c4 > max ) max = c4;
   }
   return max;
}

node_t* find_1st_child( node_t* n)
{
  switch (n->pAttr->direct) {

     case HORIZONTAL:
	             return  n->pLeftNode;
		     break;
     case VERTICAL:
	             return  n->pUpNode;
		     break;

  }
   return NULL;
}
node_t* find_2nd_child( node_t* n)
{
  switch (n->pAttr->direct) {

     case HORIZONTAL:
	             return  n->pRightNode;
		     break;
     case VERTICAL:
	             return  n->pDownNode;
		     break;

  }
   return NULL;
}

node_t* find_node(node_t* n, direct_t d) {

   if ( n->pAttr->direct == d )
   {
       return  n;
   }
   node_t* r = NULL;

   if ( n->pLeftNode != NULL ) {
            r = find_node( n->pLeftNode  ,d);
            if (r != NULL) return r;
   } 
   if ( n->pRightNode != NULL ) {
            r = find_node( n->pRightNode ,d);
            if (r != NULL) return r;
   } 
   if ( n->pUpNode    != NULL ) {
            r = find_node( n->pUpNode    ,d);
            if (r != NULL) return r;
   } 
   if ( n->pDownNode  != NULL ) {
            r = find_node( n->pDownNode  ,d);
            if (r != NULL) return r;
   }
   return NULL;
}
int main(){
    tree_t* pTree = NULL;    
    
    pTree = initTree();

    node_t* root = makeNode();
    root->pAttr->direct = HORIZONTAL;
    root->pAttr->id     = 1;

    setRootNode(pTree, root);
    //node_t* root = getRootNode(pTree);

    node_t* child1  = makeNode();
    child1->pAttr->direct = VERTICAL;
    child1->pAttr->id     = 11;

    node_t* child2  = makeNode();
    child2->pAttr->direct = VERTICAL;
    child2->pAttr->id     = 12;

    setChildNode(root, child1, child2);

    node_t* child11  = makeNode();
    child11->pAttr->direct = VIEW;
    child11->pAttr->id     = 111;
    child11->pAttr->cwd     = "/home/gusa1120/tmp/mtm";

    node_t* child12  = makeNode();
    child12->pAttr->direct = VIEW;
    child12->pAttr->id     = 112;
    child12->pAttr->cwd    = "/usr";

    setChildNode(child1, child11, child12);

    node_t* child21  = makeNode();
    child21->pAttr->direct = HORIZONTAL;
    child21->pAttr->id     = 121;

    node_t* child22  = makeNode();
    child22->pAttr->direct = VIEW;
    child22->pAttr->id     = 122;
    child22->pAttr->cwd    = "/home/gusa1120";

    setChildNode(child2, child21, child22);

    node_t* child211  = makeNode();
    child211->pAttr->direct = VIEW;
    child211->pAttr->id     = 1211;
    child211->pAttr->cwd    = "/";

    node_t* child212  = makeNode();
    child212->pAttr->direct = VIEW;
    child212->pAttr->id     = 1212;
    child212->pAttr->cwd    = "/var/log";

    setChildNode(child21, child211, child212);

    printTree(pTree);

    node_t *root2 = getRootNode(pTree );
    int na = count_node(root2,0, NONE);
    int nv = count_node(root2,0, VIEW);
    int nh = count_node(root2,0, HORIZONTAL);
    int nt = count_node(root2,0, VERTICAL);
    printf("ALL:%d\n", na);
    printf("VIEW:%d\n", nv);
    printf("HORIZONTAL:%d\n", nh);
    printf("VERTICAL:%d\n", nt);

    int dp = count_depth(root2,0);
    printf("DEPTH:%d\n", dp);

/*
id: 1         HORIZONTAL 
id: 11           VERTICAL   parent:1
id: 111              VIEW       parent:11            1 root
id: 112              VIEW       parent:11            3 root:c1:split(V)
id: 12           VERTICAL   parent:1
id: 121              HORIZONTAL parent:12
id: 1211                 VIEW       parent:121       2 root:split(H)
id: 1212                 VIEW       parent:121       5 root:c2:c1:split(H)
id: 122              VIEW       parent:12            4 root:c2:split(V)

nodes=9 
ALL:9
VIEW:5
HORIZONTAL:2
VERTICAL:2
DEPTH:4


1   t_root[t_root_index] = newview(NULL, 0, 0, LINES-1, COLS, "~/tmp/mtm","nvim _log.txt");
3   split(t_root[0], HORIZONTAL,"/",NULL);    //4 root 
5   split(t_root[0]->c1, VERTICAL,"/usr",NULL);  //5 root left
7   split(t_root[0]->c2, VERTICAL,NULL,"/usr/local/bin/fish");  //6 root right
9   split(t_root[0]->c2->c1, HORIZONTAL, "/var/log",NULL);  //7 root right up

 9 % 2 = 4 (split回数)

*/

     node_t* t1 = find_1st_child(root2);
     node_t* t2 = find_2nd_child(root2);
     printf("root 1st: %d\n",t1->pAttr->id);
     printf("root 2nd: %d\n",t2->pAttr->id);

     node_t* v1 = find_node(t1, VIEW);
     node_t* v2 = find_node(t2, VIEW);

     printf("root 1st VIEW: %d\n",v1->pAttr->id); //111
     printf("root 2nd VIEW: %d\n",v2->pAttr->id); //1211

     node_t* t11 = find_1st_child(t1);
     node_t* t12 = find_2nd_child(t1);
     node_t* t21 = find_1st_child(t2);
     node_t* t22 = find_2nd_child(t2);
     printf("t1 1st: %d\n",t11->pAttr->id);
     printf("t1 2nd: %d\n",t12->pAttr->id);
     printf("t2 1st: %d\n",t21->pAttr->id);
     printf("t2 2nd: %d\n",t22->pAttr->id);

    genTree(pTree);

    freeNodeInTree(pTree);
    
    return 0;
}


