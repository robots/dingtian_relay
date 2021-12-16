/* Copyright (c) 2001, Swedish Institute of Computer Science.
  * All rights reserved.
  *
  * Redistribution and use in source and binary forms, with or without
  * modification, are permitted provided that the following conditions
  * are met:
  * 1. Redistributions of source code must retain the above copyright
  *    notice, this list of conditions and the following disclaimer.
  * 2. Redistributions in binary form must reproduce the above copyright
  *    notice, this list of conditions and the following disclaimer in the
  *    documentation and/or other materials provided with the distribution.
  * 3. Neither the name of the Institute nor the names of its contributors
  *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE INSTITUTE AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE INSTITUTE OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * httpd.c
 *
 * Author : Adam Dunkels <adam@sics.se>
 *
 */
#include "platform.h"

#include "config.h"
#include "ee_config.h"
#include "iap.h"
#include "httpserver.h"
#include "lwip/tcp.h"
#include "fsdata.h"
#include "fsdata.c"
#include "flash_if.h"
#include "systime.h"
#include <string.h>
#include <stdio.h>

#ifdef USE_IAP_HTTP

static __IO uint32_t DataFlag=0;
static __IO uint32_t size =0;
static uint32_t TotalReceived=0;
static uint32_t ContentLengthOffset =0,BrowserFlag=0;
static __IO uint32_t TotalData=0;
static uint32_t DataOffset, FilenameOffset;

enum htmlpage
{
  LoginPage,
  FileUploadPage,
  UploadDonePage,
  ResetDonePage,
};

struct http_state
{
  char *file;
  uint32_t left;
  enum htmlpage htmlpage;
	uint8_t header_ok:1;
	uint8_t abort_iap:1;
};


static const char octet_stream[14] = 
/* "octet-stream" */
{0x6f, 0x63, 0x74, 0x65, 0x74, 0x2d, 0x73, 0x74, 0x72, 0x65, 0x61, 0x6d,0x0d, };
static const char Content_Length[17] = 
/* Content Length */
{0x43, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x4c, 0x65, 0x6e, 0x67,0x74, 0x68, 0x3a, 0x20, };


static uint32_t Parse_Content_Length(char *data, uint32_t len);
static int fs_open(char *name, struct fs_file *file);


/**
  * @brief  callback function for handling connection errors
  * @param  arg: pointer to an argument to be passed to callback function
  * @param  err: LwIP error code   
  * @retval none
  */
static void conn_err(void *arg, err_t err)
{
  (void)err;

  struct http_state *hs;

  hs = arg;
  mem_free(hs);
}

/**
  * @brief  closes tcp connection
  * @param  pcb: pointer to a tcp_pcb struct
  * @param  hs: pointer to a http_state struct
  * @retval
  */
static void close_conn(struct tcp_pcb *pcb, struct http_state *hs)
{

  tcp_arg(pcb, NULL);
  tcp_sent(pcb, NULL);
  tcp_recv(pcb, NULL);
  mem_free(hs);
  tcp_close(pcb);
}

/**
  * @brief sends data found in  member "file" of a http_state struct
  * @param pcb: pointer to a tcp_pcb struct
  * @param hs: pointer to a http_state struct
  * @retval none
  */
static void send_data(struct tcp_pcb *pcb, struct http_state *hs)
{
  err_t err;
  u16_t len;

  /* We cannot send more data than space available in the send
     buffer. */
  if (tcp_sndbuf(pcb) < hs->left)
  {
    len = tcp_sndbuf(pcb);
  }
  else
  {
    len = hs->left;
  }
	if (hs->file != NULL) {
		err = tcp_write(pcb, hs->file, len, 0);

		if (err == ERR_OK)
		{
			hs->file += len;
			hs->left -= len;
		}
	}
}

/**
  * @brief tcp poll callback function
  * @param arg: pointer to an argument to be passed to callback function
  * @param pcb: pointer on tcp_pcb structure
  * @retval err_t
  */
static err_t http_poll(void *arg, struct tcp_pcb *pcb)
{
  if (arg == NULL)
  {
    tcp_close(pcb);
  }
  else
  {
    send_data(pcb, (struct http_state *)arg);
  }
  return ERR_OK;
}

/**
  * @brief callback function called after a successfull TCP data packet transmission  
  * @param arg: pointer to an argument to be passed to callback function
  * @param pcb: pointer on tcp_pcb structure
  * @param len
  * @retval err : LwIP error code
  */
static err_t http_sent(void *arg, struct tcp_pcb *pcb, u16_t len)
{
  (void)len;
  struct http_state *hs;

  hs = arg;

  if (hs->left > 0)
  {
    send_data(pcb, hs);
  }
  else
  {
    if(hs->htmlpage == ResetDonePage)
    {
			tcp_output(pcb);
			systime_delay(10);
			tcp_abort(pcb);
			systime_delay(10);
      NVIC_SystemReset();
      while(1);
    } else {
			close_conn(pcb, hs);
		}
      
  }
  return ERR_OK;
}


/**
  * @brief callback function for handling TCP HTTP traffic
  * @param arg: pointer to an argument structure to be passed to callback function
  * @param pcb: pointer to a tcp_pcb structure
  * @param p: pointer to a packet buffer
  * @param err: LwIP error code
  * @retval err
  */
static err_t http_recv(void *arg, struct tcp_pcb *pcb,  struct pbuf *p, err_t err)
{
  int32_t i,len=0;
  char *data, *ptr;
  //char filename[13];
  struct fs_file file = {0, 0};
  struct http_state *hs;
	struct pbuf *q;
  
  hs = arg;

  if (err == ERR_OK && p != NULL)
  {
    /* Inform TCP that we have taken the data */
    tcp_recved(pcb, p->tot_len);

    if (hs->file != NULL)
    {
      pbuf_free(p);
      close_conn(pcb,hs);
      return ERR_OK;
    }

    q = p;
    while (q != NULL) {
      data = q->payload;
      len = q->len;
      q = q->next;

      /* process HTTP GET requests*/
      if (strncmp(data, "GET /", 5) == 0)
      {
				if (strncmp(data, "GET /favicon.ico", 16) == 0) {
          fs_open("/favicon.ico", &file);
				} else if (strncmp(data, "GET /style.css", 14) == 0) {
          fs_open("/style.css", &file);
				} else if ((strncmp(data, "GET /resetmcu.cgi", 17) == 0)) {
          fs_open("/reset.html", &file);
          hs->htmlpage = ResetDonePage;
        } else {
          fs_open("/index.html", &file);
          hs->htmlpage = LoginPage;
        }

        hs->file = file.data;
        hs->left = file.len;
        //pbuf_free(p);

        /* send reset.html page */
        send_data(pcb, hs);

        /* Tell TCP that we wish be to informed of data that has been
        successfully sent by a call to the http_sent() function. */
        tcp_sent(pcb, http_sent);
      }

      /* process POST request for checking login */
      else if ((strncmp(data, "POST /checklogin.cgi", 20) == 0) && (hs->htmlpage == LoginPage))
      {
				uint8_t login = 0;
        char *x;
				char *username = "username=";
				uint32_t username_len = strlen(username);
				char *password = "&password=";
				uint32_t password_len = strlen(password);
				char *loginfld = "&login=";
				uint32_t loginfld_len = strlen(loginfld);

        if ((x = strstr(data, username))) {
					uint32_t lenn = MIN(strlen(ee_config.username), EE_USERNAME_LEN);
					x += username_len;
          if (strncmp(x, ee_config.username, lenn) == 0) {
            x += lenn;
            if (strncmp(x, password, password_len) == 0) {
              x += password_len;
							lenn = MIN(strlen(ee_config.password), EE_PASSWORD_LEN);
              if (strncmp(x, ee_config.password, lenn) == 0) {
								x += lenn;
								if (strncmp(x, loginfld, loginfld_len) == 0) {
									login = 1;
								}
              }
            }
          }
        }

        if (login) {
          fs_open("/upload.html", &file);
          hs->htmlpage = FileUploadPage;
        } else {
          /* reload index.html */
          fs_open("/index.html", &file);
          hs->htmlpage = LoginPage;
        }
        hs->file = file.data;
        hs->left = file.len;

        send_data(pcb, hs);

        /* Tell TCP that we wish be to informed of data that has been
        successfully sent by a call to the http_sent() function. */
        tcp_sent(pcb, http_sent); 

        pbuf_free(p);
        return ERR_OK;

      }
    
      /* process POST request for file upload and incoming data packets after POST request*/
      else if (((strncmp(data, "POST /upload.cgi", 16) == 0) || (DataFlag >= 1)) && (hs->htmlpage == FileUploadPage))
      { 
        DataOffset =0;
        
        /* POST Packet received */
        if (DataFlag ==0)
        { 
          BrowserFlag=0;
          TotalReceived =0;
          
          /* parse packet for Content-length field */
          size = Parse_Content_Length(data, p->tot_len);

          /* parse packet for the octet-stream field */
          for (i=0;i<len;i++)
          {
             if (strncmp ((char*)(data+i), octet_stream, 13)==0)
             {
               DataOffset = i+16;
               break;
             }
          }  
          /* case of MSIE8 : we do not receive data in the POST packet*/ 
          if (DataOffset == 0)
          {
             DataFlag++;
             BrowserFlag = 1;
             continue;
             //pbuf_free(p);
             //return ERR_OK;
             
          }
          /* case of Mozilla Firefox v3.6 : we receive data in the POST packet*/
          else
          {
            TotalReceived = len - (ContentLengthOffset + 4);
          }
        }
        
        if (((DataFlag == 1) && (BrowserFlag == 1)) || ((DataFlag == 0) && (BrowserFlag == 0)))
        { 
           if ((DataFlag == 0) && (BrowserFlag == 0)) 
           {
             DataFlag++;
           }
           else if ((DataFlag ==1)&&(BrowserFlag==1))
           {
             /* parse packet for the octet-stream field */
             for (i=0;i<len;i++)
             {
               if (strncmp ((char*)(data+i), octet_stream, 13)==0)
               {
                 DataOffset = i+16;
                 break;
               }
             }
             TotalReceived+=len;
             DataFlag++;
           }  
                
           /* parse packet for the filename field */
           FilenameOffset = 0;
           for (i=0;i<len;i++)
           {
             if (strncmp ((char*)(data+i), "filename=", 9)==0)
             {
                FilenameOffset = i+10;
                break;
             }
           }           
           i =0;
           if (FilenameOffset)
           {
             while((*(data+FilenameOffset + i)!=0x22 )&&(i<13))
             {
               //filename[i] = *(data+FilenameOffset + i);
               i++;
             }
             //filename[i] = 0x0;
           }
           
					 if (i==0)
           {
             /* no filename, in this case reload upload page */
             fs_open("/upload.html", &file);
             hs->htmlpage = FileUploadPage;
             hs->file = file.data;
             hs->left = file.len;
             
             /* send index.html page */ 
             send_data(pcb, hs);

             /* Tell TCP that we wish be to informed of data that has been
             successfully sent by a call to the http_sent() function. */
             tcp_sent(pcb, http_sent); 
             DataFlag=0;

             pbuf_free(p);
             return ERR_OK;
           }
             
          
           TotalData =0 ;
         }
         /* DataFlag >1 => the packet is data only  */
         else 
         {
           TotalReceived += len;
         }
        
         ptr = (char*)(data + DataOffset);
         len -= DataOffset;
        
         /* update Total data received counter */
         TotalData += len;

					if (hs->header_ok == 0) {
						// we expect the header to come in one piece. (as MTUis > 1.4k, and HTTPheader + our header is < 200bytes)
						hs->header_ok = 1;
						hs->abort_iap = 1;
						struct iap_header_t ih;

						memcpy(&ih, ptr, sizeof(struct iap_header_t));
						hs->abort_iap = iap_init(&ih);

						ptr += sizeof(struct iap_header_t);
						len -= sizeof(struct iap_header_t);
					}

					/* write data in flash */
					if (len && hs->abort_iap == 0 && hs->header_ok == 1) {
						iap_write_data(ptr, len, 0);
					}

					if (iap_write_done()) {
						DataFlag=0;

						if ((hs->abort_iap == 1) || (iap_check_fw() == 0)) {
							fs_open("/uploaderror.html", &file);
						} else {
							/* send uploaddone.html page */
							fs_open("/uploaddone.html", &file);
							hs->htmlpage = UploadDonePage;
						}
						hs->file = file.data;
						hs->left = file.len;
						send_data(pcb, hs);

						/* Tell TCP that we wish be to informed of data that has been
						 successfully sent by a call to the http_sent() function. */
						tcp_sent(pcb, http_sent);
					}
      }
      else
      {
        pbuf_free(p);
        close_conn(pcb, hs);
        return ERR_OK;
      }
    }

    pbuf_free(p);
		return ERR_OK;
  }

  if (err == ERR_OK && p == NULL)
  {
    close_conn(pcb, hs);
  }
  return ERR_OK;
}

/**
  * @brief  callback function on TCP connection setup ( on port 80)
  * @param  arg: pointer to an argument structure to be passed to callback function
  * @param  pcb: pointer to a tcp_pcb structure
  * &param  err: Lwip stack error code
  * @retval err
  */
static err_t http_accept(void *arg, struct tcp_pcb *pcb, err_t err)
{
	(void)arg;
	(void)err;
  struct http_state *hs;

  /* Allocate memory for the structure that holds the state of the connection */
  hs = mem_malloc(sizeof(struct http_state));

  if (hs == NULL)
  {
    return ERR_MEM;
  }

  /* Initialize the structure. */
  hs->file = NULL;
  hs->left = 0;
	hs->header_ok = 0;
	hs->abort_iap = 0;

  /* Tell TCP that this is the structure we wish to be passed for our
     callbacks. */
  tcp_arg(pcb, hs);

  /* Tell TCP that we wish to be informed of incoming data by a call
     to the http_recv() function. */
  tcp_recv(pcb, http_recv);

  tcp_err(pcb, conn_err);

  tcp_poll(pcb, http_poll, 10);
  return ERR_OK;
}

/**
  * @brief  intialize HTTP webserver  
  * @param  none
  * @retval none
  */
void IAP_httpd_init(void)
{
  struct tcp_pcb *pcb;
  /*create new pcb*/
  pcb = tcp_new();
  
  if (!pcb)
  {
    return ;
  }
  /* bind HTTP traffic to pcb */
  tcp_bind(pcb, IP_ADDR_ANY, 80);
  /* start listening on port 80 */
  pcb = tcp_listen(pcb);
  /* define callback function for TCP connection setup */
  tcp_accept(pcb, http_accept);
}

/**
  * @brief  Opens a file defined in fsdata.c ROM filesystem 
  * @param  name : pointer to a file name
  * @param  file : pointer to a fs_file structure  
  * @retval  1 if success, 0 if fail
  */
static int fs_open(char *name, struct fs_file *file)
{
  struct fsdata_file_noconst *f;

  for (f = (struct fsdata_file_noconst *)FS_ROOT; f != NULL; f = (struct fsdata_file_noconst *)f->next)
  {
    if (!strcmp(name, (char *)f->name))
    {
      file->data = (char *)f->data;
      file->len = f->len;
      return 1;
    }
  }

  return 0;
}

/**
  * @brief  Extract the Content_Length data from HTML data  
  * @param  data : pointer on receive packet buffer 
  * @param  len  : buffer length  
  * @retval size : Content_length in numeric format
  */
static uint32_t Parse_Content_Length(char *data, uint32_t len)
{
  uint32_t i=0,imagesize=0, S=1;
  int32_t j=0;
  char sizestring[6], *ptr;
   
  ContentLengthOffset =0;
  
  /* find Content-Length data in packet buffer */
  for (i=0;i<len;i++)
  {
    if (strncmp ((char*)(data+i), Content_Length, 16)==0)
    {
      ContentLengthOffset = i+16;
      break;
    }
  }
  /* read Content-Length value */
  if (ContentLengthOffset)
  {
    i=0;
    ptr = (char*)(data + ContentLengthOffset);
    while(*(ptr+i)!=0x0d)
    {
      sizestring[i] = *(ptr+i);
      i++;
      ContentLengthOffset++; 
    }
    if (i>0)
    {
      /* transform string data into numeric format */
      for(j=i-1;j>=0;j--)
      {
        imagesize += (sizestring[j]-0x30)*S;
        S=S*10;
      }
    }
  }
  return imagesize;
}

#endif
