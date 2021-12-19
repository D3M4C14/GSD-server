#ifndef PARSE_CFG
#define PARSE_CFG

// 默认值
#define DEFAULT_RECV_BUFFER_SIZE 1024
#define DEFAULT_SEND_BUFFER_SIZE 1024

typedef struct 
{
	int prot;
	bool daemon;
	char log_file[512];
	int work_thread_num;
	int recv_buffer_size;
	int send_buffer_size;
} launch_cfg_t;


int get_launch_cfg( const char* file_name, launch_cfg_t* cfg );

int print_launch_cfg_info( launch_cfg_t* cfg, char* context );



#endif
