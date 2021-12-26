#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include "parse_cfg.h"


void set_default_value( launch_cfg_t* cfg );
int parse_line( launch_cfg_t* cfg, const char* line );
int parse_key_value( launch_cfg_t* cfg, const char* key, const char* value );

int get_launch_cfg( const char* file_name, launch_cfg_t* cfg )
{
	if( access( file_name, F_OK | R_OK ) == -1 )
	{
		perror( file_name );
		printf("access %s\n", file_name );
		return -1;
	}

    int file_fd = open( file_name, O_RDONLY );
    if( file_fd < 0 )
	{
		perror( file_name );
		printf("open %s\n", file_name );
		return -2;
	}
    struct stat file_stat;
    fstat( file_fd, &file_stat );

    char * file_buf = new char [ file_stat.st_size + 1 ];
    memset( file_buf, '\0', file_stat.st_size + 1 );
    if ( read( file_fd, file_buf, file_stat.st_size ) < 0 )
    {
    	close( file_fd );
    	delete [] file_buf;
		perror( file_name );
		printf("read %s\n", file_name );
		return -3;
    }

	set_default_value( cfg );

	int ret;

	char c, line[LINE_MAX_LEN];
	bool lok=false;
	int line_len=0;
	memset( line, '\0', LINE_MAX_LEN );
	for (int i = 0; i < file_stat.st_size; ++i)
	{
		c = file_buf[i];

		if( c == '\n') lok = true;
		if( line_len >= LINE_MAX_LEN )
		{
			perror( file_name );
			printf( "line len too long : '%s...'\n", line );
			return -4;
		}

		if( lok )
		{
			if( line_len > 0 )
			{
				for (int li = line_len-1; li >= 0; --li)
				{
					if( line[li] == ' ' or line[li] == '\r' or line[li] == '\t' or line[li] == ',' or line[li] == ';' )
					{
						--line_len;
						line[li] = '\0';
					}
					else
					{
						break;
					}
				}

				if( line_len > 0 )
				{
					ret = parse_line( cfg, line );
					if ( ret != 0 )
					{
						printf( "parse line error(%d) : '%s'\n", ret, line );
						return -4;
					}
				}
				line_len = 0;
				memset( line, '\0', LINE_MAX_LEN );
			}
			lok = false;
		}
		else
		{
			if( c != ' ' or line_len > 0 )
			{
				line[ line_len++ ] = c;
			}
		}

	}
	
	close( file_fd );
	delete [] file_buf;
	return 0;
}

void set_default_value( launch_cfg_t* cfg )
{
	cfg->recv_buffer_size = DEFAULT_RECV_BUFFER_SIZE;
	cfg->send_buffer_size = DEFAULT_SEND_BUFFER_SIZE;
	cfg->backlog = DEFAULT_BACKLOG;
	cfg->msg_delay = DEFAULT_MSG_DELAY;

	memset( cfg->log_file, '\0', VALUE_MAX_LEN );

	unsigned cpu_num = sysconf( _SC_NPROCESSORS_ONLN );
	cfg->work_thread_num = cpu_num;
}

int parse_line( launch_cfg_t* cfg, const char* line )
{
	if ( line[0] == '#' || (line[0] == '/' && line[1] == '/') )return 0;

	char c, key[KEY_MAX_LEN], value[VALUE_MAX_LEN];

	// mid key-end value-start
	int m=0, ke, vs;
	for (int i = 0; i < LINE_MAX_LEN; ++i)
	{
		c = line[i];
		if ( c == '=' )
		{
			m = i;
			break;
		}
		if( c == '\0' )break;
	}
	if( m == 0 || line[m+1] == '\0' )return -1;

	// key
	ke=0;
	for (int i = m-1; i >= 0; --i)
	{
		if ( line[i] != ' ' )
		{
			ke = i+1;
			break;
		}
	}
	if( ke == 0 || ke > KEY_MAX_LEN-1 )
	{
		printf("key length is zero or too long\n");
		return -2;
	}

	memset( key, '\0', KEY_MAX_LEN);
	for (int i = 0; i < ke; ++i)
	{
		key[i] = line[i];
	}

	// value
	vs=LINE_MAX_LEN-1;
	for (int i = m+1; i < LINE_MAX_LEN-1; ++i)
	{
		if ( line[i] != ' ' || line[i] == '\0' )
		{
			vs = i;
			break;
		}
	}
	if( line[vs] == '\0' )
	{
		printf("value length is zero\n");
		return -3;
	}

	memset( value, '\0', VALUE_MAX_LEN);
	for (int li = vs, vi=0; li < vs+VALUE_MAX_LEN-1; ++li,++vi)
	{
		value[vi] = line[li];
		if ( value[vi] == '\0' || li == vs+VALUE_MAX_LEN-2 )
		{
			if ( line[li+1] != '\0' )
			{// 数据没读完
				printf("value length too long\n");
				return -4;
			}
			break;
		}
	}

	return parse_key_value( cfg, key, value );
}


// 解析bool键值变量
#define PBKV(KEY) \
else if( strcmp( #KEY, key ) == 0 )\
{\
	bool b;\
	if( strcmp( "true", value ) == 0 )\
	{\
		b = true;\
	}\
	else if( strcmp( "false", value ) == 0 )\
	{\
		b = false;\
	}\
	else\
	{\
		printf( "launch config %s's value : '%s' is error\n", #KEY, value );\
		return -1;\
	}\
	cfg->KEY = b;\
}

// 解析int键值变量
#define PIKV(KEY) \
else if( strcmp( #KEY, key ) == 0 )\
{\
	cfg->KEY = atoi( value );\
	if ( cfg->KEY < 1 )\
	{\
		printf( "launch config %s's value : '%s' is error\n", #KEY, value );\
		return -1;\
	}\
}

// 解析str键值变量
#define PSKV(KEY) \
else if( strcmp( #KEY, key ) == 0 )\
{\
	int len = strlen( value );\
	if ( (value[0] != '\'' && value[0] != '\"') || value[0] != value[len-1] )\
	{\
		printf( "launch config %s's value : '%s' is error\n", #KEY, value );\
		return -1;\
	}\
	memset( cfg->KEY, '\0', VALUE_MAX_LEN );\
	memcpy( cfg->KEY, value+1, len-2);\
}

int parse_key_value( launch_cfg_t* cfg, const char* key, const char* value )
{
	// printf( "'%s':'%s'\n",key, value );
	if( strcmp( "#", key ) == 0 )
	{}
	PIKV(prot)
	PBKV(daemon)
	PSKV(log_file)
	PIKV(work_thread_num)
	PIKV(work_buffer_size)
	PIKV(recv_buffer_size)
	PIKV(send_buffer_size)
	PIKV(backlog)
	PIKV(msg_delay)
	else
	{
		printf( "the key : '%s' is not define\n", key );
	}

	return 0;
}


int print_launch_cfg_info( launch_cfg_t * const cfg )
{
	printf("\n===========================launch_config========================\n");

	printf( "prot : %d\n", cfg->prot );
	printf( "daemon : %s\n", cfg->daemon ? "true" : "false" );
	printf( "log_file : %s\n", cfg->log_file );
	printf( "work_thread_num : %d\n", cfg->work_thread_num );
	printf( "work_buffer_size : %d\n", cfg->work_buffer_size );
	printf( "recv_buffer_size : %d\n", cfg->recv_buffer_size );
	printf( "send_buffer_size : %d\n", cfg->send_buffer_size );
	printf( "backlog : %d\n", cfg->backlog );
	printf( "msg_delay : %d\n", cfg->msg_delay );
	
	printf("===========================launch_config========================\n\n");

	return 0;
}
