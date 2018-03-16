// PLEASE CHECK BUILD INSTRUCTIONS 


#include <stdio.h>
#include <stdlib.h>
#include <cstdlib>
#include <string>
#include <cstring>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>
#include <iostream>
#include <map>
#include <signal.h>
#include <algorithm>
#include <sstream>


using namespace std;

#define MAXEVENTS 64

static int
make_socket_non_blocking (int sfd)
{
  int flags, s;

  flags = fcntl (sfd, F_GETFL, 0);
  if (flags == -1)
    {
      perror ("fcntl");
      return -1;
    }

  flags |= O_NONBLOCK;
  s = fcntl (sfd, F_SETFL, flags);
  if (s == -1)
    {
      perror ("fcntl");
      return -1;
    }

  return 0;
}

static int
create_and_bind (char *port)
{
  struct addrinfo hints;
  struct addrinfo *result, *rp;
  int s, sfd;

  memset (&hints, 0, sizeof (struct addrinfo));
  hints.ai_family = AF_UNSPEC;       // ipv4 or ipv6
  hints.ai_socktype = SOCK_STREAM;  //for tcp
  hints.ai_flags = AI_PASSIVE;     

  s = getaddrinfo (NULL, port, &hints, &result);
  if (s != 0)
    {
      fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
      return -1;
    }

  for (rp = result; rp != NULL; rp = rp->ai_next)
    {
      sfd = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
      if (sfd == -1)
        continue;

      s = bind (sfd, rp->ai_addr, rp->ai_addrlen);
      if (s == 0)
        {
          // We managed to bind successfully
          break;
        }

      close (sfd);
    }

  if (rp == NULL)
    {
      fprintf (stderr, "Could not bind\n");
      return -1;
    }

  freeaddrinfo (result);

  return sfd;
}

void error(const char* msg)
{
	perror(msg);
	exit(1);
}


int main(int argc, char *argv[])
{
	int sfd,s;
	int efd;
	int key,value;
	map<int,int> store;
	struct epoll_event event;
	struct epoll_event *events;

	if(argc!=2)
	{
		fprintf(stderr, "Usage: %s [port]\n", argv[0]);
		exit (EXIT_FAILURE);
	}

	sfd=create_and_bind(argv[1]);
	if (sfd==-1)
		abort();

	s=make_socket_non_blocking(sfd);
	if(s==-1)
		abort();

	s=listen(sfd,SOMAXCONN);
	if(s==-1)
		error("listen");

	efd=epoll_create1(0);
	if(efd==-1)
		error("epoll_create");

	event.data.fd=sfd;
	event.events=EPOLLIN | EPOLLET;
	s=epoll_ctl(efd, EPOLL_CTL_ADD, sfd, &event);
	if(s==-1)
		error("epoll_ctl");

	events= (epoll_event*)calloc(MAXEVENTS, sizeof event);

	while(1)
	{
		int n,i;

		n=epoll_wait(efd, events, MAXEVENTS, -1);
		for(i=0;i<n;i++)
		{
			if((events[i].events & EPOLLERR) || (events[i].events & EPOLLHUP) || (!(events[i].events & EPOLLIN)))
			{
				// An error has occured on this fd, or the socket is not ready
				fprintf(stderr, "epoll error\n");
				close(events[i].data.fd);
				continue;
			}
			else if(sfd ==events[i].data.fd)
			{
				// One or more incoming connections
				while(1)
				{
					struct sockaddr in_addr;
					socklen_t in_len;
					int infd;
					char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
					in_len=sizeof(in_addr);
					infd=accept(sfd, &in_addr, &in_len);
					if(infd ==-1)
					{
						if((errno==EAGAIN) || (errno ==EWOULDBLOCK))
							break;
						else
							error("accept");
					}

					s=getnameinfo(&in_addr, in_len, hbuf, sizeof(hbuf),sbuf,sizeof(sbuf), NI_NUMERICHOST | NI_NUMERICSERV);

					if(s==0)
						printf("Accepted connection on descriptor %d (host=%s, port=%s)\n", infd, hbuf, sbuf);

					s=make_socket_non_blocking(infd);
					if(s==-1)
						abort();

					event.data.fd=infd;
					event.events= EPOLLIN | EPOLLET;
					s=epoll_ctl(efd, EPOLL_CTL_ADD, infd, &event);
					if(s==-1)
						error("epoll_ctl");

				}
				continue;
			}

			else
			{
				// forming key value mapping
				int done=0;

				while(1)
				{
					size_t count;
					char buffer[512];

					count=read(events[i].data.fd, buffer, sizeof(buffer));
					if(count==-1)
					{
						if(errno!= EAGAIN)
						{
							done=1;
							perror("Read");
						}
						break;
					}
					else if (count ==0)
					{
						done=1;
						break;
					}

					//START MAPPING

					if(buffer[0]=='p')
					{
						string flag=buffer;
						stringstream ss(flag);
						string command;
						ss>>command>>key>>value;

						if(store.count(key)>0)
						{
							s=send(events[i].data.fd, "Key already exists",18,0);
						}
						else
						{
							store[key]=value;
							//s=send(events[i].data.fd, to_string(value.length()).c_str(),to_string(value.length()).length(),0);
							s=send(events[i].data.fd, "OK",2,0);
						}
					}

					else if(buffer[0]=='g')
					{
						string flag=buffer;
						stringstream ss(flag);	
						string command;
						ss>>command>>key;

						if(store.count(key)>0)
						{
							value=store[key];
							const char* ret= to_string(value).c_str();
							s=send(events[i].data.fd, ret, to_string(value).length(),0);
						}
						else
						{
							//cout<<key.length();
							//s=send(events[i].data.fd,to_string(key.length()).c_str(),to_string(key.length()).length(),0);
							s=send(events[i].data.fd, "Key not found", 13,0);
						}
					}

					else if(buffer[0]== 'd')
					{
						string flag=buffer;
						stringstream ss(flag);	
						string command;
						ss>>command>>key;

						if(store.count(key)>0)
						{
							store.erase(key);
							s=send(events[i].data.fd,"OK",2,0);
						}
						else
						{
							s=send(events[i].data.fd, "Key not found", 13,0);
							//s=send(events[i].data.fd,to_string(key.length()).c_str(),to_string(key.length()).length(),0);
						}
					}

					else if(buffer[0]= 'B')
					{
						s=send(events[i].data.fd, "Goodbye",7,0);
					}

					if(s<0)
						error("Error writing to socket");

					if(done)
					{
						printf("Closed connection on descriptor %d\n", events[i].data.fd);
						close(events[i].data.fd);
					}
				}	
			}
		}
	}

	free(events);

	close(sfd);

	return EXIT_SUCCESS;

}











