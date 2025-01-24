# Step 1: Base image
FROM ubuntu:latest

# Step 2: Install dependencies
RUN apt-get update && apt-get install -y \
    gcc \
    make \
    openssl \
    libssl-dev \
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
RUN gcc -o server server.c -pthread -lssl -lcrypto

# Step 6: Expose ports
EXPOSE 8080

# Step 7: Run the server
CMD ["./server"]
