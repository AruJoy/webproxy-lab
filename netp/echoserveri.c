#include "csapp.h"

void echo(int connfd);

int main(int argc, char **argv)    //  argc: 실행하며 전달한 인자갯수 : ./echoserver 12345
                                   //  argv: 실행할때 인자 저장
                                   //   -> 인자 2개
{
     int listenfd, connfd;      //  listen 버퍼fd, 연결버퍼fd 선언
     socklen_t clientlen;       //  client주소 길이
     struct sockaddr_storage clientaddr;     //  client 주소 구조체
     char client_hostname[MAXLINE], client_port[MAXLINE];   //  client 주소와 사용중인 포트 배열 ()
     if (argc != 2){                                        //  실행시, 인자값 검증
          fprintf(stderr, "usage: %s <port>\n", argv[0]);
          exit(0);
     }

     listenfd = Open_listenfd(argv[1]);                     //  두번째 인자 -> port 번호
     while (1){
          clientlen = sizeof(struct sockaddr_storage);
          connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);             //  connfd -> listenfd로 들어온 접속요청을 수락하면 생성
          Getnameinfo((SA *)&clientaddr, clientlen, client_hostname, MAXLINE,   //  host 기기 명 확인
                      client_port, MAXLINE, 0);
          printf("Connected to (%s, %s)\n", client_hostname, client_port);      //  접속 확인 output
          echo(connfd);
          Close(connfd);
     }
     exit(0);
}