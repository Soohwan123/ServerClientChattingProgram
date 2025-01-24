// WebSocket 연결
const socket = new WebSocket('ws://localhost:8080'); // 서버 주소

// DOM 요소
const messagesDiv = document.getElementById('messages');
const chatInput = document.getElementById('chat-input');
const sendButton = document.getElementById('send-button');

// 메시지 전송
sendButton.addEventListener('click', () => {
    const message = chatInput.value.trim();
    if (message) {
        socket.send(message); // 서버로 메시지 전송
        chatInput.value = ''; // 입력창 초기화
    }
});

// WebSocket 메시지 수신
socket.onmessage = (event) => {
    const message = event.data; // 서버에서 받은 메시지
    const messageElement = document.createElement('div');
    messageElement.textContent = message;
    messagesDiv.appendChild(messageElement);
    messagesDiv.scrollTop = messagesDiv.scrollHeight; // 스크롤 아래로
};

// WebSocket 연결 상태 확인
socket.onopen = () => {
    console.log('Connected to the server');
};

socket.onerror = (error) => {
    console.error('WebSocket error:', error);
};

socket.onclose = () => {
    console.log('Disconnected from the server');
};
