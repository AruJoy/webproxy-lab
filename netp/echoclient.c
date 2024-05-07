#include "csapp.h"

int main(int argc, char **argv){                                 //  ./echoclient 172.17.0.2 12345
                                                                 //  인자 3개, 두번째인자: ip, 세번째인자: port
     int clientfd;                                               //  clientfd  미리 선언
     char *host, *port, buf[MAXLINE];                            //  buf[MAXLINE]: client에서 출력용으로 사용하는 데 사용하는 버퍼
     rio_t rio;
     //  rio 구조채 선언
     //  구성요소
     //  rio_fd : 네트워크 입력 버퍼 (버퍼입력을 받을 fd)
     //  rio_cnt: 아직 읽지 않은 버퍼의 크기 
     //  rio_bufptr: 읽지않은 데이터 시작 주소
     //  rio_buf[RIO_BUFSIZE]: 버퍼

     if (argc != 3){                                             // 실행인자 검증
          fprintf(stderr, "usage: %s <host> <port>\n", argv[0]);
          exit(0);
     }
     host = argv[1];                                             //  두번째인자: ip
     port = argv[2];                                             //  세번째인자: port

     clientfd = Open_clientfd(host, port);                       // 두번째 인자와 세번째 인지를 사용하여 접속시도
     Rio_readinitb(&rio, clientfd);                              // rio패키지 초기화

     while (Fgets(buf, MAXLINE, stdin) != NULL){                 // 콘솔 입력 -> stdin -> buf 저장
          Rio_writen(clientfd, buf, strlen(buf));                // buf -> connfd를 통해 전송
          Rio_readlineb(&rio, buf, MAXLINE);                     // rio -> buf 저장
          Fputs(buf, stdout);                                    // buf 내용 -> stdout
     }
     Close(clientfd);
     exit(0);
}