# Step 1: Base image
FROM ubuntu:latest

# Step 2: Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    openssl \
    libssl-dev \
    liburing-dev \
    curl \
    net-tools \
    && apt-get clean

# Step 3: Set working directory
WORKDIR /app

# Step 4: Copy application files
COPY server/ /app/server
COPY client/ /app/client

# Step 5: Build the server
WORKDIR /app/server
RUN gcc -o server server.c utils.c websocket.c -pthread -lssl -lcrypto -luring

# Step 6: Expose the server port
EXPOSE 8080

# Step 7: Run the server
CMD ["./server"]

