/* 链表函数 */
#ifndef	_LIST_H
#define	_LIST_H

#define CGI_BODY_LEN 1024;

typedef struct node_data
{
    int fd; //文件描述符
    int header;
    int header_len_remain;
    char body[1024];
    int body_len_remain;
    int data_empty;
} Node_data_t;

/* 封装链表节点 */
typedef struct node
{
    struct node_data * data;			// 节点数据指定为整型
    struct node *next;	// 下一个节点
} Node_t;



/**
 * @函数名: appendListTop
 * @函数功能: 插入节点到链表头
 * @参数: h:链表头结点的地址 data:待插入的数据
 * @返回值：插入成功返回0，失败返回非零
 */

/**
 * @函数名: searchList
 * @函数功能: 在链表中查找数据
 * @参数: h:链表头结点的地址 data:待查找的数据
 * @返回值: 查找成功返回数据所在节点的地址,失败返回NULL
 */

/**
 * @函数名: deleteListOne
 * @函数功能: 在链表中删除指定值的节点，如果有多个匹配只删除第一个
 * @参数: h:链表头结点的地址 data:待删除的数据
 * @返回值: void
 */

/**
 * @函数名: appendListTop
 * @函数功能: 插入节点到链表头
 * @参数: h:链表头结点的地址 data:待插入的数据
 * @返回值：插入成功返回0，失败返回非零
 */
int appendListTop(Node_t *h, struct node_data * data);

/**
 * @函数名: searchList
 * @函数功能: 在链表中查找数据,如果有多个匹配则返回第一个
 * @参数: h:链表头结点的地址 data:待查找的数据
 * @返回值: 查找成功返回数据所在节点的地址,失败返回NULL
 */
Node_t *searchList(const Node_t *h, int fd);

/**
 * @函数名: deleteListOne
 * @函数功能: 在链表中删除指定值的节点，如果有多个匹配只删除第一个
 * @参数: h:链表头结点的地址 data:待删除的数据
 * @返回值: void
 */
void deleteListOne(Node_t *h, int fd);


#endif	/* _LIST_H */


