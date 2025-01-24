# ServerClientChattingProgram
이 프로그램은 다수의 클라이언트들이 서버에 접속을해서 채팅을 보내거나 파일을 업로드/다운로드를 할 수 있는 대규모 서버구조를 모방한 채팅 프로그램입니다.


#로컬로 테스트 하는법

## 웹서버 설치
### sudo apt install -y nginx
### sudo cp -r /root/ServerClientChattingProgram/client/* /var/www/html/




## 서버 실행방법

docker-compose up --build < 실행
docker-compose down --volumes < 현재 켜져있는 컨테이너 중지



## 접속법 
### 터미널에 hostname -I
### 나오는 IP 주소를 치면 나옵니다.
