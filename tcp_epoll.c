#include<stdio.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/socket.h>
#include<sys/epoll.h>
#include<unistd.h>
#include<netinet/in.h>
#include<arpa/inet.h>
#include<string.h>

static void usage(const char *proc)
{
	printf("usage:%s [ip] [port]\n",proc);
}

int startup(char *ip,int port)
{
	int sock = socket(AF_INET,SOCK_STREAM,0);
	if(sock < 0){
		perror("socket\n");
		return 2;
	}

	struct sockaddr_in local;
	local.sin_family = AF_INET;
	local.sin_port = htons(port);
	local.sin_addr.s_addr = inet_addr(ip);

    if(bind(sock,(struct sockaddr *)&local,sizeof(local)) < 0){
   		perror("bind\n");
   		return 3;
   	}
    
   	if(listen(sock,5) < 0){
		perror("listen");
		return 4;
	}

	return sock;
}

int main(int argc,char *argv[])
{
	if(argc != 3){
		usage(argv[0]);
		return 1;
	}

	int listen_sock = startup(argv[1],atoi(argv[2]));

	int epfd = epoll_create(256);
	if(epfd < 0){
		perror("epoll_create");
		return 5;
	}

	struct epoll_event _ev;
	_ev.events = EPOLLIN;
	_ev.data.fd = listen_sock;
	epoll_ctl(epfd,EPOLL_CTL_ADD,listen_sock,&_ev);

	struct epoll_event _ready_ev[128];
	int _ready_evs = 128;
	int _timeout = -1;
	int nums = 0;

	int done = 0;
	while(!done){
		switch(nums = epoll_wait(epfd,_ready_ev,_ready_evs,_timeout)){
			case 0:
				printf("timeout...\n");
				break;
			case -1:
				perror("epoll_wait");
				break;
			default:
				{
					int i = 0;
					for(;i < nums;++i){
						int _fd = _ready_ev[i].data.fd;
						if(_fd == listen_sock && _ready_ev[i].events & EPOLLIN){
							struct sockaddr_in peer;
							socklen_t len = sizeof(peer);
							int new_sock = accept(listen_sock,(struct sockaddr *)&peer,&len);
							if(new_sock > 0){
								printf("client info:socket:%s:%d\n",inet_ntoa(peer.sin_addr),ntohs(peer.sin_port));

								_ev.events = EPOLLIN;
								_ev.data.fd = new_sock;
								epoll_ctl(epfd,EPOLL_CTL_ADD,new_sock,&_ev);
							}
						}else{
							if(_ready_ev[i].events & EPOLLIN){
								char buf[1024];
								memset(buf,'\0',sizeof(buf));
								ssize_t _s = recv(_fd,buf,sizeof(buf)-1,0);
								if(_s > 0){
									buf[_s-1] = '\0';
									printf("client# %s\n",buf);
								}else if(_s == 0){
									printf("client close...");
									epoll_ctl(epfd,EPOLL_CTL_DEL,_fd,NULL);
									close(_fd);
								}else{
									perror("recv");
									return 6;
								}
							}							
						}
					}
				}
				break;
		}			
	}

	return 0;
}
