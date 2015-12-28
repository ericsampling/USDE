#include <stdio.h>
#include <string.h>
#include <pthread.h>
#include "atomic_64.h"
#include <limits.h>
#include <stdarg.h>
#include "USDE.h"
#include <stdlib.h>
#include <time.h>
#include <math.h>
#define SETBIT(a, n) (a[n/CHAR_BIT] |= (1<<(n%CHAR_BIT)))
#define GETBIT(a, n) (a[n/CHAR_BIT] & (1<<(n%CHAR_BIT)))

int max_num=6000;
char* node_property_filename = "node_property_powerlaw.txt"; //node degree file: nodeID#1#degree#
char* result_file_path="result_powerlaw.txt";

char* input_file_path="social_graph_powerlaw.txt";  //link information file: nodeID 1 neighborID_1 neighborID_2 neighborID_3 ...  neighborID_N

char* sampled_id_filename_prefix = "sampled_id_USDE_powerlaw";

int initial_seed[100]={0};
int temp_branch_num[THREAD_MAX_NUM]={0};
LOCAL_NODE* local_record = NULL;
PRE_NODE* pre_record_list = NULL;
PRE_NODE* last_node = NULL;
time_t start_time;

double correct_value = 0.0001;
double correct_value2 = 0.9999;
unsigned char *bloom;
unsigned char *seen_bloom;
unsigned int global_num = 0;
unsigned int global_num_branch[THREAD_MAX_NUM] = {0};
unsigned int global_num_repute[THREAD_MAX_NUM] = {0};
pthread_mutex_t global_lock;
pthread_mutex_t record_lock;
pthread_mutex_t write_lock;
pthread_mutex_t crawler_lock;
static pthread_t captids[100];

double graph_size[100][THREAD_MAX_NUM]={0};
int pre_record_num = 0;
int * MC_link[THREAD_MAX_NUM];

double degree_ccdf[THREAD_MAX_NUM][300]={0};

int degree_ccdf_stat[THREAD_MAX_NUM][1000]={0};

int degree_list[MAX_ID_NUM+1] = {0};

void get_info(){
  FILE* fd = NULL;
  FILE* node_property_fd = NULL;
  char lbuf[1000000]={0};
  char tbuf[1000] = {0};
  char data[200000]={0};
  char* start = NULL;
  char* end = NULL;
  int offset = 0;
  char local_id_str[100]={0};
  char neighbor_id_str[100]={0};
  char neighbor_prob_str[100]={0};
  char check_num_str[10]={0};
  int local_id = 0;
  int local_num = 0;
  int check_num = 0;
  int neighbor_id = 0;
  double neighbor_prob = 0;
  int comp = 0;
  int z = 0;
  int i = 0;


  node_property_fd = fopen(node_property_filename,"r");
  if(node_property_fd == NULL){
    printf("There is no such file!\n");
    exit(1);
  }
  while (fgets(tbuf,1000,node_property_fd) != NULL){
  //  printf("%s\n", tbuf);
    if((z = strlen(tbuf)) > 0 && tbuf[z-1]=='\n'){

      tbuf[z-1]=0x20;
      memcpy(data+offset,tbuf,strlen(tbuf));
      comp = 1;
    }
    else if((z = strlen(tbuf)) > 0 && tbuf[z-1]!='\n'){
      memcpy(data+offset,tbuf,strlen(tbuf));
      offset += strlen(tbuf);
      comp = 0;
    }
    if(comp == 1){
      start = data;
      end = strstr(data,"#");
      if(end == NULL){
        printf("The local_id is not found!\n");
        exit(1);
      }
      memcpy(local_id_str,start,end-start);
      local_id = atoi(local_id_str)+1;

      if(local_id<0)
        printf("%d\n",local_id);
      start = end+1;
      end = strstr(start,"#");
      start = end+1;
      end = strstr(start,"#");
      if(end == NULL){
        printf("%s\tThe local_num is not found!\n",tbuf);
        exit(1);
      }
      memcpy(check_num_str,start,end-start);
      check_num = atoi(check_num_str);
      degree_list[local_id] = check_num;
      memset(data,0,200000);
      offset = 0;
      comp = 0;
      memset(local_id_str,0,100);
      memset(check_num_str,0,10);

    }//if(comp==1)

    memset(tbuf,0,1000);
  }//while

  memset(data,0,200000);

  fd = fopen(input_file_path,"r");
  if(fd == NULL){
    printf("There is no such file %s!\n",input_file_path);
    exit(1);
  }
  while (fgets(lbuf,1000000,fd) != NULL){
    if((z = strlen(lbuf)) > 0 && lbuf[z-1]=='\n'){

      lbuf[z-1]=0x20;
      memcpy(data+offset,lbuf,strlen(lbuf));
      comp = 1;
    }
    else if((z = strlen(lbuf)) > 0 && lbuf[z-1]!='\n'){
      memcpy(data+offset,lbuf,strlen(lbuf));
      offset += strlen(lbuf);
      comp = 0;
    }
    if(comp == 1){
      start = data;
      end = strstr(data," ");
      if(end == NULL){
        printf("The local_id is not found!\n");
        exit(1);
      }
      memcpy(local_id_str,start,end-start);
      local_id = atoi(local_id_str)+1;
      if(local_id<0)
        printf("%d\n",local_id);
      //local_id
      if(local_id%10000 == 0){
        printf("local_id is:%d\n",local_id);
      }
      check_num = degree_list[local_id];

      local_record[local_id-1].self_loop = 1.0;
      local_record[local_id-1].neighbor_num=check_num;
      for(i = 0;i < THREAD_MAX_NUM;i++)
        local_record[local_id-1].step_num[i] = 0;

      local_record[local_id-1].neighbor_info=(NEIGHBOR_NODE*)malloc(local_record[local_id-1].neighbor_num*sizeof(NEIGHBOR_NODE));
      memset(local_record[local_id-1].neighbor_info,0,local_record[local_id-1].neighbor_num*sizeof(NEIGHBOR_NODE));
      start = end+1;
      end = strstr(start," ");

      start = end+1;
      local_num = 0;
      while((end=strstr(start," "))!=NULL){
        if(start == end){
          start = end+1;
          continue;
        }
        memcpy(neighbor_id_str,start,end-start);
        if(neighbor_id_str[0]<'0'||neighbor_id_str[0]>'9'){
          start = end+1;
          continue;
        }
        neighbor_id = atoi(neighbor_id_str)+1;
   //     start = end+1;
   //     end = strstr(start," ");
   //     if(end == NULL){
   //       printf("The neighbor_prob is not found!\n");
   //       exit(1);
   //     }
   //     memcpy(neighbor_prob_str,start,end-start);
   //     neighbor_prob = atof(neighbor_prob_str);
//        neighbor_prob = (double)1/(double)local_record[local_id-1].neighbor_num;
        local_record[local_id-1].neighbor_info[local_num].neighbor_id=neighbor_id;
   //     local_record[local_id-1].neighbor_info[local_num].neighbor_prob=neighbor_prob;
   //     local_record[local_id-1].self_loop -= neighbor_prob;
        local_num ++;
        memset(neighbor_id_str,0,100);
        memset(neighbor_prob_str,0,100);
        start = end+1;
      }
      if(check_num != local_num){
        printf("check_num != local_num\n");
        exit(1);
      }
      memset(data,0,200000);
      offset = 0;
      comp = 0;
      memset(local_id_str,0,100);
      memset(check_num_str,0,10);

    }//if(comp==1)

    memset(lbuf,0,1000000);
  }//while


  fclose(node_property_fd);
  fclose(fd);
}

void choose_seed(){
  int choice = 0;
  int i = 0;
  int choose_num = 0;
  int repute = 0;
//  initial_seed[0]=281072;
  srand((unsigned int)time(0));
  while(choose_num<100){
  //  choice = rand()%MAX_ID_NUM+1;
    choice = (int)((double)rand()/RAND_MAX*MAX_ID_NUM)+1;
    for(i = 0; i<=choose_num-1;i++){
      if(initial_seed[i]==choice){
        repute = 1;
        break;
      }
    }
    if(repute==1){
      repute = 0;
      continue;
    }
    else{
      initial_seed[choose_num]=choice;
      choose_num++;
    }
  }
  initial_seed[0]=1434;
}

void output_loop(){
  char* record_filename="loop_static.txt";
  char file_content[100]={0};
  int i = 0;
  double loop_sum = 0;
  int  j = 0;
  int h = 0;
  FILE* record_fd = fopen(record_filename,"ab+");
  if(record_fd==NULL){
    printf("The loop_static file open error!\n");
    exit(1);
  }
  for(i = 0;i<MAX_ID_NUM;i++){
   if(local_record[i].step_num[0]>0){
      h++;
    loop_sum = 1;
    for(j = 0;j<local_record[i].neighbor_num;j++){
      loop_sum = loop_sum-local_record[i].neighbor_info[j].neighbor_prob;
    }
    memset(file_content,0,100);
    sprintf(file_content,"%d\t%d\t%d\t%lf\n",h,local_record[i].neighbor_num,local_record[i].step_num[0],loop_sum);
    fwrite(file_content,strlen(file_content),1,record_fd);
    fflush(record_fd);
   }
  }
  fclose(record_fd);

}

void insert_into_pre_list(int pre_id,double remain_prob){
  PRE_NODE* cur_node = NULL;
  PRE_NODE* start_node = NULL;
  PRE_NODE* end_node = NULL;
  PRE_NODE* new_node = NULL;


  int is_sampled = 0;

  int k=0;
  for(k=0;k<=THREAD_MAX_NUM-1;k++){
    if(local_record[pre_id-1].step_num[k]>0){
      is_sampled = 1;
      break;
    }

  }

  if(is_sampled==1){
    return;
  }

  if(local_record[pre_id-1].is_used ==1){
    local_record[pre_id-1].pre_location->remain_prob+=remain_prob;
    local_record[pre_id-1].pre_location->is_crawled++;
 /*   new_node = local_record[pre_id-1].pre_location;

    if(new_node->start_node != NULL)
      new_node->start_node->next_node = new_node->next_node;
    else
      pre_record_list = new_node->next_node;
    if(new_node->next_node != NULL){
      new_node->next_node->start_node = new_node->start_node;
    }
    else
      last_node = new_node->start_node;
    start_node = new_node->start_node;
    new_node->start_node = NULL;
    new_node->next_node = NULL;
    while(start_node != NULL && start_node->remain_prob<new_node->remain_prob){
      start_node = start_node->start_node;
    }
    if(start_node == NULL){
      new_node->next_node = pre_record_list;
      if(pre_record_list!=NULL)
        pre_record_list->start_node = new_node;
      new_node->start_node = NULL;
      pre_record_list = new_node;

    }
    else{
      if(start_node->next_node!=NULL){
        start_node->next_node->start_node = new_node;
        new_node->next_node = start_node->next_node;
        new_node->start_node = start_node;
        start_node->next_node = new_node;
      }
      else{
        start_node->next_node = new_node;
        new_node->start_node = start_node;
        last_node = new_node;
      }
    }
    */
    return;
  }


  start_node = pre_record_list;
  cur_node = pre_record_list;
  pre_record_num++;

  new_node = (PRE_NODE*)malloc(sizeof(PRE_NODE)+1);
  memset(new_node,0,sizeof(PRE_NODE)+1);
  new_node->neighbor_id = pre_id;
  new_node->remain_prob = remain_prob;

  if(pre_record_list == NULL){
    pre_record_list = new_node;
    new_node->start_node = NULL;
    new_node->next_node = NULL;
    last_node = new_node;
  }
  else{
  /*  while(start_node != NULL && start_node->remain_prob>remain_prob){
      start_node = start_node->next_node;
    }
    if(start_node == NULL){
       last_node->next_node = new_node;
       new_node->start_node = last_node;
       last_node = new_node;
    }
    else{
      if(start_node->start_node == NULL){
        pre_record_list = new_node;
        new_node->start_node = NULL;
        new_node->next_node = start_node;
        start_node->start_node = new_node;
      }
      else{
        start_node->start_node->next_node = new_node;
        new_node->start_node = start_node->start_node;
        new_node->next_node = start_node;
        start_node->start_node = new_node;
      }
    }
   */
    last_node->next_node = new_node;
    new_node->start_node = last_node;
    new_node->next_node = NULL;
    last_node = new_node;
  }
  local_record[pre_id-1].pre_location = new_node;
  local_record[pre_id-1].is_used = 1;
  local_record[pre_id-1].pre_location->is_crawled++;

}

void add_virtual_neighbor(int local_id,PRE_NODE* cur_node,double neighbor_prob){
  int neighbor_id = cur_node->neighbor_id;
  VIRTUAL_NODE* local_cur_node = NULL;
  VIRTUAL_NODE* neighbor_cur_node = NULL;
  VIRTUAL_NODE* local_new_node = NULL;
  VIRTUAL_NODE* neighbor_new_node = NULL;
  VIRTUAL_NODE* local_start_node = NULL;
  VIRTUAL_NODE* neighbor_start_node = NULL;

//  if(local_record[neighbor_id-1].self_loop<0.000001){
//    return;
//  }
  local_new_node = (VIRTUAL_NODE*)malloc(sizeof(VIRTUAL_NODE)+1);
  memset(local_new_node,0,sizeof(VIRTUAL_NODE)+1);
  local_new_node->neighbor_id = neighbor_id;
  local_new_node->neighbor_prob = neighbor_prob;
  local_new_node->pre_location = cur_node;
  local_record[local_id-1].self_loop -= neighbor_prob;
  if(local_record[local_id-1].self_loop<-0.000000000001){
    printf("OK1111!\n");
  }
  local_cur_node = local_record[local_id-1].pre_info;
  local_start_node = local_record[local_id-1].pre_info;
  while(local_cur_node!=NULL){
    if(local_cur_node->neighbor_id == neighbor_id){
      local_cur_node->neighbor_prob += neighbor_prob;

      local_new_node->pre_location = NULL;
      free(local_new_node);
      local_new_node = NULL;
      goto yes;
    }
    local_start_node = local_cur_node;
    local_cur_node = local_cur_node->next_node;
  }
  if(local_record[local_id-1].pre_info==NULL){
    local_record[local_id-1].pre_info = local_new_node;
  }
  else{
    local_start_node->next_node = local_new_node;
  }
yes:
  neighbor_new_node = (VIRTUAL_NODE*)malloc(sizeof(VIRTUAL_NODE)+1);
  memset(neighbor_new_node,0,sizeof(VIRTUAL_NODE)+1);
  neighbor_new_node->neighbor_id = local_id;
  neighbor_new_node->neighbor_prob = neighbor_prob;
  neighbor_new_node->pre_location = cur_node;
  local_record[neighbor_id-1].self_loop -= neighbor_prob;

  neighbor_cur_node = local_record[neighbor_id-1].pre_info;
  neighbor_start_node = local_record[neighbor_id-1].pre_info;
  while(neighbor_cur_node!=NULL){
    if(neighbor_cur_node->neighbor_id == local_id){
      neighbor_cur_node->neighbor_prob += neighbor_prob;
      neighbor_new_node->pre_location = NULL;
      free(neighbor_new_node);
      neighbor_new_node = NULL;
      return;
    }
    neighbor_start_node = neighbor_cur_node;
    neighbor_cur_node = neighbor_cur_node->next_node;
  }
  if(local_record[neighbor_id-1].pre_info==NULL){
    local_record[neighbor_id-1].pre_info = neighbor_new_node;
  }
  else{
    neighbor_start_node->next_node = neighbor_new_node;
  }

}

void delete_pre_node(PRE_NODE* del_node){
  PRE_NODE* cur_node;

  if(local_record[del_node->neighbor_id-1].is_used == 0){
    return;
  }
  pre_record_num--;

  if(del_node->start_node == NULL){
    pre_record_list = del_node->next_node;
    if(del_node->next_node!=NULL)
      del_node->next_node->start_node = NULL;
  }
  else{
    del_node->start_node->next_node = del_node->next_node;
    if(del_node->next_node!=NULL)
      del_node->next_node->start_node = del_node->start_node;
  }
  if(del_node == last_node){
    last_node = del_node->start_node;
  }
  del_node->next_node = NULL;
  del_node->start_node = NULL;
  local_record[del_node->neighbor_id-1].is_used = 0;

  local_record[del_node->neighbor_id-1].pre_location = NULL;
  free(del_node);
  del_node = NULL;
}

int crawl_node(int local_id,const int tindex){

  int candidate[MAX_ID_NUM]={0};
  int i = 0;
  int j = 0;
  int h = 0;
  int k = 0;
  int l = 0;
  int choice = 0;
  int next_id = 0;
  double sum_val = 0;
  double avg_val = 0;
  double variance = 0;
  int neighbor_num_sum = 0;
  double neighbor_num_sum_repute = 0;
  double neighbor_avg_num = 0;
  double neighbor_avg_num_repute = 0;
  int metric = 0;
  int iteration = 0;
  char file_content[1000] = {0};
  int choose_node[MAX_ID_NUM]={0};
  double z_score = 0;
  int uni_neighbor_sum = 0;
  double uni_avg_neighbor_num = 0;
  time_t timer;
  int choose_num = 0;
  int repute = 0;
  double estimate = 0.0;
  double estimate2 = 0.0;
//  double temp_value = 0.0;
  double temp_value2 = 0.0;
  int collision_count  = 0;
  int temp_count = 0;
  int max_remain_num = 0;
  double self_prob = 0.0;
  int rish_num = 0;
  int haha_num = 100;
  PRE_NODE* cur_node = NULL;
  PRE_NODE* temp_node = NULL;
  PRE_NODE* new_node = NULL;
  PRE_NODE* start_node = NULL;
  VIRTUAL_NODE* cur_virtual_node = NULL;
  double temp_self_loop = 0.0;
  int temp_value = 0;


  candidate[0] = 0;
  global_num_repute[tindex]++;
  MC_link[tindex][global_num_repute[tindex]-1]=local_id;


  int is_sampled = 0;
  int m = 0;
  for(m=0;m<=THREAD_MAX_NUM-1;m++){
    if(local_record[local_id-1].step_num[m]>0){
      is_sampled = 1;
      break;
    }
  }
  local_record[local_id-1].step_num[tindex]++;
  if(is_sampled==0){
    for(i = 0;i<=local_record[local_id-1].neighbor_num-1;i++){
      if(local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_num>local_record[local_id-1].neighbor_num){
        local_record[local_id-1].neighbor_info[i].neighbor_prob = (double)((double)1/(double)local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_num);
      }
      else
        local_record[local_id-1].neighbor_info[i].neighbor_prob = (double)((double)1/(double)local_record[local_id-1].neighbor_num);
      local_record[local_id-1].self_loop -= local_record[local_id-1].neighbor_info[i].neighbor_prob;
    }

  }

  /////////////////////////////////////////////////////////////////////
if(global_num_repute[tindex]<100){

    for(i = 0;i<=local_record[local_id-1].neighbor_num-1;i++){
      if(is_sampled==0 && local_record[local_id-1].neighbor_num>local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_num){
        insert_into_pre_list(local_record[local_id-1].neighbor_info[i].neighbor_id,(double)1/(double)local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_num-(double)1/(double)local_record[local_id-1].neighbor_num);
      }
    }
    if(local_record[local_id-1].pre_location != NULL){
      local_record[local_id-1].pre_location->remain_prob = local_record[local_id-1].self_loop;

    }

}
else{

  for(i = 0;i<=local_record[local_id-1].neighbor_num-1;i++){
    if(is_sampled==0 && local_record[local_id-1].neighbor_num>local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_num){
      insert_into_pre_list(local_record[local_id-1].neighbor_info[i].neighbor_id,(double)1/(double)local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_num-(double)1/(double)local_record[local_id-1].neighbor_num);
    }
  }
  if(pre_record_num>1000)
    max_remain_num = 1000;
  else
    max_remain_num = pre_record_num;
  haha_num = 1000;
//  if(local_record[local_id-1].self_loop<0){
//    printf("%lf\t",local_record[local_id-1].self_loop);
//  }
  if(local_record[local_id-1].self_loop > 0.00001 && is_sampled==0){
 //   printf("The origin is: %d\t%lf\n",local_id,local_record[local_id-1].self_loop);
    //add lock
    cur_node = pre_record_list;
    haha_num = 1000;
    if(haha_num>pre_record_num)
      haha_num = pre_record_num;
    temp_self_loop =  local_record[local_id-1].self_loop;
    while(cur_node != NULL){
      if(cur_node->neighbor_id == local_id || cur_node->remain_prob<0.00001  ){
        cur_node = cur_node->next_node;
        continue;
      }
      haha_num--;
      if(cur_node->remain_prob<(double)temp_self_loop/(double)max_remain_num){
        add_virtual_neighbor(local_id,cur_node,cur_node->remain_prob); //add dummy edges
     //   local_record[local_id-1].self_loop -= cur_node->remain_prob;
        cur_node->remain_prob -= cur_node->remain_prob;
      /*
        new_node = cur_node;

        if(new_node->start_node != NULL)
          new_node->start_node->next_node = new_node->next_node;
        else
          pre_record_list = new_node->next_node;
        if(new_node->next_node != NULL){
          new_node->next_node->start_node = new_node->start_node;
        }
        else
          last_node = new_node->start_node;
        start_node = new_node->next_node;
        new_node->start_node = NULL;
        new_node->next_node = NULL;

        while(start_node != NULL && start_node->remain_prob>new_node->remain_prob){
          start_node = start_node->next_node;
        }
        if(start_node == NULL){
           last_node->next_node = new_node;
           new_node->start_node = last_node;
           last_node = new_node;
        }
        else{
          if(start_node->start_node == NULL){
            pre_record_list = new_node;
            new_node->start_node = NULL;
            new_node->next_node = start_node;
            start_node->start_node = new_node;
          }
          else{
            start_node->start_node->next_node = new_node;
            new_node->start_node = start_node->start_node;
            new_node->next_node = start_node;
            start_node->start_node = new_node;
          }
        }
       */
      }
      else{
        add_virtual_neighbor(local_id,cur_node,(double)temp_self_loop/(double)max_remain_num);//add dummy edges
        cur_node->remain_prob -= (double)temp_self_loop/(double)max_remain_num;

     //   local_record[local_id-1].self_loop -= (double)local_record[local_id-1].self_loop/(double)100;
  /*      new_node = cur_node;

        if(new_node->start_node != NULL)
          new_node->start_node->next_node = new_node->next_node;
        else
          pre_record_list = new_node->next_node;
        if(new_node->next_node != NULL){
          new_node->next_node->start_node = new_node->start_node;
        }
        else
          last_node = new_node->start_node;
        start_node = new_node->next_node;
        new_node->start_node = NULL;
        new_node->next_node = NULL;

        while(start_node != NULL && start_node->remain_prob>new_node->remain_prob){
          start_node = start_node->next_node;
        }
        if(start_node == NULL){
           last_node->next_node = new_node;
           new_node->start_node = last_node;
           last_node = new_node;
        }
        else{
          if(start_node->start_node == NULL){
            pre_record_list = new_node;
            new_node->start_node = NULL;
            new_node->next_node = start_node;
            start_node->start_node = new_node;
          }
          else{
            start_node->start_node->next_node = new_node;
            new_node->start_node = start_node->start_node;
            new_node->next_node = start_node;
            start_node->start_node = new_node;
          }
        }
      */


      }

      if(cur_node->remain_prob<=0.00001){//if LP<0, remove the end point from Q
         temp_node = cur_node;

         cur_node = cur_node->next_node;
         delete_pre_node(temp_node);
      }
      else
        cur_node = cur_node->next_node;
      if(local_record[local_id-1].self_loop <= 0.00001 || haha_num<=0){//if self-sampling probability is reduced to 0, stop the addition of dummy edges
        if(local_record[local_id-1].self_loop <= 0.00001 && local_record[local_id-1].pre_location!=NULL){
         delete_pre_node(local_record[local_id-1].pre_location);
         local_record[local_id-1].pre_location = NULL;
       //     local_record[local_id-1].pre_location->remain_prob = local_record[local_id-1].self_loop;
       //     local_record[local_id-1].pre_location->is_crawled = 0;
        }
        else{
          if(local_record[local_id-1].pre_location!=NULL){
            delete_pre_node(local_record[local_id-1].pre_location);
            local_record[local_id-1].pre_location = NULL;
            //local_record[local_id-1].pre_location->remain_prob = local_record[local_id-1].self_loop;
            //local_record[local_id-1].pre_location->is_crawled = 0;
          }
        }
 //       printf("The last is: %d\t%lf\n",local_id,local_record[local_id-1].self_loop);


        break;
      }

    }
  }
//  printf("%lf\r\n",local_record[local_id-1].self_loop);
}
   if(local_record[local_id-1].pre_location!=NULL){
         delete_pre_node(local_record[local_id-1].pre_location);
         local_record[local_id-1].pre_location = NULL;
       //     local_record[local_id-1].pre_location->remain_prob = local_record[local_id-1].self_loop;
       //     local_record[local_id-1].pre_location->is_crawled = 0;
   }

  for(i = 1;i<=local_record[local_id-1].neighbor_num;i++){
    local_record[local_id-1].neighbor_info[i-1].is_seen = 1;
  }

  for(i = 1;i<=local_record[local_id-1].neighbor_num;i++){
    candidate[i] = candidate[i-1]+local_record[local_id-1].neighbor_info[i-1].neighbor_prob*100000;
  }
  cur_virtual_node = local_record[local_id-1].pre_info;
  rish_num = 0;
  while(cur_virtual_node != NULL){
    candidate[i] = candidate[i-1]+cur_virtual_node->neighbor_prob*100000;
    rish_num++;
    i++;
    cur_virtual_node = cur_virtual_node->next_node;
  }
//  printf("%d\n",candidate[local_record[local_id-1].neighbor_num+rish_num]);
  candidate[local_record[local_id-1].neighbor_num+rish_num+1] = 100000;

   //choice = rand()%100000 ;
   choice = (int)((double)rand()/RAND_MAX*99999);
 // printf("choice is %d\n",choice);
  for(i = 0;i<=local_record[local_id-1].neighbor_num+rish_num;i++){
    if(choice>=candidate[i]&&choice<candidate[i+1])
      break;
  }
  if(i == local_record[local_id-1].neighbor_num+rish_num){//self

    return local_id;
  }
  else{
    if(i<local_record[local_id-1].neighbor_num){
      next_id=local_record[local_id-1].neighbor_info[i].neighbor_id;
    }
    else{
      temp_value = i-local_record[local_id-1].neighbor_num;
      cur_virtual_node = local_record[local_id-1].pre_info;
      while(temp_value!=0){
        temp_value --;
        cur_virtual_node = cur_virtual_node->next_node;
      }
      if(cur_virtual_node->pre_location!=NULL){
 //       delete_pre_node(cur_virtual_node->pre_location);
        cur_virtual_node->pre_location = NULL;
      }
      next_id = cur_virtual_node->neighbor_id;
    }
   // if(local_record[next_id-1].neighbor_num>2000){
  //    printf("error!\n");
  //  }
  /*
    if(local_record[local_id-1].neighbor_info[i].is_crawled == 0){
      local_record[local_id-1].neighbor_info[i].is_crawled = 1;
        for(l = 0;l<local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_num-1;l++){
          if(local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_info[l].neighbor_id == local_id){
            local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_info[l].is_crawled = 1;
          }
        }
    }
    else{//³åÍ»
     //   local_record[local_id-1].neighbor_info[i].is_count ++;
  //      local_record[local_id-1].neighbor_info[i].is_crawled+=local_record[local_id-1].neighbor_info[i].is_count;
        local_record[local_id-1].neighbor_info[i].is_crawled=2;
        for(l = 0;l<local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_num-1;l++){
          if(local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_info[l].neighbor_id == local_id){
       //     local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_info[l].is_count ++;
        //    local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_info[l].is_crawled+=local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_info[l].is_count;
            local_record[local_record[local_id-1].neighbor_info[i].neighbor_id-1].neighbor_info[l].is_crawled=2;
          }
        }
    }
   */
     if(local_record[next_id-1].step_num[tindex] == 0){
      global_num_branch[tindex]++;

     if(global_num_branch[tindex]<100){
         metric = 10;
         iteration = 0;
      }
      else if(global_num_branch[tindex]>=100 && global_num_branch[tindex]<1000){
         metric = 100;
         iteration = 1;
      }
      else if(global_num_branch[tindex]>=1000 && global_num_branch[tindex]<10000){
         metric = 1000;
         iteration = 2;
      }
      else if(global_num_branch[tindex]>=10000 && global_num_branch[tindex]<100000){
         metric = 1000;
         iteration = 3;
      }
      else if(global_num_branch[tindex]>=100000 && global_num_branch[tindex]<1000000){
         metric = 1000;
         iteration = 4;
      }
      else if(global_num_branch[tindex]>=1000000 && global_num_branch[tindex]<10000000){
         metric = 1000;
         iteration = 5;
      }
      if(global_num_branch[tindex]%metric == 0){


         uni_avg_neighbor_num = 0;
        for(j = 0;j<MAX_ID_NUM;j++){
          if(local_record[j].step_num[tindex] > 0){
             sum_val += local_record[j].step_num[tindex];
             neighbor_num_sum += local_record[j].neighbor_num;
             neighbor_num_sum_repute +=  local_record[j].neighbor_num*local_record[j].step_num[tindex];
          }

        }
        avg_val = sum_val/(double)global_num_branch[tindex];
   //     printf("The sum_val is %d\tThe total_repute is %d\n",sum_val,global_num_repute[0]);
      //  temp_value = 0.0;
      //  temp_value2 = 0.0;
        for(j = 0;j<MAX_ID_NUM;j++){
          if(local_record[j].step_num[tindex] > 0){
             variance += (local_record[j].step_num[tindex]-avg_val)*(local_record[j].step_num[tindex]-avg_val);
        //     temp_value2 = (double)(local_record[j].neighbor_num)/(double)36854534-(double)(local_record[j].step_num[tindex])/(double)global_num_repute[tindex];
        //     if(temp_value2>0)
        //       temp_value += temp_value2;
        //     else
        //       temp_value -= temp_value2;

          }

        }
        collision_count = 0;
 /*       for(j = 0;j<694814;j++){
          for(l = 0;l<local_record[j].neighbor_num-1;l++){
            if(local_record[j].neighbor_info[l].is_crawled>1){
              temp_count = local_record[j].neighbor_info[l].is_crawled-1;
              collision_count += temp_count;
            }
          }
        }
   */
        variance = variance/(double)global_num_branch[tindex];
        neighbor_avg_num = (double)neighbor_num_sum/(double)global_num_branch[tindex];
        neighbor_avg_num_repute = (double)neighbor_num_sum_repute/(double)global_num_repute[tindex];
        z_score = compute_zscore(tindex);   //z-score

        char record_filename[20]={0};
        sprintf(record_filename,"USDE_record_powerlaw_%d.txt",tindex);
        FILE* record_fd = fopen(record_filename,"ab+");
        if(record_fd==NULL){
          printf("The %d file open error!\n",tindex);
          exit(1);
        }
        memset(file_content,0,1000);
        sprintf(file_content,"%d\t%lf\t%lf\t%lf\t%lf\t%lf\n",global_num_branch[tindex]/metric+iteration*10,avg_val,variance,neighbor_avg_num,neighbor_avg_num_repute,z_score);
        //step number, average of sampling times per node,variance of sampling times per node,E_u(k)£¬E_r(k)£¬z-score£¬


        fwrite(file_content,strlen(file_content),1,record_fd);
        fclose(record_fd);
        temp_branch_num[tindex]=global_num_branch[tindex];
      }


     }
    pthread_mutex_lock(&(global_lock));
    if(!(GETBIT(bloom,next_id))){
      SETBIT(bloom,next_id);
      global_num++;

    }
    pthread_mutex_unlock(&(global_lock));
    return next_id;
   }
}

void start_crawl(int local_id,const int tindex){
  int next_id = 0;
  int i = 0;
  int j = 0;

  char file_content[100]={0};

  FILE* sampled_id_fd = NULL;
  char sampled_id_filename[20] = {0};;
  sprintf(sampled_id_filename,"USDE_sampled_id_powerlaw_%d.txt",tindex);
  sampled_id_fd = fopen(sampled_id_filename,"w");
  if(sampled_id_fd==NULL){
    printf("The %d file open error!\n",tindex);
    exit(1);
  }


  while(1){


    pthread_mutex_lock(&(crawler_lock));
    next_id = crawl_node(local_id,tindex);


    local_id = next_id;

    memset(file_content,0,100);
    sprintf(file_content,"%d\t\n",next_id);
    fwrite(file_content,strlen(file_content),1,sampled_id_fd);
    fflush(sampled_id_fd);
    pthread_mutex_unlock(&(crawler_lock));
    if( global_num_branch[tindex] >= max_num){

        double temp_sum = 0;
        for(i = 0;i<300;i++){
          temp_sum = 0;
          for(j = 0;j<THREAD_MAX_NUM;j++){
             temp_sum += degree_ccdf[j][i];
          }

        }

       break;
    }

  }//while(1)
  fclose(sampled_id_fd);
}

static void *capture_thread(void *_arg){
    const int tindex = (int)(long)_arg;
    int local_id = 0;

    local_id=initial_seed[tindex];
    local_record[local_id-1].remain_num = 1000000.0;
    srand((unsigned int)time(NULL));
    pthread_mutex_lock(&(global_lock));
    if(!(GETBIT(bloom,local_id))){
      SETBIT(bloom,local_id);
      global_num++;


    }
    pthread_mutex_unlock(&(global_lock));
    start_crawl(local_id,tindex);
    return NULL;
}

int main(){
      int i = 0;
      int h = 0;
      int k = 0;
      int l = 0;
      char file_content[1000] = {0};
      pthread_mutexattr_t attr;

      pthread_mutexattr_init (&attr);
      pthread_mutexattr_setkind_np (&attr, PTHREAD_MUTEX_RECURSIVE_NP);
      pthread_mutex_init (&global_lock, &attr);
      pthread_mutexattr_destroy (&attr);

      pthread_mutexattr_init (&attr);
      pthread_mutexattr_setkind_np (&attr, PTHREAD_MUTEX_RECURSIVE_NP);
      pthread_mutex_init (&crawler_lock, &attr);
      pthread_mutexattr_destroy (&attr);


      pthread_mutexattr_init (&attr);
      pthread_mutexattr_setkind_np (&attr, PTHREAD_MUTEX_RECURSIVE_NP);
      pthread_mutex_init (&record_lock, &attr);
      pthread_mutexattr_destroy (&attr);

      pthread_mutexattr_init (&attr);
      pthread_mutexattr_setkind_np (&attr, PTHREAD_MUTEX_RECURSIVE_NP);
      pthread_mutex_init (&write_lock, &attr);
      pthread_mutexattr_destroy (&attr);

      local_record = (LOCAL_NODE*)malloc(MAX_ID_NUM*sizeof(LOCAL_NODE));
      memset(local_record,0,MAX_ID_NUM*sizeof(LOCAL_NODE));

      for(i = 0;i<THREAD_MAX_NUM;i++){
        MC_link[i] = (int*)malloc(2000000*sizeof(int));
        memset(MC_link[i],0,2000000*sizeof(int));
      }
      choose_seed();

      get_info();

      printf("get info OK!\n");
      start_time = time(NULL);
      bloom = calloc((MAX_ID_NUM+CHAR_BIT-1)/CHAR_BIT, sizeof(char));
      seen_bloom = calloc((MAX_ID_NUM+CHAR_BIT-1)/CHAR_BIT, sizeof(char));
      for (i = 0; i < THREAD_MAX_NUM; i++) {
        pthread_create(&captids[i], NULL, capture_thread, (void *)(long)i);
      }

      for (i = 0; i < THREAD_MAX_NUM; i++) {
        pthread_join(captids[i], NULL);
       }


       printf("sampling OK!\n");
       output_result();

  //   output_edge_coverage();
  //   output_node_coverage();
  //   output_loop();
  //   compute_repute();
  //   compute_size_variance();

       return 0;
}

void compute_size_variance(){
  FILE* fd = NULL;
  char file_content[500] = {0};
  double avg_size = 0;
  double variance_size = 0;
  int thread_num = THREAD_MAX_NUM;
  int sum_size = 0;
  int i = 0;
  int j = 0;

  fd = fopen("variance_old_16.txt","ab+");
  if(fd == NULL){
    printf("There is no such file variance_old_70.txt!\n");
    exit(1);
  }
  for(i = 0;i<100;i++){
    memset(file_content,0,500);
    for(j = 0;j<THREAD_MAX_NUM;j++){
      if(graph_size[i][j]>0)
       sum_size += graph_size[i][j];
    }
    avg_size = (double)sum_size/(double)thread_num;
    for(j = 0;j<THREAD_MAX_NUM;j++){
      if(graph_size[i][j]>0){
        variance_size += (double)(graph_size[i][j] - avg_size)/(double)avg_size*(double)(graph_size[i][j] - avg_size)/(double)avg_size;
      }
    }
    variance_size = variance_size/(double)thread_num;
    sprintf(file_content,"%d\t%lf\n",i,variance_size);
    fwrite(file_content,strlen(file_content),1,fd);
    avg_size = 0;
    variance_size = 0;
    sum_size = 0;
  }
  fclose(fd);

}

void compute_repute(){
    FILE* fd = NULL;
    char file_content[500]={0};
    int i = 0;
    int j = 0;
    int k = 0;
    int seen_node_sum = 0;
    int sum = 0;
    int h = 0;
    int g = 0;
    int jugde = 0;
    int tot_num = 0;

    for(i = 0;i<MAX_ID_NUM;i++){
     jugde = 0;
     for(g = 0;g<THREAD_MAX_NUM;g++){
       if(local_record[i].step_num[g] > 0){
         jugde++;

       }
     }
     if(jugde >= 1)
       tot_num++;
      if(jugde > 1){
       if(!GETBIT(seen_bloom,i)){
          seen_node_sum++;
          SETBIT(seen_bloom,i);
       }

     }
    }


    printf("The repute is %lf\n",(double)seen_node_sum/(double)tot_num);
    return;
}

void compute_variance(){
  int i = 0;
  int j = 0;

  int node_sum = 0;

  int thread_ident_sum[THREAD_MAX_NUM]={0};
  double avg_step[THREAD_MAX_NUM] = {0};
  double sum[THREAD_MAX_NUM] = {0};
  double variance[THREAD_MAX_NUM] = {0};
  double avg_variance = 0;
  for(j = 0;j<THREAD_MAX_NUM;j++){
    node_sum = 0;

    for(i = 0;i<MAX_ID_NUM;i++){
     if(local_record[i].step_num[j]>0){
       node_sum+=local_record[i].step_num[j];

       thread_ident_sum[j]++;
     }
    }
    avg_step[j] = (double)node_sum/(double)thread_ident_sum[j];
  }

  for(j = 0;j<THREAD_MAX_NUM;j++){
    for(i = 0;i<MAX_ID_NUM;i++){
      if(local_record[i].step_num[j]>0){
         sum[j]+=(double)(local_record[i].step_num[j]-avg_step[j])*(double)(local_record[i].step_num[j]-avg_step[j]);
      }
    }
    variance[j] = (double)sum[j]/(double)thread_ident_sum[j];
  }
  for(j = 0;j<THREAD_MAX_NUM;j++){
    avg_variance+=variance[j];
  }

  printf("%lf\t%lf\n",(double)(avg_variance)/(double)(29),(double)max_num/(double)694814);
}

void output_edge_coverage(){

    char file_content[500]={0};
    int i = 0;
    int j = 0;
    int k = 0;
    int l = 0;
    int seen_edge_sum = 0;
    int sum = 0;

    for(i = 0;i<MAX_ID_NUM;i++){

      for(j = 0;j<=local_record[i].neighbor_num-1;j++){

        if(local_record[i].neighbor_info[j].is_seen == 1){
          for(k = 0;k<local_record[local_record[i].neighbor_info[j].neighbor_id-1].neighbor_num-1;k++){
             if(local_record[local_record[i].neighbor_info[j].neighbor_id-1].neighbor_info[k].neighbor_id == i+1 && local_record[local_record[i].neighbor_info[j].neighbor_id-1].neighbor_info[k].is_seen == 1)
               local_record[local_record[i].neighbor_info[j].neighbor_id-1].neighbor_info[k].is_seen = 0;
          }
          seen_edge_sum++;
        }
      }

    }


    printf("The edge_coverage is %d\n",seen_edge_sum);

    return;
}

void output_node_coverage(){
    FILE* fd = NULL;
    char file_content[500]={0};
    int i = 0;
    int j = 0;
    int k = 0;
    int seen_node_sum = 0;
    int sum = 0;
    int h = 0;
    int g = 0;
    int jugde = 0;

    for(i = 0;i<MAX_ID_NUM;i++){
     jugde = 0;
  //   for(g = 0;g<THREAD_MAX_NUM;g++){
       if(local_record[i].step_num[0] > 0){
         jugde = 1;
  //       break;
       }
  //   }
      if(jugde == 1){
       if(!GETBIT(seen_bloom,i)){
          seen_node_sum++;
          SETBIT(seen_bloom,i);
       }
       for(k = 0;k<=local_record[i].neighbor_num-1;k++){
         h = local_record[i].neighbor_info[k].neighbor_id-1;
         if(!GETBIT(seen_bloom,h)){
            seen_node_sum++;
            SETBIT(seen_bloom,h);
         }
       }
     }
    }


    printf("The node_coverage is %d\n",seen_node_sum);
    return;
}

void output_result(){
    FILE* fd = NULL;
    char file_content[50000]={0};
    int i = 0;
    int j = 0;
    fd = fopen(result_file_path,"w");
    if(fd == NULL){
      printf("There is no such file!\n");
      exit(1);
    }
    for(i = 0;i<MAX_ID_NUM;i++){
      if(local_record[i].step_num[0] > 0){
        j++;
        memset(file_content,0,50000);
        sprintf(file_content,"%d\t%d\t%d\t%lf\n",i,local_record[i].step_num[0],local_record[i].neighbor_num,local_record[i].self_loop);
        fwrite(file_content,strlen(file_content),1,fd);
      }

    }
    fclose(fd);
    return;
}

double compute_zscore(const int tindex){
  int i = 0;
  int pre_link_start = 0;
  int pre_link_end = 0;
  int pro_link_start = 0;
  int pro_link_end = 0;

  int pre_link_sum = 0;
  double pre_link_avg = 0;
  double pre_link_var = 0;

  int pro_link_sum = 0;
  double pro_link_avg = 0;
  double pro_link_var = 0;

  double result = 0;


  pre_link_start = 0;
  pre_link_end = global_num_repute[tindex]/10;

  pro_link_start = global_num_repute[tindex]/2;
  pro_link_end = global_num_repute[tindex];

  for(i = pre_link_start;i<pre_link_end;i++){
    pre_link_sum += local_record[MC_link[tindex][i]-1].neighbor_num;
  }
  pre_link_avg = (double)pre_link_sum/(double)(pre_link_end-pre_link_start);
  for(i = pre_link_start;i<pre_link_end;i++){
    pre_link_var += (double)((double)local_record[MC_link[tindex][i]-1].neighbor_num-pre_link_avg)*(double)((double)local_record[MC_link[tindex][i]-1].neighbor_num-pre_link_avg);
  }

  for(i = pro_link_start;i<pro_link_end;i++){
    pro_link_sum += local_record[MC_link[tindex][i]-1].neighbor_num;
  }
  pro_link_avg = (double)pro_link_sum/(double)(pro_link_end-pro_link_start);
  for(i = pro_link_start;i<pro_link_end;i++){
    pro_link_var += (double)((double)local_record[MC_link[tindex][i]-1].neighbor_num-pro_link_avg)*(double)((double)local_record[MC_link[tindex][i]-1].neighbor_num-pro_link_avg);
  }

  result = (double)(pre_link_avg-pro_link_avg)/sqrt(pre_link_var/(double)(pre_link_end-pre_link_start)+pro_link_var/(double)(pro_link_end-pro_link_start));
  return result;
}






