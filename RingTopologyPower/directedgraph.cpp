#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define MAXN 36
struct
{
    int head[MAXN + 1];
    int nxt[6 * MAXN + 1];
    int to[6 * MAXN + 1];
    int tot;            /* 邻接表，最多 3n 条边 */
    int dist[MAXN + 1]; /* 对当前起点，dist[i] 为到 i 的最短距离 */
    int q[MAXN];        /* 手写队列，比 STL 快 */
    int qh;
    int qt;
    int nodeCount;
    int pileCount;
    char locked[MAXN + 1]; /* >0 表示被锁，节点编号 1..n */
} config;

static inline void add_edge(int u, int v)
{
    config.tot += 1;
    config.nxt[config.tot] = config.head[u];
    config.head[u] = config.tot;
    config.to[config.tot] = v;
}
/* 建图：每个节点连 左、右、对径 */
void build_graph(void)
{
    config.tot = 0;
    int n = config.nodeCount;
    memset(config.head, 0, sizeof(config.head));
    for (int u = 1; u <= n; ++u)
    {
        int v1 = (u == 1 ? n : u - 1);                /* 左邻居 */
        int v2 = (u == n ? 1 : u + 1);                /* 右邻居 */
        int v3 = (u > n / 2 ? u - n / 2 : u + n / 2); /* 对径 */
        add_edge(u, v1);
        add_edge(v1, u);
        add_edge(u, v2);
        add_edge(v2, u);
        add_edge(u, v3);
        add_edge(v3, u);
    }
}
/* 从 start 出发跑一次 BFS，结果写进全局 dist[] */
static void _bfs(int start)
{
    memset(config.dist, -1, sizeof(config.dist));
    config.qh = config.qt = 0;
    config.dist[start] = 0;
    config.q[config.qt++] = start;
    while (config.qh < config.qt)
    {
        int u = config.q[config.qh++];
        for (int e = config.head[u]; e; e = config.nxt[e])
        {
            int v = config.to[e];
            if (config.locked[v] > 0)
                continue; /* 锁节点直接跳过 */
            if (config.dist[v] == -1)
            {
                config.dist[v] = config.dist[u] + 1;
                config.q[config.qt++] = v;
            }
        }
    }
}

/* 从 start 出发跑一次 BFS，只计算到 locked==pileid 的点的距离 */
void bfs(int start, int pileid, bool find_type)
{
    memset(config.dist, -1, sizeof(config.dist));
    config.qh = config.qt = 0;
    config.dist[start] = 0;
    config.q[config.qt++] = start;
    while (config.qh < config.qt)
    {
        int u = config.q[config.qh++];
        for (int e = config.head[u]; e; e = config.nxt[e])
        {
            int v = config.to[e];
            if ((!find_type && config.locked[v] > 0 && config.locked[v] != pileid) || (find_type && config.locked[v] != pileid))
                continue;
            if (config.dist[v] == -1)
            {
                config.dist[v] = config.dist[u] + 1;
                config.q[config.qt++] = v;
            }
        }
    }
}
static int getpilenode(int pileid)
{
    if (pileid >= config.pileCount || pileid <= 0)
        return -1;
    int nodesPerPile = config.nodeCount / config.pileCount;
    pileid -= 1;
    int nodeId = pileid * nodesPerPile + 1;
    return nodeId;
}
static int find_min_dist_index(int pileid, int startid)
{
    int min_index = -1;
    printf("distance array[]: ");
    for (int i = 1; i <= config.nodeCount; i++)
    {
        printf("%d ", config.dist[i]);
    }
    printf("\r\nlocked array[]: ");
    for (int i = 1; i <= config.nodeCount; i++)
    {
        printf("%d ", config.locked[i]);
    }
    printf("\r\n");
    fflush(stdout);
    for (int n = 1; n <= config.nodeCount; n++)
    {
        int i = startid + config.nodeCount + (n / 2) * (n % 2 > 0 ? 1 : -1);
        i = ((i - 1) % config.nodeCount) + 1;
        //  检查是否为非负数且当前是最小值
        if (config.dist[i] >= 0 &&
            (min_index == -1 || config.dist[i] < config.dist[min_index]))
        {
            min_index = i;
        }
    }
    if (min_index != -1)
    {
        config.locked[min_index] = pileid; // 标记为已分配（锁定）
        config.dist[min_index] = -1;
    }
    return min_index; // 返回最小值的索引，如果没找到则返回-1
}
static int find_max_dist_index(int pileid, int startid)
{
    int max_index = -1;
    printf("distance array[]: ");
    for (int i = 1; i <= config.nodeCount; i++)
    {
        printf("%d ", config.dist[i]);
    }
    printf("\r\nlocked array[]: ");
    for (int i = 1; i <= config.nodeCount; i++)
    {
        printf("%d ", config.locked[i]);
    }
    printf("\r\n");
    fflush(stdout);
    for (int n = 1; n <= config.nodeCount; n++)
    {
        int i = (startid + config.nodeCount / 2 - 1) % config.nodeCount + 1;
        i = i + config.nodeCount + (n / 2) * (n % 2 > 0 ? 1 : -1);
        i = ((i - 1) % config.nodeCount) + 1;
        // 检查是否为非负数且pileid为参数的节点范围内最大值
        if (config.dist[i] >= 0 && config.locked[i] == pileid &&
            (max_index == -1 || config.dist[i] > config.dist[max_index]))
        {
            max_index = i;
        }
    }
    if (max_index != -1)
    {
        config.locked[max_index] = 0; // 标记为未分配（解锁）
        config.dist[max_index] = -1;
    }
    return max_index; // 返回最大值的索引，如果没找到则返回-1
}

static void clear_lockedarray(int pileid)
{
    for (int i = 0; i < config.nodeCount; i++)
    {
        if (config.locked[i] == pileid)
            config.locked[i] = 0;
    }
}
int allocPower_recaller(int pileid, int startnodeID, bool find_type, bool init) // find_type: 0=min, 1=max
{
    if (init)
    {
        if (config.locked[startnodeID] != 0 && config.locked[startnodeID] != pileid) // node is locked by another pile
        {
            for (int i = 1; i <= config.nodeCount; i++)
            {
                config.dist[i] = -1;
            }
            return -1;
        }
        bfs(startnodeID, pileid, find_type);
        return -1;
    }
    else
    {
        int nodeindex;
        if (find_type)
        {
            nodeindex = find_max_dist_index(abs(pileid), startnodeID);
        }
        else
        {
            nodeindex = find_min_dist_index(abs(pileid), startnodeID);
        }
        return nodeindex;
    }
}

void pau_init(int nodes, int piles)
{
    if (nodes & 1)
    {
        printf("节点数量必须为偶数\n");
        return;
    }
    if (piles > nodes / 2)
    {
        printf("充电桩数量不能超过节点半数\n");
        return;
    }
    config.nodeCount = nodes;
    config.pileCount = piles;
    memset(config.locked, 0, sizeof(config.locked));
    build_graph();
}
