#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "parse_cfg.h"

int get_launch_cfg( const char* file_name, launch_cfg_t* cfg )
{
	cfg->recv_buffer_size = DEFAULT_RECV_BUFFER_SIZE;
	cfg->send_buffer_size = DEFAULT_SEND_BUFFER_SIZE;

	if( access( file_name, F_OK ) == -1 )
	{
		perror( file_name );
		return -1;
	}

	return 0;
}

int print_launch_cfg_info( launch_cfg_t* cfg )
{
	printf("\n===========================launch_config========================\n");

	printf( "prot : %d\n", cfg->prot );
	printf( "recv_buffer_size : %d\n", cfg->recv_buffer_size );
	printf( "send_buffer_size : %d\n", cfg->send_buffer_size );

	printf("===========================launch_config========================\n\n");

	return 0;
}