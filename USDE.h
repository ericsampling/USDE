#ifndef __VINE_H__
#define __VINE_H__

#include <pthread.h>

#define MAX_ID_NUM 20000

#define THREAD_MAX_NUM 1

typedef struct neighbor_node{
  int neighbor_id;
  double neighbor_prob;
  int is_crawled;

//  int is_count;
  int is_seen;
}NEIGHBOR_NODE;

typedef struct virtual_node{
  int neighbor_id;
  double neighbor_prob;
  int is_crawled;

//  int is_count;
  int is_seen;
  struct pre_node* pre_location;
  struct virtual_node* next_node;
}VIRTUAL_NODE;

typedef struct pre_node{
  int neighbor_id;
  double remain_prob;
  int is_crawled;
//  int is_count;
  int is_seen;
  struct pre_node* start_node;
  struct pre_node* next_node;
}PRE_NODE;

typedef struct local_node{
  int neighbor_num;
  int step_num[THREAD_MAX_NUM];
  double self_loop;
  int remain_neighbor;
  double remain_num;
  int is_used;
  struct pre_node* pre_location;
  NEIGHBOR_NODE* neighbor_info;
  VIRTUAL_NODE* pre_info;//dummy edges
}LOCAL_NODE;

void choose_seed();

void get_info();

void start_crawl(int local_id,const int tindex);

int crawl_node(int local_id,const int tindex);

void output_result();

double compute_zscore(const int tindex);

void output_loop();

void output_edge_coverage();

void output_node_coverage();

void compute_repute();

void compute_size_variance();

void compute_variance();

void insert_into_pre_list(int pre_id,double remain_prob);

void add_virtual_neighbor(int local_id,PRE_NODE* cur_node,double neighbor_prob);

void delete_pre_node(PRE_NODE* del_node);

#endif
