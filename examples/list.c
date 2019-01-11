/* 链表函数 */
#include <string.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <float.h>
#include <limits.h>
#include <ctype.h>
#include "list.h"


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
int appendListTop(Node_t *h, struct node_data * data)
{
    Node_t *newNode;

    /* 第一步：分配新节点空间 */
    newNode = malloc(sizeof(Node_t));
    if (newNode == NULL)
    {
        return -1;
    }

    /* 第二步：赋值给新节点 */
    newNode->data = data;

    /* 第三步：把新节点插入到链表头 */
    newNode->next = h->next;	// 先改变自己的指针
    h->next = newNode;			// 再改变头节点的指针，顺序不能调

    return 0;					// 成功返回0
}

/**
 * @函数名: searchList
 * @函数功能: 在链表中查找数据,如果有多个匹配则返回第一个
 * @参数: h:链表头结点的地址 data:待查找的数据
 * @返回值: 查找成功返回数据所在节点的地址,失败返回NULL
 */
Node_t *searchList(const Node_t *h, int fd)
{
    Node_t *i;

    for (i = h->next; i != NULL; i = i->next)
    {
        if (i->data->fd == fd)
        {
            return i;
        }
    }

    return NULL;
}

/**
 * @函数名: deleteListOne
 * @函数功能: 在链表中删除指定值的节点，如果有多个匹配只删除第一个
 * @参数: h:链表头结点的地址 data:待删除的数据
 * @返回值: void
 */
void deleteListOne(Node_t *h, int fd)
{
    Node_t *slow = h;		// 慢指针
    Node_t *fast = h->next;	// 快指针

    for ( ; fast != NULL; slow = fast, fast = fast->next)
    {
        if (fast->data->fd == fd)
        {
            slow->next = fast->next;	// 通过摘掉节点
            free(fast);					// 和释放节点空间达到删除目的
            break;
        }
    }
}
