
#include "csapp.h"
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
void serve_static(int fd, char *filename, int filesize, char *method);
void get_filetype(char *filename, char *filetype);
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, char **argv)
{
	int listenfd, connfd;
  // hostname: 8kb, port:8kb
  // host최대길이: 63byte(일부: 255자) port: 2byte
	char hostname[MAXLINE], port[MAXLINE];    //  -> 확장 가능성, 최대 길이보다 길면 되기때문에 MAXLINE으로 크기 설정
	socklen_t clientlen;                      //  c언어 socket 표준 라이브러리가 정의한 데이터 타입
	struct sockaddr_storage clientaddr;       //  나중에 주소 구조체를 저장하기 위해 구조체 크기만큼 미리 할당해둔 매모리
// 구조 ->
//  typedef struct sockaddr_storage {
//   short   ss_family;                     //  주소 타입
//   char    __ss_pad1[_SS_PAD1SIZE];       //  ipv4 기준 주소 크기
//   __int64 __ss_align;                    //  ipv4 기준 0패딩
//   char    __ss_pad2[_SS_PAD2SIZE];       //  port 길이
// } SOCKADDR_STORAGE, *PSOCKADDR_STORAGE;

	if (argc != 2) {                                  //  횐경변수 검증
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(1);
	}

	listenfd = Open_listenfd(argv[1]);        // 환경변수 port를 통해 listener 소켓 설정
	while (1) {
		clientlen = sizeof(clientaddr);         // client 주소 구조채 크기
		fprintf(stdin, "clientlen: %d\n", clientlen);
    // [1st hand shake]listenfd의 요청큐(client - SYN)에서 요청 불러온뒤
    // [2nd handshake]Accept내부애세서 연결 소켓 생성후 연결 응답(SYN,ACK)
		connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
    // 이름 불러온 뒤 로그 생성
		Getnameinfo((SA *)&clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
		printf("Accepted connection from (%s, %s)\n", hostname, port);
    // connfd번호를 doit에 넘겨 주며 호출
		doit(connfd);
    // echo(connfd);  // 숙제: 11.6
		Close(connfd);
	}
}

void echo(int connfd) {
  size_t n;
  char buf[MAXLINE];
  rio_t rio;

  Rio_readinitb(&rio, connfd);
  while ((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0) {
    if (strcmp(buf, "\r\n") == 0)
      break;
    Rio_writen(connfd, buf, n);
  }
}
void doit(int fd)
{
	int is_static;
	struct stat sbuf;
  // buf: header 정보 버퍼, method/uri/version: 헤더의 첫줄 정보
	char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
	char filename[MAXLINE], cgiargs[MAXLINE];
  // 빈 rio 선언
	rio_t rio;
  // 빈 rio 와 connfd 연결
	Rio_readinitb(&rio, fd);
  // rio를 통해 buf에 connfd 에 저장된 요헝헤더의 맨 윗줄을 불러옴
  // [3rd handshake] 함수가 진행되는동안 요청헤더에 client가 요청 데이터 보냄
	Rio_readlineb(&rio, buf, MAXLINE);
  // 요청헤더 출력
	printf("Request headers:\n");
	printf("%s", buf);

  // buf에 저장한 conndfd의 요청헤더 첫줄을 method, uri, versioin세개로 나눔
	sscanf(buf, "%s %s %s", method, uri, version);

  // 이하: 요청 헤더 분석에 따라 분기처리

  //서버 스팩상 지원하지 않는 http요청 메소드 예외처리
  // -> 메소드: GET이 아닌경우, 에러 메세지 반환
	if (!(strcasecmp(method, "GET") == 0 || strcasecmp(method, "HEAD") == 0)) {
		clienterror(fd, method, "501", "Not implemented", "TIny does not implement this method");
		return;
	}
  
  // 헤더 첫줄을 제외한 나머지 헤더 출력
	read_requesthdrs(&rio);
  
  // uri 를 분석하여 정적 콘텐츠 / 동적 콘텐츠 파싱
	is_static = parse_uri(uri, filename, cgiargs);

  // stat(pathname, statbuf)
  // pathname의 i노드에서 파일의 메타 데이터를 읽어와 statbuf(stat 구조체에 저장)
  // struct stat {
  //   dev_t     st_dev;         /* ID of device containing file */
  //   ino_t     st_ino;         /* Inode number */
  //   mode_t    st_mode;        /* File type and mode */
  //   nlink_t   st_nlink;       /* Number of hard links */
  //   uid_t     st_uid;         /* User ID of owner */
  //   gid_t     st_gid;         /* Group ID of owner */
  //   dev_t     st_rdev;        /* Device ID (if special file) */
  //   off_t     st_size;        /* Total size, in bytes */
  //   blksize_t st_blksize;     /* Block size for filesystem I/O */
  //   blkcnt_t  st_blocks;      /* Number of 512B blocks allocated */
  //   struct timespec st_atim;  /* Time of last access */
  //   struct timespec st_mtim;  /* Time of last modification */
  //   struct timespec st_ctim;  /* Time of last status change */
  // };
	if (stat(filename, &sbuf) < 0) { // 읽어들인 i노드 정보가 없을 시, 소스 탐색 에러 반환
		clienterror(fd, filename, "404", "Not found", "Tiny couldn't find this file");
		return;
	}

  // 정적 컨텐츠 권한 처리
	if (is_static) {
    // S_ISREG() -> 파일이 일반파일 일 경우, True
    // S_IRUSR -> 파일 소유자 읽기/쓰기 권한 확인 비트flag
    // S_IRUSR & sbuf.st_mode -> 파일을 읽을 권한이 있다면, True
    //  !(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode) -> 일반 파일이 아니거나 파일 읽기 권한이 없을 경우 예외처리
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IRUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden", "Tiny couldn't read the file");
			return;
		}
    // 정적 파일 응답 -> 설명: 함수 참고
		serve_static(fd, filename, sbuf.st_size, method);
	}
  // 정적 컨텐츠가 아닐경우. -> 동적 컨텐츠인 경우.
	else {
    // 일반 파일이 아니거나 파일 실행 권한이 없을 경우 예외처리
		if (!(S_ISREG(sbuf.st_mode)) || !(S_IXUSR & sbuf.st_mode)) {
			clienterror(fd, filename, "403", "Forbidden", "Tiny could't run the CGI program");
			return;
		}
    // 동적 파일 응답 -> 설명: 함수 참고
		serve_dynamic(fd, filename, cgiargs, method);
	}
}
// 오류 메세지 생성 함수
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) {
	char buf[MAXLINE], body[MAXBUF];
	sprintf(body, "<html><title>Tiny Error</title>");
	sprintf(body, "%s<body bgcolor=""ffffff"">\r\n", body);
	sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
	sprintf(body, "%s<p>%s: %s\r\n", body, longmsg, cause);
	sprintf(body, "%s<hr><em>The Tiny Web server</em>\r\n", body);
  	sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-type: text/html\r\n");
	Rio_writen(fd, buf, strlen(buf));
	sprintf(buf, "Content-length: %d\r\n\r\n", (int)strlen(body));
	Rio_writen(fd, buf, strlen(buf));
	Rio_writen(fd, body, strlen(body));
}

// 요청에 필요한 해더파트 이외의 헤더 출력 함수
void read_requesthdrs(rio_t *rp) {
	char buf[MAXLINE];
  // line 단위로 읽어온 뒤, 단일 "\r\n"(헤더 마지막) 나올 때 까지 출력
	Rio_readlineb(rp, buf, MAXLINE);
	while(strcmp(buf, "\r\n")) {
		Rio_readlineb(rp, buf, MAXLINE);
		printf("%s", buf);
	}
	return;
}

// uri 파싱 함수
int parse_uri(char *uri, char *filename, char *cgiargs) {
	char *ptr;
  // url에 cgi-bin 이 포함되어있지 않다면, 정적 컨텐츠
	if (!strstr(uri, "cgi-bin")) {
    // 정적 컨텐츠이기 때문에, cgiargs 는 비워둠
		strcpy(cgiargs, "");
    // filename을 초기화하고 .을 복사
		strcpy(filename, ".");
    // filename 뒤에 uri를 이어붙임
		strcat(filename, uri);

    // 마지막 글자가 '/' 일경우, home.html
		if (uri[strlen(uri) - 1] == '/') {
			strcat(filename, "home.html");
		}
		return 1;
	}

	else {
    // 동적컨텐츠일 경우, uri 문자열에서 '?'을 찾음
		ptr = index(uri, '?');
		if (ptr) {
      //? 이하 cgi 인자 복사
			strcpy(cgiargs, ptr + 1);
			*ptr = '\0';
		} else {
			strcpy(cgiargs, "");
		}
    //현재 디랙토리애서 uri 명의 파일을 찯아서 Return
		strcpy(filename, ".");
		strcat(filename, uri);
		return 0;
	}
}

void serve_static(int fd, char *filename, int filesize, char *method){
	int srcfd;
	char *srcp, filetype[MAXLINE], buf[MAXBUF];
	get_filetype(filename, filetype);
	sprintf(buf, "HTTP/1.0 200 OK\r\n");
	sprintf(buf, "%sServer: Tiny Web Server\r\n", buf);
	sprintf(buf, "%sConnection: close\r\n", buf);
	sprintf(buf, "%sContent-length: %d\r\n", buf, filesize);
	sprintf(buf, "%sContent-type: %s\r\n\r\n", buf, filetype);
	Rio_writen(fd, buf, strlen(buf));
	printf("Response headers:\n");
	printf("%s", buf);

	srcfd = Open(filename, O_RDONLY, 0);
	// srcp = Mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);  // 숙제 11.9를 위해 주석처리
	srcp = (char*)Malloc(filesize);                                   // 11.9 추가
	Rio_readn(srcfd, srcp, filesize);
	Close(srcfd);
	Rio_writen(fd, srcp, filesize);
	// Munmap(srcp, filesize);                                      // 숙제 11.9를 위해 주석처리
  free(srcp);                                                       // 11.9 추가
}
void get_filetype(char *filename, char *filetype) {
	if (strstr(filename, ".html")) {
		strcpy(filetype, "text/html");
	} else if (strstr(filename, ".gif")) {
		strcpy(filetype, "image/gif");
	} else if (strstr(filename, ".png")) {
		strcpy(filetype, "image/png");
	} else if (strstr(filename, ".jpg")) {
		strcpy(filetype, "image/jpeg");
	} else if (strstr(filename, ".mpeg")){          				// 숙제: 11.7
    strcpy(filetype, "video/mpeg");
  } else {
    strcpy(filetype, "text/plain");
	}
}
// 동적 컨텐츠
void serve_dynamic(int fd, char *filename, char *cgiargs, char *method){
  char buf[MAXLINE], *emptylist[] = { NULL };
  /* Return first part of HTTP response */
  sprintf(buf, "HTTP/1.0 200 OK\r\n");
  Rio_writen(fd, buf, strlen(buf));
  sprintf(buf, "Server: Tiny Web Server\r\n");
  Rio_writen(fd, buf, strlen(buf));

  if (Fork() == 0) { /* Child */
    /* Real server would set all CGI vars here */
    setenv("QUERY_STRING", cgiargs, 1);  // 
    // method를 cgi-bin/adder.c에 넘겨 주기 위해 환경변수 set -> 11.11 문제
    setenv("REQUEST_METHOD", method, 1);
    // 클라이언트의 표준 출력을 CGI 프로그램의 표준 출력과 연결한다.
    // 이제 CGI 프로그램에서 printf하면 클라이언트에서 출력됨
    Dup2(fd, STDOUT_FILENO); /* Redirect stdout to client */
    Execve(filename, emptylist, environ); /* Run CGI program */
  }
  Wait(NULL); /* Parent waits for and reaps child */
}