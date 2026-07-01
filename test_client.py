import socket
import struct
import sys

MAGIC_BYTE = 0x32
OP_GET = 0x01
OP_SET = 0x02
OP_DEL = 0x03

def send_packet(ip, port, opcode, key, value=""):
    key_bytes = key.encode('utf-8')
    val_bytes = value.encode('utf-8') if isinstance(value, str) else value
    
    key_len = len(key_bytes)
    val_len = len(val_bytes)
    
    # Pack the header: 
    # ! = Network byte order (Big-Endian)
    # B = uint8_t (Magic)
    # B = uint8_t (Opcode)
    # H = uint16_t (Key Len)
    # I = uint32_t (Val Len)
    header_format = "!BBHI"
    header = struct.pack(header_format, MAGIC_BYTE, opcode, key_len, val_len)
    
    packet = header + key_bytes + val_bytes
    
    print(f"Connecting to {ip}:{port}...")
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
            s.connect((ip, port))
            print(f"Sending Command: Opcode={opcode}, Key='{key}', ValLen={val_len}")
            print(f"Raw binary packet size: {len(packet)} bytes")
            
            s.sendall(packet)
            
            response = s.recv(1024)
            print(f"Server Response: {response.decode('utf-8', errors='ignore').strip()}")
            
    except ConnectionRefusedError:
        print("Error: Could not connect to the engine server. Is it running?")
        sys.exit(1)

if __name__ == "__main__":
    SERVER_IP = "127.0.0.1"
    SERVER_PORT = 8080
    
    print("--- Testing Storage Engine Wire Protocol ---")
    
    print("\n[Test 1] Sending valid SET command...")
    send_packet(SERVER_IP, SERVER_PORT, OP_SET, "user_id", "10023")
    
    print("\n[Test 2] Sending valid GET command...")
    send_packet(SERVER_IP, SERVER_PORT, OP_GET, "user_id")
