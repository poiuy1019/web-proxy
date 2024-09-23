/* $begin tinymain */
/*
 * tiny.c - A simple, iterative HTTP/1.0 Web server that uses the
 *     GET method to serve static and dynamic content.
 *
 * Updated 11/2019 droh
 *   - Fixed sprintf() aliasing issue in serve_static(), and clienterror().
 */
#include "csapp.h"

void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv) {
  int listenfd,/*server listening socket*/ connfd;/*client connection file descriptor*/
  char hostname[MAXLINE], port[MAXLINE];
  socklen_t clientlen; //clientaddr의 size
  struct sockaddr_storage clientaddr; //client의 주소 정보 structure

  /* Check command line args */
  if (argc != 2) { //argc:command-line argument 가 2가 아니면 error
    fprintf(stderr, "usage: %s <port>\n", argv[0]);
    exit(1);}

  listenfd = Open_listenfd(argv[1]); //command-line에서 받은 port 번호의 listening socket을 open

  while (1) {
    clientlen = sizeof(clientaddr);
    connfd = Accept(listenfd, (SA *) &clientaddr, &clientlen);  //client가 연결되기를 기다렸다가 연결되면 connfd를 return
    Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE,0);

    //client IP & port를 사람이 알아 볼 수 있는 hostname & port 형태로 변경
    printf("Accepted connection from (%s, %s)\n", hostname, port);
    doit(connfd);   //connfd를 doit하여 HTTP request handle
    Close(connfd);  //connfd를 close하여 server가 client와의 connection
  }
}
/*doit - client request를 처리해줌*/
void doit(int fd) 
{
    //1 for static content(HTML,image file), 0 for dynamic contenct(CGI scripts)
    int is_static;
    //file의 size, permission등의 정보를 가지고 있는 structure
    struct stat sbuf;
    //request line 정보 저장 buffer
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], cgiargs[MAXLINE];
    //client와 connection handling 위한 I/O structure
    rio_t rio;

    /* Read request line and headers */
    Rio_readinitb(&rio, fd); 
    Rio_readlineb(&rio, buf, MAXLINE); 
    printf("Request headers:\n");
    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version); //buf를 읽어서 parsing 한다
    
    if (strcasecmp(method, "GET") && strcasecmp(method, "HEAD")) // 과제 11.11를 위해 수정한 코드
    {                                                            
        clienterror(fd, method, "501", "Not Implemented", "Tiny does not implement this method");
        return;
    }                                                    
    read_requesthdrs(&rio);//HTTP header read

    /* Parse URI from GET request */
    is_static = parse_uri(uri, filename, cgiargs);//URI를 바탕으로 static or dynamic content request인지 판단
                                                  //static content일 경우 is_static에 저장
    if (stat(filename, &sbuf) < 0) {              //stat이 file이 존재하는지 check, 없을 시 error
	clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
	return;
    }                                                    

    if (is_static) { /* Serve static content */          
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) { //static content가 regular file에 readable한지 check
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't read the file"); //check 통과 못할 시 error
	    return;
	}
	serve_static(fd, filename, sbuf.st_size, method);//check 통과 시 serve_static함수로 client에 file 제공
    }
    else { /* Serve dynamic content */
	if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) { //dynamic content가 executable하고 regular 한지 check
	    clienterror(fd, filename, "403", "Forbidden",
			"Tiny couldn't run the CGI program");
	    return;
	}
	serve_dynamic(fd, filename, cgiargs, method);//check 통과 시 serve_dynamic함수로 CGI를 통해 client에 전달
    }
}
/*error 처리 함수*/
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
//fd:file descriptor, cause:error사유, errnum:HTTP error code, shortmsg,longmsg: error 설명문구,  
{
    char buf[MAXLINE], body[MAXBUF]; //body: HTML error msg를 위한 buffer

    /* Build the HTTP response body */
    sprintf(body, "<html><title>Tiny Error</title>"); //html title: Tiny error
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body); //html background 하얀색으로 설정
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg); //간략한 error 문구
    sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause); //자세한 error 문구
    sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);//html에 footer 추가

    /* Print the HTTP response */
    //HTTP response header
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg); // 예시: HTTP/1.0 404 Not Found 
    Rio_writen(fd, buf, strlen(buf)); //fd를 통해 client에게 전송
    sprintf(buf, "Content-type: text/html\r\n"); //content type을 명시
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body)); //content length 명시, 추가 \r\n은 header끝을 나타냄
    Rio_writen(fd, buf, strlen(buf));
    //HTTP response body
    Rio_writen(fd, body, strlen(body));
}
/*request header 읽어주는 함수*/
void read_requesthdrs(rio_t *rp) 
{
    char buf[MAXLINE]; 
    Rio_readlineb(rp, buf, MAXLINE); //각 라인을 읽어서 변수로 저장
    printf("%s", buf);               
    while(strcmp(buf, "\r\n")) {     //빈 라인이 나올때까지 while loop
	Rio_readlineb(rp, buf, MAXLINE); //빈 라인 전까지 라인을 read해서 buf에 저장
	printf("%s", buf);               
    }
    return;
}

/*parse_uri - parse URI into filename and CGI args return 0 if dynamic content, 1 if static*/
int parse_uri(char *uri, char *filename, char *cgiargs) 
{
    char *ptr;

    if (!strstr(uri, "cgi-bin")) {  /* Static content */ //cgi-bin이 없으면, 즉 static content이면
	strcpy(cgiargs, "");                             //cgiargs를 clear하고
	strcpy(filename, ".");                           //"."->current directory에서 filename initialize
	strcat(filename, uri);                           //filename과 URI 합쳐서 full path 완성
	if (uri[strlen(uri)-1] == '/')                   //URI가 '/'로 끝나면 default file home.html을 append
	    strcat(filename, "home.html");               
	return 1;                                        //1을 return하여 static content임을 명시
    }
    else {  /* Dynamic content */                    //else 즉, dynamic content이면
	ptr = index(uri, '?');                           //URI에서 '?'를 찾는다, '?'는 cgi program path를 구분지어주는 기호
	if (ptr) {
	    strcpy(cgiargs, ptr+1);                      //'?'다음 URI 부분을 cgiargs에 복사
	    *ptr = '\0';                                 //'?'를 '\0'로 대체하여 URI를 cgi program path로 truncate
	}
	else 
	    strcpy(cgiargs, "");                         //cgiargs를 비워주고
	strcpy(filename, ".");                           //current directory에 filename initialize
	strcat(filename, uri);                           //filename에 URI append하여 CGI script로 full path 형성
	return 0;                                        //0을 return 하여 dynamic content임을 표현
    }
}
/*serve_static memory map version*/
/*
void serve_static(int fd, char *filename, int filesize) 
{
    int srcfd;                                  //file descriptor
    char *srcp, filetype[MAXLINE], buf[MAXBUF]; 

    get_filetype(filename, filetype);       //file의 MIME type filetype buffer에 저장(.html, .jpg, .png 등)
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //HTTP response header를 만들고
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); 
    Rio_writen(fd, buf, strlen(buf));       //client에 HTTP response header를 보낸다
    printf("Response headers:\n");          
    printf("%s", buf);


    srcfd = Open(filename, O_RDONLY, 0);    //filename을 read only mode로 열어 file descriptor를 return.
    srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);//file content를 memory에 mapping
    
    Close(srcfd);                           //file descriptor를 close
    Rio_writen(fd, srcp, filesize);         //srcp를 client에 write
    Munmap(srcp, filesize);                 //client에 content가 전달됐기에 memory map을 unmapping한다
}
*/

/*serve_static malloc version*/
void serve_static(int fd, char *filename, int filesize, char *method) 
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];

    get_filetype(filename, filetype);       //file의 MIME type filetype buffer에 저장(.html, .jpg, .png 등)
    sprintf(buf, "HTTP/1.0 200 OK\r\n");    //HTTP response header를 만들고
    sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
    sprintf(buf, "%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype); 
    Rio_writen(fd, buf, strlen(buf));       //client에 HTTP response header를 보낸다
    printf("Response headers:\n");          
    printf("%s", buf);

    if (!strcasecmp(method, "HEAD")) // 같으면(0) 바로 return (HEAD가 맞으면)
    return;                            

    srcfd = Open(filename, O_RDONLY, 0);    //filename을 read only mode로 열어 file descriptor를 return.
    srcp = (char *)Malloc(filesize);
    Rio_readn(srcfd, srcp, filesize);
    Close(srcfd);
    Rio_writen(fd, srcp, filesize);
    free(srcp);
}

//MIME type을 읽고 값을 *filetype에 저장
void get_filetype(char *filename, char *filetype)
{
    if (strstr(filename, ".html"))//type이 html인지 판별 
	strcpy(filetype, "text/html");
    else if (strstr(filename, ".gif"))//type이 gif인지 판별
	strcpy(filetype, "image/gif");
    else if (strstr(filename, ".png"))//type이 png인지 판별
	strcpy(filetype, "image/png");
    else if (strstr(filename, ".jpg"))//type이 jpg인지 판별
	strcpy(filetype, "image/jpeg");
    else if (strstr(filename, ".mp4"))//type이 mp4인지 판별
	strcpy(filetype, "video/mp4");
    else
	strcpy(filetype, "text/plain");//만약 무엇에도 해당되지 않으면 default MIME인 text/plain으로 지정
}  

/*Dynamic content의 CGI program 실행*/
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method) 
{
    char buf[MAXLINE], *emptylist[] = { NULL };

    /* Return first part of HTTP response */
    sprintf(buf, "HTTP/1.0 200 OK\r\n"); //HTTP status line
    Rio_writen(fd, buf, strlen(buf)); //client에 전송
    sprintf(buf, "Server: Tiny Web Server\r\n"); //server를 특정하는 header 추가
    Rio_writen(fd, buf, strlen(buf)); //client에 server header를 전송
    
    if (!strcasecmp(method, "HEAD")) // 같으면 0(false) 들어가고 끝냄(HEAD가 맞으면)
    return;                        // void 타입이라 바로 리턴해도 됨(끝내라)

    if (Fork() == 0) { /* Child */ //CGI program 실행을 위한 child process
	/* Real server would set all CGI vars here */
	setenv("QUERY_STRING", cgiargs, 1); //CGI program이 읽을 수 있는 QUERY_STRING 설정
	Dup2(fd, STDOUT_FILENO);         /* Redirect stdout to client */ //CGI output을 바로 client로 전달
	Execve(filename, emptylist, environ); /* Run CGI program */ //Child process를 CGI program으로 대체
    }
    Wait(NULL); /* Parent waits for and reaps child */ //Child process가 완료된 후 child를 reap후 parent가 진행
}
