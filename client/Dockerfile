# NGINX로 정적 파일 서빙
FROM nginx:alpine

# 클라이언트 파일 복사
COPY client/ /usr/share/nginx/html

# NGINX 실행
CMD ["nginx", "-g", "daemon off;"]
