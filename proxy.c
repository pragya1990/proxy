#ifdef HAVE_CONFIG_H
#include <config.h>
#endif
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <string.h>
#include <netinet/if_ether.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <ctype.h>
#include <netdb.h>
#include <ifaddrs.h>

#define WIRELESS_INTERFACE "eth1" ////////////////////////////////////////////////////////////////////////////////
// #define HTTP_PACKET_SIZE 65535 //////////// searched on a lot of forums..it could be greater than 8000 so, took 10000
#define HTTP_PORT_NUMBER 80
#define HTTP_CLIENT_REQUEST_SIZE 65535 //http://www.boutell.com/newfaq/misc/urllength.html
#define INTERNAL_ERROR_SIZE 158

void get_ip_addr(char *);
int start_proxy(char *argv[]);
int make_socket_connection_with_web_client(char *argv[]);
int listen_to_client_requests();
int make_socket_connection_with_web_server();
int forward_client_request_to_server();
int read_server_response();
int forward_server_response_to_client();
void close_socket_connection_with_web_server();
int store_in_file(char *);
int validate_client_request();
int change_buffer_size();
int find_length_of_response_from_server();

int internal_error();
int unauthorised();
int moved_permanently();
int forbidden();
int not_found();
int bad_request();
int hostless_request();
		
int server_sockfd, server_len, i,rc, client_sockfd;
unsigned client_len;
struct sockaddr_in server_address;
struct sockaddr_in client_address;
char *host;
char *client_request;
char *server_response;
FILE *fp;
char *host_ip, *s;
struct hostent *host_struct;
int remote_sockfd, remote_len, remote_result;
struct sockaddr_in remote_address;
int count=0;
int start_buffer_size = 1024, current_buffer_size = start_buffer_size, maximum_buffer_size = 65535;
int response_size = start_buffer_size;

int main(int argc, char *argv[])
{
	start_proxy(argv);
}

int start_proxy(char *argv[])
{
	rc = make_socket_connection_with_web_client(argv);
	printf("initial_buffer_length: %d\n", current_buffer_size);
	if(rc == -1)
		return -1;
	while(1)
	{
		change_buffer_size();		
		if( listen_to_client_requests() == -1)
			continue;
		if( make_socket_connection_with_web_server() == -1)
			continue;
		if( forward_client_request_to_server() == -1)
			continue;
		if( read_server_response() == -1)
			continue;
		if( forward_server_response_to_client() == -1)
			continue;
		close_socket_connection_with_web_server();
		
		count++;
		free(server_response);
		free(client_request);
		free(host);
		free(host_ip);
	}
}

int change_buffer_size()
{
	if(current_buffer_size < maximum_buffer_size)
		current_buffer_size = current_buffer_size * 2;
	response_size = current_buffer_size;
	printf("current buffer length: %d\n",current_buffer_size);
}

int forward_client_request_to_server()
{
	rc = write(remote_sockfd, client_request, HTTP_CLIENT_REQUEST_SIZE);
	printf("request sent... now reading the response\n");
	return 0;
}

int read_server_response()
{
	server_response = (char *)malloc(response_size);	
	rc = read(remote_sockfd, server_response, response_size);
	store_in_file(server_response);	
	if(rc == -1)
	{	printf("read unsuccessful\n"); return -1; }
	else
	{	printf("read from remote server successful.....values:\n");
		char *str = (char *)malloc(1024); //random value
		fp = fopen("http_request.txt","r");
		for(i=0;i<2;i++)
		{	fscanf(fp, "%s", str);
			printf("%s\n",str);
		}
		if(str[0]=='2' && str[1]=='0' && str[2]=='0')
		{	int len = find_length_of_response_from_server();
			if(len > response_size) 	
				return internal_error();
			return 0;		
		}
		else if(str[0]=='2' && str[1]=='0' && str[2]=='6')
			return internal_error();
		else if(str[0]=='3' && str[1]=='0' && str[2]=='1')
		{	printf("Inside moved permanently\n");
			return moved_permanently();		
		}		
		else if(str[0]=='4' && str[1]=='0' && str[2]=='0')
		{	printf("Inside bad request\n");
			return bad_request();
		}		
		else if(str[0]=='4' && str[1]=='0' && str[2]=='1')
			return unauthorised();	
		else if(str[0]=='4' && str[1]=='0' && str[2]=='3')
			return forbidden();
		else if(str[0]=='4' && str[1]=='0' && str[2]=='4')
		{	printf("INside NOt found\n");
			return not_found();		
		}		
		else
		{
			printf("Error No: %s\n", str);
			printf("Printing the whole contents\n");
			//printf("%s\n",server_response);
			return 0;
		}
	}
}

int find_length_of_response_from_server()
{
	s = (char *)malloc(16384); //random value
	fp = fopen("http_request.txt","r");	
	int j;
	for(j=1;j<50;j++)
	{	
		fscanf(fp, "%s", s);
		printf("%s\n",s);
		if(s[8] == 'L' && s[9] == 'e' && s[10] == 'n' && s[11] == 'g' && s[12] == 't' && s[13] == 'h')
			break;
	}
	fscanf(fp, "%s", s);
	printf("Length: %s\n",s);

	FILE *flen;
	flen = fopen("length.txt","a+");
	int len=0;
	while(s[len++]!='\0');
	fwrite((const void *)s, 1, len, flen);
	fwrite("\n", 1, 2, flen);
	fclose(flen);
	fclose(fp);
	
	return atoi(s);		
}

int forward_server_response_to_client()
{
	// MAking the complete packet
	printf("sending the response back to the web server ... count: %d\n", count);	
	rc = write(client_sockfd, server_response, response_size);   
	if(rc == -1)
	{	printf("sending response to the web server UNSUCCESSFUL \n");
		return -1;	
	}	
	else
		printf("sending response to the web server SUCCESSFUL ... count: %d\n", count);			
	return 0;
}

void close_socket_connection_with_web_server()
{
	printf("closed socket connection with web server\n");
	close(remote_sockfd);
}

int make_socket_connection_with_web_client(char *argv[])
{
	server_sockfd = socket(AF_INET, SOCK_STREAM, 0);
	server_address.sin_family = AF_INET;
	
	host = (char *)malloc(sizeof(NI_MAXHOST));
	get_ip_addr(host);
	server_address.sin_addr.s_addr = htons(INADDR_ANY);
	
	server_address.sin_port = htons(atoi(argv[1]));
	server_len = sizeof(server_address);
 
	rc = bind(server_sockfd, (struct sockaddr *) &server_address, server_len);
	if(rc == -1)
	{	printf("bind failed with client\n");	
		return -1;
	}	
	rc = listen(server_sockfd, 3);
	return 0;	
}


int listen_to_client_requests()
{
	client_len = sizeof(client_address);
    	client_sockfd = accept(server_sockfd, (struct sockaddr *) &client_address, &client_len);
    	printf("after accept()... client_sockfd = %d\n", client_sockfd) ; 
	printf("client address: %s\n",inet_ntoa(*(struct in_addr *)&client_address.sin_addr.s_addr));
	client_request = (char *)malloc(HTTP_CLIENT_REQUEST_SIZE);
		
	printf("server waiting\n");
	rc = read(client_sockfd, client_request, HTTP_CLIENT_REQUEST_SIZE);   
	printf("values from the client: \n%s\n",client_request); 

	if( store_in_file(client_request) == -1)
		return -1;
	return validate_client_request();
	//return 0;
}

int store_in_file(char *data)
{
	fp = fopen("http_request.txt","w");
	rc = fwrite(data, 1,HTTP_CLIENT_REQUEST_SIZE, fp);
	if(rc != -1)      		  	
		printf("write successful for http client request......\n");
	else
	{	printf("write unsuccessful\n");
		return -1;	
	}	
	fclose(fp);
	return 0;	
}

int validate_client_request()
{
	printf("inside validate client request\n");	
	fp = fopen("http_request.txt","r");
	if(fp == NULL)
	{	printf("http_request.txt file open unsuccessful\n");
		return -1;
	}
	host_ip = (char *)malloc(2048);	
	for(i=0;i<4;i++)
	{	fscanf(fp, "%s", host_ip);
	}

	fscanf(fp, "%s", host_ip);
	printf("%s\n",host_ip);
	int h_errno;
	if( (host_struct = gethostbyname(host_ip)) == NULL)
	{	
		printf("some error while using gethostbyname: %d\n",h_errno );
		hostless_request();
		return -1;
	}
	printf("\n\nHOST NAME: %s %s\n\n",host_struct->h_name,inet_ntoa(*( struct in_addr*)(host_struct->h_addr_list[0])));
	char *h1 = (char *)malloc(16); 
	h1 = (host_struct->h_addr_list[0]);
	int len = strlen(h1) + 1;
	printf("Length of address: %d\n", len);
	struct hostent *host_struct1 = gethostbyaddr(( struct in_addr*)(host_struct->h_addr_list[0]), len, AF_INET);	
	printf("\n\nHOST NAME second: %s %s\n\n",host_struct1->h_name,inet_ntoa( *( struct in_addr*)(host_struct1->h_addr_list[0])));
	int k=0;
	char *name1 = host_struct->h_name;
	char *name2 = host_struct1->h_name;
	char ch1 = name1[k];
	while(ch1 != '\0')
	{	char ch2 = name2[k];
		if(ch1>=65 && ch1<= 90 && ch1!='.')
		{
			if(ch1 == ch2 || (ch1+32)==ch2)
			{	ch1 = name1[++k];
				continue;
			}
			else
			{	hostless_request();	
				printf("IP addresses not same\n"); return -1;}	
		}
		else if(ch1>=97 && ch1<= 122 && ch1!='.')
		{
			if(ch1 == ch2 || (ch1-32)==ch2)
			{	ch1 = name1[++k];
				continue;
			}
			else
			{	hostless_request(); printf("IP addresses not same\n"); return -1;}	
		}
		k++;
		ch1=name1[k];
	}

	fclose(fp);
	return 0;
}

int make_socket_connection_with_web_server()
{
	remote_sockfd = socket(PF_INET, SOCK_STREAM, 0);
	if (remote_sockfd == -1) { 
       		perror("Socket create failed.\n") ; 
		return -1 ; 
	} 
     
	remote_address.sin_family = AF_INET;
	remote_address.sin_addr.s_addr = inet_addr(inet_ntoa( *( struct in_addr*)(host_struct->h_addr_list[0])));  
	remote_address.sin_port = htons(HTTP_PORT_NUMBER);
	remote_len = sizeof(remote_address);

	remote_result = connect(remote_sockfd, (struct sockaddr *)&remote_address, remote_len);
	if(remote_result == -1)
	{       perror("Error has occurred while connecting web server with remote server");
		return -1;
	}
	return 0;
}

void get_ip_addr(char *host)
{
	struct ifaddrs *ifap;
	getifaddrs(&ifap);
	char *iface = (char *)WIRELESS_INTERFACE;
	while(ifap != NULL)
	{	if(ifap->ifa_addr->sa_family == AF_INET && !strcmp(iface,ifap->ifa_name) )	
		{	getnameinfo(ifap->ifa_addr,sizeof(struct sockaddr_in), host, NI_MAXHOST, NULL, 0, NI_NUMERICHOST);
			printf("address: %s %s\n", host,ifap->ifa_name);
			break;
		}
		ifap = ifap->ifa_next;
	}
}

int bad_request()
{
	FILE *finternal = fopen("bad_request_400.html","r");
	char *s = (char *)malloc(INTERNAL_ERROR_SIZE); 
	rc = fread(s,1, INTERNAL_ERROR_SIZE,finternal);
	if(rc == -1)
	{
		printf("Read from html file failed\n");
		return -1;
	}
	server_response = s;
	response_size = INTERNAL_ERROR_SIZE;
	return 0;
}

int not_found()
{
	FILE *finternal = fopen("not_found_404.html","r");
	char *s = (char *)malloc(INTERNAL_ERROR_SIZE); 
	rc = fread(s,1, INTERNAL_ERROR_SIZE,finternal);
	if(rc == -1)
	{
		printf("Read from html file failed\n");
		return -1;
	}
	server_response = s;
	response_size = INTERNAL_ERROR_SIZE;
	return 0;
}

int forbidden()
{
	FILE *finternal = fopen("forbidden_403.html","r");
	char *s = (char *)malloc(INTERNAL_ERROR_SIZE); 
	rc = fread(s,1, INTERNAL_ERROR_SIZE,finternal);
	if(rc == -1)
	{
		printf("Read from html file failed\n");
		return -1;
	}
	server_response = s;
	response_size = INTERNAL_ERROR_SIZE;
	return 0;
}

int moved_permanently()
{
	FILE *finternal = fopen("moved_permanently_301.html","r");
	char *s = (char *)malloc(INTERNAL_ERROR_SIZE); 
	rc = fread(s,1, INTERNAL_ERROR_SIZE,finternal);
	if(rc == -1)
	{
		printf("Read from html file failed\n");
		return -1;
	}
	server_response = s;
	response_size = INTERNAL_ERROR_SIZE;
	return 0;
}

int unauthorised()
{
	FILE *finternal = fopen("unauthorised_401.html","r");
	char *s = (char *)malloc(INTERNAL_ERROR_SIZE); 
	rc = fread(s,1, INTERNAL_ERROR_SIZE,finternal);
	if(rc == -1)
	{
		printf("Read from html file failed\n");
		return -1;
	}
	server_response = s;
	response_size = INTERNAL_ERROR_SIZE;
	return 0;
	fclose(finternal);
}


int internal_error()
{
	FILE *finternal = fopen("missing500.html","r");
	char *s = (char *)malloc(INTERNAL_ERROR_SIZE); 
	rc = fread(s,1, INTERNAL_ERROR_SIZE,finternal);
	if(rc == -1)
	{
		printf("Read from html file failed\n");
		return -1;
	}
	server_response = s;
	response_size = INTERNAL_ERROR_SIZE;
	fclose(finternal);
	return 0;
}

int hostless_request()
{
	printf("inside hostless request function\n");	
	FILE *finternal = fopen("hostless_request.html","r");
	char *s = (char *)malloc(INTERNAL_ERROR_SIZE); 
	rc = fread(s,1, INTERNAL_ERROR_SIZE,finternal);
	if(rc == -1)
	{
		printf("Read from html file failed\n");
		return -1;
	}
	server_response = s;
	response_size = INTERNAL_ERROR_SIZE;
	write(client_sockfd, server_response, response_size); 
	fclose(finternal);
	return 0;
}
