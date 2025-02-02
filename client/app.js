// WebSocket 연결
const socket = new WebSocket('ws://localhost:8080');// 서버 주소

// DOM 요소
const messagesDiv = document.getElementById('messages');
const chatInput = document.getElementById('chat-input');
const sendButton = document.getElementById('send-button');

// 메시지 전송
sendButton.addEventListener('click', () => {
    const message = chatInput.value.trim();
    if (message && socket.readyState === WebSocket.OPEN) {  // 연결 상태 체크
        socket.send(message); // 서버로 메시지 전송
        chatInput.value = ''; // 입력창 초기화
    } else {
        console.error("WebSocket is not open. Cannot send message.");
    }
});

document.getElementById('chat-input').addEventListener('keydown', (event) => {
    if (event.key === 'Enter') { // 엔터 키 감지
        const messageInput = document.getElementById('chat-input');
        const message = messageInput.value.trim(); // 공백 제거
        if (message) {
            socket.send(message); // 메시지 전송
            messageInput.value = ''; // 입력 필드 초기화
        }
        event.preventDefault(); // 엔터 키 기본 동작(줄바꿈) 방지
    }
});

socket.onmessage = (event) => {
    console.log("Message received from server:", event.data); // 디버깅용
    const message = event.data; 
    const messageElement = document.createElement('div');
    messageElement.textContent = message;
    messagesDiv.appendChild(messageElement);
    messagesDiv.scrollTop = messagesDiv.scrollHeight;
};

// WebSocket 연결 상태 확인
socket.onopen = () => {
    console.log('Connected to the server');
};

socket.onerror = (error) => {
    console.error('WebSocket error:', error);

    console.log('WebSocket readyState:', socket.readyState);
    console.log('WebSocket URL:', socket.url);
};

socket.onclose = () => {
    console.log('Disconnected from the server');
};
