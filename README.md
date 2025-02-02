# 📦 ChattingApp - ServerClientChattingProgram

## 📋 프로젝트 설명

이 어플리케이션은 C로 작성된 서버와 JavaScript로 작성된 클라이언트로 구성된 **오픈 채팅 애플리케이션**입니다. WebSocket을 사용하여 실시간 채팅 기능을 제공합니다.
이 어플리케이션은 아직 완전배포 전인 상태여서 로컬에서 밖에 테스트 할 수 없습니다.

- **서버:** C로 작성된 고성능 멀티스레드 서버
- **클라이언트:** HTML, CSS, JavaScript 기반의 웹 인터페이스
- **컨테이너화:** Docker 및 Docker Compose를 사용하여 쉽게 배포 가능

---

## 🚀 프로젝트 구조

```
ChattingApp/ServerClientChattingProgram
├── Dockerfile
├── docker-compose.yml
├── README.md
├── client
│   ├── app.js
│   ├── index.html
│   ├── style.css
│   └── package-lock.json
└── server
    ├── server
    ├── server.c
    ├── server.h
    ├── server.crt
    ├── server.key
    ├── utils.c
    ├── websocket.c
    └── websocket.h
```

---

## ⚙️ 요구 사항

- **Docker** (20.x 이상)
- **Docker Compose** (1.29 이상)

Docker가 설치되어 있지 않다면:
```bash
sudo apt update
sudo apt install docker.io -y
sudo systemctl start docker
sudo systemctl enable docker
```

Docker Compose가 필요하다면:
```bash
sudo apt install docker-compose -y
```

---

## 🐳 Docker로 실행하기

### 📦 1. 컨테이너 빌드

```bash
docker-compose build
```

### 🚀 2. 애플리케이션 실행

```bash
docker-compose up -d
```

- `-d` 옵션은 **백그라운드 모드**로 실행합니다.

### 🛑 3. 애플리케이션 중지

```bash
docker-compose down
```

---

## 🔍 서버 & 클라이언트 로그 확인

### 📊 모든 로그 보기
```bash
docker-compose logs -f
```

### 🗂️ 개별 서비스 로그 보기
```bash
docker-compose logs -f server
```
```bash
docker-compose logs -f client
```

---

## 🌐 클라이언트 접속 방법

1. **서버가 실행 중인지 확인**
2. 웹 브라우저 열기
3. 주소창에 입력: [http://localhost:3000](http://localhost:3000)
4. 채팅 시작하기 🚀

---

## 🗑️ 캐시 초기화 후 재빌드 (문제 해결용)

```bash
docker-compose down --rmi all --volumes --remove-orphans
docker-compose build --no-cache
docker-compose up -d
```

---

## 📝 주요 기술 스택

- **서버:** C, Epoll, pthread, OpenSSL
- **클라이언트:** HTML, CSS, JavaScript (WebSocket API)
- **배포:** Docker, Docker Compose

---

## ❗ 문제 해결

1. **권한 오류 발생 시:**
   ```bash
   sudo usermod -aG docker $USER
   newgrp docker
   ```

2. **포트 충돌 발생 시:**
   ```bash
   sudo fuser -k 8080/tcp
   ```

3. **로그가 출력되지 않을 때:**
   - `server.c`에 `fflush(stdout)` 추가 확인
   - `docker-compose.yml`에 `tty: true` 설정 확인

---

## 🙌 기여 방법

1. 이 프로젝트를 포크합니다
2. 새로운 브랜치를 만듭니다 (`git checkout -b feature-브랜치이름`)
3. 변경 사항을 커밋합니다 (`git commit -m 'Add new feature'`)
4. 브랜치를 푸시합니다 (`git push origin feature-브랜치이름`)
5. Pull Request를 생성합니다

---

## 📧 연락처

- **개발자:** Soohwan Kim
- **문의:** rlatnghks789456@gmail.com

---

## 🚀 라이선스

이 프로젝트는 MIT 라이선스 하에 배포됩니다.


## 차후 업데이트 내역
iouring 아키텍쳐 도입 (현재 140us 응답속도를 30us 까지 낮출 예정)
모바일앱을 Qt 로 제작예정
완전배포


## 업데이트
v 2.0 
TCP Nagle 알고리즘 비활성화 

v 3.0 
멀티스레드 스레드풀 아키텍쳐 적용

v 3.1 
서버 클라이언트 연결 오류 수정

