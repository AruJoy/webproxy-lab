#include "csapp.h"

void echo(int connfd){
    size_t n;
    char buf[MAXLINE]; // echo함수 내부에서 대이터 보관용으로 사용하는 버퍼
    rio_t rio;
    //  rio 구조채 선언
    //  구성요소
    //  rio_fd : 네트워크 입력 버퍼 (버퍼입력을 받을 fd)
    //  rio_cnt: 아직 읽지 않은 버퍼의 크기 
    //  rio_bufptr: 읽지않은 데이터 시작 주소
    //  rio_buf[RIO_BUFSIZE]: 버퍼

    Rio_readinitb(&rio, connfd);
    // 내부동작
    // rp->rio_fd = fd;                 connfd의 정보를 불러옴
    // rp->rio_cnt = 0;                 초기 내부버퍼 데이터: 0
    // rp->rio_bufptr = rp->rio_buf;    버퍼 데이터 시작점: 버퍼 시작점

    while((n = Rio_readlineb(&rio, buf, MAXLINE)) != 0){        // echo 버퍼에 rio버퍼 내용 복사.
        printf("server received %d bytes\n", (int)n);
        Rio_writen(connfd, buf, n);                             // echo 버퍼에서 내용을 conn소켓으로 전송
    }
}
