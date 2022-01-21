/*
 * Wifi.c
 *
 *  Created on: 2021. 11. 14.
 *      Author: Gyuhyun_Cho
 */

#include "GoProfi_Wifi.h"
#include "esp_wifi.h"
#include "esp_log.h"
#include "esp_http_server.h"
#include "esp_system.h"
#include "nvs_flash.h"

#include "math.h"

#include <dirent.h>
#include <string.h>
#include <time.h>

#include "esp_vfs.h"

#include "MainParameter.h"

#include "GoProfi_Camera.h"

#define LOG "WIFI"

#define ESP_WIFI_CHANNEL 1
#define ESP_MAX_STA_CONN 2

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

/* Empty handle to esp_http_server */
static httpd_handle_t server = NULL;
static httpd_handle_t server_control = NULL;
static struct tm time_st;
static time_t time_t_val;

extern int State_Request = 2;

static void wifi_event_handler(void* arg, esp_event_base_t event_base,
                                    int32_t event_id, void* event_data)
{
    if (event_id == WIFI_EVENT_AP_STACONNECTED) {
        wifi_event_ap_staconnected_t* event = (wifi_event_ap_staconnected_t*) event_data;
        ESP_LOGI(LOG, "station "MACSTR" join, AID=%d",
                 MAC2STR(event->mac), event->aid);
    } else if (event_id == WIFI_EVENT_AP_STADISCONNECTED) {
        wifi_event_ap_stadisconnected_t* event = (wifi_event_ap_stadisconnected_t*) event_data;
        ESP_LOGI(LOG, "station "MACSTR" leave, AID=%d",
                 MAC2STR(event->mac), event->aid);
    }
}

/*
 * WIFI Main part
 */
int Wifi_init()
{
	esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND)
    {
      ESP_ERROR_CHECK(nvs_flash_erase());
      ret = nvs_flash_init();
    }

	esp_netif_init();
	esp_event_loop_create_default();
	esp_netif_create_default_wifi_ap();

	wifi_init_config_t wifi_config_default = WIFI_INIT_CONFIG_DEFAULT();

	esp_wifi_init(&wifi_config_default);

    ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT,
                                                        ESP_EVENT_ANY_ID,
                                                        &wifi_event_handler,
                                                        NULL,
                                                        NULL));
	wifi_config_t wf_config =
	{
		.ap =
		{
			.ssid = ESP_WIFI_SSID,
			.ssid_len = strlen(ESP_WIFI_SSID),
			.channel = ESP_WIFI_CHANNEL,
			.password = ESP_WIFI_PASS,
			.max_connection = ESP_MAX_STA_CONN,
			.authmode = WIFI_AUTH_WPA_WPA2_PSK
		},
	};

	if( esp_wifi_set_mode(WIFI_MODE_AP) == ESP_OK && esp_wifi_set_config(WIFI_IF_AP, &wf_config) == ESP_OK )
	{
		ESP_LOGI(LOG, "Entered");
		if ( esp_wifi_start() == ESP_OK ) return 0;
		else return 1;
	}
	return 1;
}

void Wifi_control_main()
{
	start_control_server();
	ESP_LOGI(LOG, "WIFI CONTROL MAIN");

	for ( ; ; )
	{

	}
}

void Wifi_main()
{
	//if( esp_wifi_start() == ESP_OK )
	//{
		// debug
		ESP_LOGI(LOG, "Free size SPIRAM : %d", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
		ESP_LOGI(LOG, "Free size INTERNAL : %d", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));

		start_webserver();
		ESP_LOGI(LOG, "WIFI STARTED");
	//}

	for ( ; ; )
	{

	}
}

int Wifi_stop()
{
	if(server != NULL)
	{
		if( httpd_stop(server) == ESP_OK )
		{
			if ( esp_wifi_stop() == ESP_OK )
			{
				return 1;
			}
		}
	}
	return 0;
}

int get_control(httpd_req_t *req)
{
	char buf[SCRATCH_BUFSIZE];
	size_t buf_size = sizeof(buf);

	char control[5];
	size_t control_size = sizeof(control);

	size_t qur_len;
	char *qur;

	FILE *fp;

	memset(buf,0,buf_size);

	ESP_LOGI(LOG, "BEFORE READ");
	int Main_State = State_Read();
	/*
	if ( Main_State == 2 )
	{
		sprintf(buf, "<script>function Func_initVal(){ \
		    		  document.getElementById(\"Status\").innerHTML = \"Not recording\";</script>");
	}
	else if ( Main_State == 3 )
	{
		sprintf(buf, "<script>function Func_initVal(){ \
		    		  document.getElementById(\"Status\").innerHTML = \"Recording\";</script>");
	}
	else
	{
		sprintf(buf, "<script>function Func_initVal(){ \
		    		  document.getElementById(\"Status\").innerHTML = \"ERROR\";</script>");
	}

	httpd_resp_send_chunk(req, buf, buf_size);
	memset(buf,0,buf_size);
*/
	memset(buf,0,buf_size);
	sprintf(buf,"<html>\
				<h1>CONTROL & CONFIG PAGE</h1>\
				STATUS : %d <a href=\"http://192.168.4.1/control\">UPDATE</a>\
				<br>CONTROL : \
				<br><a href=\"http://192.168.4.1/control?req=rec\">RECORD</a>\
				<br><a href=\"http://192.168.4.1/control?req=stop\">STOP_RECORD</a>\
				<br>\
				<br><a href=\"http://192.168.4.1/config\">CONFIG</a>\
				<br><a href=\"http://192.168.4.1/flist\">FILE MANAGER</a>\
				</html>", Main_State);
	httpd_resp_send_chunk(req, buf, buf_size);
	httpd_resp_send_chunk(req, NULL, buf_size);

	/*
	ESP_LOGI(LOG, "BEFORE OPEN FILE");

	fp = fopen("\sdcard\system\control.html","r");
	if ( fp != NULL )
	{
		do
		{
			memset(buf,0,buf_size);
			buf_size = fread(buf, 1, SCRATCH_BUFSIZE, fp);
			if (buf_size > 0)
			{
				if(httpd_resp_send_chunk(req, buf, buf_size) != ESP_OK)
				{
					fclose(fp);
				}
			}
		}while (buf_size != 0);
		fclose(fp);
	}
*/
	ESP_LOGI(LOG, "FILE_SEND DONE");

	qur_len = httpd_req_get_url_query_len(req) + 1;
	if (qur_len >1)
	{
		ESP_LOGI(LOG, "QUR EXIST");
		qur = malloc(qur_len);
		if (httpd_req_get_url_query_str(req, qur, qur_len) == ESP_OK)
		{
			ESP_LOGI(LOG, "QUR GET");
   			if ( httpd_query_key_value(qur, "req", control, sizeof(control)) == ESP_OK )
   			{
   				ESP_LOGI(LOG, "key value : %s",qur);
   				if ( strcmp(control,"rec") == 0 )
   				{
   					State_Write((int)(3));
   				}
   				else if ( strcmp(control, "stop") == 0 )
   				{
   					State_Write((int)(2));
   				}
   			}
		}
	}

	return 0;
}

int get_flist(httpd_req_t *req)
{
    DIR *d;
    struct dirent *dir;
    d = opendir("/sdcard/");
    char buf[SCRATCH_BUFSIZE];
    size_t buf_size = sizeof(buf);

    size_t qur_len;
    char *qur;

    FILE *f;

	char f_name[128];
	char f_mode[5];
	char f_path[512];

    int f_count = 0;

    unsigned long f_info_size = 0;
    unsigned int f_tot_size = 0;
    int f_size = 0;

    ESP_LOGI(LOG, "Check point1");
    memset(f_name,0,sizeof(f_name));

    qur_len = httpd_req_get_url_query_len(req) + 1;
    if (qur_len > 1)
    {
     	qur = malloc(qur_len);
  		if (httpd_req_get_url_query_str(req, qur, qur_len) == ESP_OK)
   		{
   			ESP_LOGI(LOG, "Query_str : %s", qur);

   			memset(f_path, 0, sizeof(f_path));
   			if ( httpd_query_key_value(qur, "file", f_name, sizeof(f_name)) == ESP_OK && httpd_query_key_value(qur, "mode", f_mode, sizeof(f_mode)) == ESP_OK )
   			{
				ESP_LOGI(LOG, "fname selected : %s",f_name);
				sprintf(f_path,"/sdcard/%s", f_name);
				ESP_LOGI(LOG, "path : %s", f_path);

   				if ( strcmp(f_mode, "down") == 0 )
   				{
					f = fopen(f_path, "r");
					if ( f == NULL ) ESP_LOGI(LOG, "F_Open failed");

					if ( f != NULL )
					{
						do
						{
							memset(buf,0,buf_size);
							buf_size = fread(buf, 1, SCRATCH_BUFSIZE, f);
							if (buf_size > 0)
							{
								if(httpd_resp_send_chunk(req, buf, buf_size) != ESP_OK)
								{
									fclose(f);
								}
							}
						}while (buf_size != 0);
						fclose(f);
					}
   				}
   				else if ( strcmp(f_mode, "dele") == 0 )
   				{
   					if( remove(f_path) == 0 )
   					{
   						ESP_LOGI(LOG, "Remove successful");
   					}
   					else
   					{
   						ESP_LOGI(LOG, "Remove unsuccessful");
   					}
   				}
   				else
   				{
   					ESP_LOGI(LOG, "ERRO");
   				}
   			}
   		}
    }

    memset(buf, 0, buf_size);
    sprintf(buf, "<html><h4>Current File List</h4><table border=1> <tr><td>No</td><td>File_Name</td><td>Download</td><td>Play</td><td>Delete</td><td>File_Size</td></tr>");
    httpd_resp_send_chunk(req, buf, buf_size);
    memset(buf, 0, buf_size);

    if (d) {
    	while( (dir = readdir(d)) != NULL )
    	{
    		if ( strstr(dir->d_name, ".mjpeg") != NULL )
    		{
    			f_count = f_count + 1;
    			memset(f_path, 0, sizeof(f_path));
    			sprintf(f_path, "/sdcard/%s", dir->d_name);

    			ESP_LOGI(LOG, "F Name : %s", f_path);
    			f_size = 0;
    			f = fopen(f_path,"r");
    			fseek(f, 0, SEEK_END);
    			f_size = ftell(f);

    			f_info_size = f_size / 1024;
    			f_tot_size = f_tot_size + f_info_size;

    			fclose(f);

      			sprintf(buf, "<tr><td>%d</td><td>%s</td><td><a href = \"http://192.168.4.1/flist?file=%s&mode=down\" download=\"%s\">Download</a></td>", f_count, dir->d_name, dir->d_name, dir->d_name);
      			httpd_resp_send_chunk(req, buf, buf_size);
      			memset(buf, 0, buf_size);
      			sprintf(buf, "<td><a href = \"http://192.168.4.1/play?file=%s\">Play</a></td><td><a href = \"http://192.168.4.1/flist?file=%s&mode=dele\">Delete</a></td><td>%lu kb</td></tr>", dir->d_name, dir->d_name, f_info_size);
        		httpd_resp_send_chunk(req, buf, buf_size);
        		memset(buf, 0, buf_size);
        		ESP_LOGI(LOG,"%s", dir->d_name);
    		}
    	}
    	closedir(d);
    }
    memset(buf, 0, buf_size);
    float percent = f_tot_size / tot_kb;
    sprintf(buf, "</table><br>Total File Found : %d<br>Space Capacity : %u / %u Kb ( %0.2f PERCENT )<br><a href = \"http://192.168.4.1/config\">Back to Config</a></html>", f_count, f_tot_size , tot_kb, percent);
    httpd_resp_send_chunk(req, buf, buf_size);
    httpd_resp_send_chunk(req, NULL, 0);

    ESP_LOGI(LOG, "DONE");
    return 0;
}

/*
 * configuration page
 * - time : YYYYMMDDHHMM
 * - size :
 *   QVGA(320x240)  - 1
 *   VGA (640x480)  - 2
 *   SVGA(800x600)  - 3
 *   XGA (1024x768) - 4
 *   SXGA(1280x1024)- 5
 *   UXGA(1600x1200)- 6
 * - fps : 1~50
 * - qly : 1~50
 * - vflip : 1 - Mode 1 / 2 - Flip
 * - hflip : 1 - Mode 1 / 2 - Flip
 * - gps : 1 - GPS OFF / 2 - GPS ON
 * -
 */
int get_config(httpd_req_t *req)
{
    char buf[SCRATCH_BUFSIZE];
    size_t buf_size = sizeof(buf);

    size_t qur_len;
    char *qur;

    sys_config_struct sys_config;

    int ret_val = 0;

    char c_time[13];
    char c_time1[9];
    char c_time2[5];
    char c_size[2];
    char c_fps[3];
    char c_qly[3];
    char c_vflip[2];
    char c_hflip[2];
    char c_gps[2];

    qur_len = httpd_req_get_url_query_len(req) + 1;
    if (qur_len > 1)
    {
     	qur = malloc(qur_len);
  		if (httpd_req_get_url_query_str(req, qur, qur_len) == ESP_OK)
  		{
  			ESP_LOGI(LOG,"Query received %s", qur);

  			if( httpd_query_key_value(qur, "time", c_time, sizeof(c_time)) == ESP_OK )
  			{
  				for ( int i = 0 ; i < 8 ; i++ )
  				{
  					c_time1[i] = c_time[i];
  				}
  				for ( int j = 0 ; j < 4 ; j++ )
  				{
  					c_time2[j] = c_time[j+8];
  				}
  				sys_config_time1( charToInt(c_time1, 8) );
  				sys_config_time2( charToInt(c_time2, 4) );
  				ESP_LOGI(LOG, "CONFIG TIME : %d %d %d %d %d %d", time_st.tm_year, time_st.tm_mon, time_st.tm_mday, time_st.tm_hour, time_st.tm_min, time_st.tm_sec );
  				time_t_val = mktime(&time_st);
  				struct timeval timeval_val;
  				timeval_val.tv_sec = time_t_val;
  				settimeofday(&timeval_val, NULL);
  				ESP_LOGI(LOG, "Val %d %d", charToInt(c_time1, 8) , charToInt(c_time1, 4));
  				ret_val = 1;
  			}

  			if( httpd_query_key_value(qur, "size", c_size, sizeof(c_size)) == ESP_OK )
  			{

  				ESP_LOGI(LOG, "%d", charToInt(c_size, 1) );
  				if( cam_config_size(charToInt(c_size, 1)) != 0 ) ret_val = 1;
  			}

  			if( httpd_query_key_value(qur, "fps", c_fps, sizeof(c_fps)) == ESP_OK )
  			{
  				if( cam_config_fps(charToInt(c_fps, 2)) != 0 ) ret_val = 1;
  			}

  			if( httpd_query_key_value(qur, "quality", c_qly, sizeof(c_qly)) == ESP_OK )
  			{
  				if( cam_config_quality(charToInt(c_qly, 2)) != 0 ) ret_val = 1;
  			}

  			if( httpd_query_key_value(qur, "vflip", c_vflip, sizeof(c_vflip)) == ESP_OK )
  			{

  			}

  			if( httpd_query_key_value(qur, "hflip", c_hflip, sizeof(c_hflip)) == ESP_OK )
  			{

  			}

  			if( httpd_query_key_value(qur, "gps", c_gps, sizeof(c_gps)) == ESP_OK )
  			{

  			}
  		}
    }
    memset(buf,0,buf_size);
    ESP_LOGI(LOG,"BEFORE_SYS_CONFIG");
    get_sys_configValue(&sys_config);
    ESP_LOGI(LOG,"AFTER_SYS_CONFIG");

    sprintf(buf,"<script>function Func_initVal(){ \
    		  document.getElementById(\"sys_time1\").innerHTML = %d; \
    		  document.getElementById(\"sys_time2\").innerHTML = %d; \
    		  document.getElementById(\"sys_size\").innerHTML = %d; \
    		  document.getElementById(\"sys_fps\").innerHTML = %d; \
    		  document.getElementById(\"sys_quality\").innerHTML = %d; \
    		  document.getElementById(\"sys_vflip\").innerHTML = %d; \
    		  document.getElementById(\"sys_hflip\").innerHTML = %d;}</script>", \
			  sys_config.sys_time_1, sys_config.sys_time_2, sys_config.sys_size, sys_config.sys_fps, sys_config.sys_quality, sys_config.sys_vflip, sys_config.sys_hflip);
    httpd_resp_send_chunk(req, buf, buf_size);

    memset(buf,0,buf_size);
    FILE* f;
    f = fopen("/sdcard/system/config.html","r");
    if ( f != NULL ) ESP_LOGI(LOG, "HTML FILE Read successful!");
    do
    {
		buf_size = fread(buf, 1, SCRATCH_BUFSIZE, f);
		if (buf_size > 0)
		{
			if(httpd_resp_send_chunk(req, buf, buf_size) != ESP_OK)
			{
				fclose(f);
			}
		}
    }while (buf_size!=0);
    fclose(f);

    httpd_resp_send_chunk(req, NULL, 0);

    return 0;
}

int get_play(httpd_req_t *req)
{
    size_t qur_len;
    char *qur;

    qur_len = httpd_req_get_url_query_len(req) + 1;

    char file_name[100];
    char buf[SCRATCH_BUFSIZE];
    size_t buf_size;
    size_t meta_size;
    char frameNo_c[9];
    char fileSize_c[9];
    int frameNo = 0;
    int fileSize = 0;
    int remainSize = 0;

    FILE* f;
    FILE* f_meta;

    memset(buf,0,SCRATCH_BUFSIZE);

    if (qur_len > 1)
    {
     	qur = malloc(qur_len);
  		if (httpd_req_get_url_query_str(req, qur, qur_len) == ESP_OK)
  		{
  			ESP_LOGI(LOG,"Query received %s", qur);

  			if( httpd_query_key_value(qur, "file", file_name, sizeof(file_name)) == ESP_OK )
  			{
  				sprintf(buf, "/sdcard/%s", file_name);
  				f = fopen(buf, "r");
  				memset(buf,0,SCRATCH_BUFSIZE);

  				int i = 0;
  				int found = 0;
  				do
  				{
  					if( file_name[i] == '.' ) found = 1;

  					if(found >= 1)
  					{
  						switch(found)
  						{
							case 1 :
								file_name[i] = '_';
								break;
							case 2 :
								file_name[i] = 'm';
								break;
							case 3 :
								file_name[i] = '.';
								break;
							case 4 :
								file_name[i] = 't';
								break;
							case 5 :
								file_name[i] = 'x';
								break;
							case 6 :
								file_name[i] = 't';
								break;
							default :
								break;
  						}
  						found = found+1;
  					}
  					i++;
  				}while(found<=6);

  				sprintf(buf, "/sdcard/%s", file_name);
  				ESP_LOGI(LOG, "META : %s", file_name);
  				f_meta = fopen(buf,"r");
  				if (f_meta != NULL) ESP_LOGI(LOG,"SUCCESSFUL!");
  				memset(buf,0,SCRATCH_BUFSIZE);

  				httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  				ESP_LOGI(LOG, "FILE OPENED");

  				sprintf(buf,"<html><a href=\"http://192.168.4.1/flist\">FILE_LIST</a>");
  				httpd_resp_send_chunk(req, buf, SCRATCH_BUFSIZE);
  				memset(buf,0,SCRATCH_BUFSIZE);

  				while(fread(frameNo_c, 1, 8, f_meta) > 0)
  				{
					fread(fileSize_c, 1, 8, f_meta);
					fseek(f_meta,16,SEEK_CUR);

					frameNo = charToInt(frameNo_c, 8);
					fileSize = charToInt(fileSize_c, 8);

					ESP_LOGI(LOG, "String - frmaeNo : %s, fileSize : %s", frameNo_c, fileSize_c);
					ESP_LOGI(LOG, "INT - frmaeNo : %d, fileSize : %d", frameNo, fileSize);

		            size_t hlen = snprintf((char *)buf, SCRATCH_BUFSIZE, _STREAM_PART, fileSize);
		            httpd_resp_send_chunk(req, (const char *)buf, hlen);
		            memset(buf,0,SCRATCH_BUFSIZE);

		            remainSize = fileSize;
					do
					{
						if ( remainSize < SCRATCH_BUFSIZE )
						{
							buf_size = fread(buf, 1, remainSize, f);
						}
						else
						{
							buf_size = fread(buf, 1, SCRATCH_BUFSIZE, f);
						}
						httpd_resp_send_chunk(req, buf, buf_size);
						memset(buf,0,SCRATCH_BUFSIZE);
						remainSize = remainSize - buf_size;
					}while(remainSize > 0);
					httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
  				};


  				/*
  				do
  				{
  					buf_size = fread(buf, 1, SCRATCH_BUFSIZE, f);
  					httpd_resp_send_chunk(req, buf, buf_size);
  				}
  				while( buf_size >= 0 );
  				*/
  	  	  	    fclose(f);
  	  	  	    fclose(f_meta);
  			}
  		}
    }
	return 0;
}

httpd_uri_t uri_control = {
		.uri	= "/control",
		.method = HTTP_GET,
		.handler = get_control,
		.user_ctx = NULL
};

httpd_uri_t uri_flist = {
		.uri	= "/flist",
		.method = HTTP_GET,
		.handler = get_flist,
		.user_ctx = NULL
};

httpd_uri_t uri_config = {
		.uri	= "/config",
		.method = HTTP_GET,
		.handler = get_config,
		.user_ctx = NULL
};

httpd_uri_t uri_play = {
		.uri	= "/play",
		.method = HTTP_GET,
		.handler = get_play,
		.user_ctx = NULL
};


/*
 * WEB Server main part
 */

int start_control_server()
{
	httpd_config_t config = HTTPD_DEFAULT_CONFIG();

	if ( httpd_start(&server, &config) == ESP_OK )
	{
		httpd_register_uri_handler(server, &uri_control);
		ESP_LOGI(LOG, "CONTROL SERVER STARTED");
		//if (httpd_start(&server, &config) == ESP_OK)
		//{
		        httpd_register_uri_handler(server, &uri_flist);
		        httpd_register_uri_handler(server, &uri_config);
		        httpd_register_uri_handler(server, &uri_play);
		        ESP_LOGI(LOG,"WEBI STARTED_SUCCESS");
		        return 1;
		//}
		//else
		//{
		//	ESP_LOGI(LOG,"WEBI STARTED_FAILED");
		//}
	}

	ESP_LOGI(LOG, "CONTROL SERVER FAILED");

	return 0;
}

int stop_control_server()
{
	if(server_control != NULL)
	{
		if( httpd_stop(server_control) == ESP_OK )
			return 1;
	}
	return 0;
}

int start_webserver()
{
    httpd_config_t config = HTTPD_DEFAULT_CONFIG();

    if (httpd_start(&server, &config) == ESP_OK) {
        httpd_register_uri_handler(server, &uri_flist);
        httpd_register_uri_handler(server, &uri_config);
        httpd_register_uri_handler(server, &uri_play);
        ESP_LOGI(LOG,"WEBI STARTED_SUCCESS");
        return 1;
    }
    return 0;
}

int stop_webserver()
{
	if(server != NULL)
	{
		if( httpd_stop(server) == ESP_OK )
			return 1;
	}
	return 0;
}


/*
 * Query handler helper functions
 */
int charToInt(char *buf, int limit)
{
	int ret_val = 0;
	for ( int i = 0 ; i < limit ; i++ )
	{
		ret_val = ret_val + ( buf[i] - '0' ) * pow( (double)10, ( limit - i - 1 ) );
	}
	ESP_LOGI(LOG, "BufVal : %d", ret_val);
	return ret_val;
}

int sys_config_time1(int val)
{
	int day = ( val ) % 100;
	int month = ( val / 100 ) % 100 -1 ;
	int year = ( val / 10000 ) - 1900;

	time_st.tm_year = year;
	time_st.tm_mon = month;
	time_st.tm_mday = day;

	ESP_LOGI(LOG, "YMD : %d %d %d", time_st.tm_year, time_st.tm_mon, time_st.tm_mday);

	return 1;
}

int sys_config_time2(int val)
{
	int hour = ( val / 100 );
	int minute = val % 100;

	time_st.tm_hour = hour;
	time_st.tm_min = minute;
	time_st.tm_sec = 0;
	time_st.tm_isdst = -1;

	ESP_LOGI(LOG, "YMD : %d %d", time_st.tm_hour, time_st.tm_min);
	return 1;
}

void get_sys_configValue(sys_config_struct* val)
{
	time_t rawtime;
	struct tm * timeinfo;
	ESP_LOGI(LOG, "1");
	time(&rawtime);
	ESP_LOGI(LOG, "2");
	timeinfo = localtime(&rawtime);
	ESP_LOGI(LOG, "READ : %d / %d / %d / %d / %d", timeinfo->tm_year, timeinfo->tm_mon, timeinfo->tm_mday, timeinfo->tm_hour, timeinfo->tm_min);

	val->sys_time_1 = ( (timeinfo->tm_year) + 1900 )*10000 + ( (timeinfo->tm_mon) + 1 )*100 + (timeinfo->tm_mday);
	val->sys_time_2 = ((timeinfo->tm_hour))*100 + (timeinfo->tm_min);
	val->sys_size = get_cam_config_size();
	val->sys_fps = get_cam_config_fps();
	val->sys_quality = get_cam_config_quality();
	val->sys_vflip = 0;
	val->sys_hflip = 0;
}

/*
 * State setter function
 */
void State_Write(int val)
{
	State_Request = val;
	ESP_LOGI(LOG, "STATE SET : %d", State);
}

int State_Read()
{
	return State_Request;
}



