import select
import socket

SERVER = "127.0.0.1"
PORT = 8080

with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as remote:
    remote.connect((SERVER, PORT))
    while True:
        data = remote.recv(4096)
        client = None
        data = data.decode()
        if "CONNECT" in data:
            client = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            data = data.split(" ")[1].split(":")
            client.connect((data[0], int(data[1])))

            while True:
                # wait until client or remote is available for read
                r, w, e = select.select([client, remote], [], [])

                if client in r:
                    data = client.recv(4096)
                    if remote.send(data) <= 0:
                        break

                if remote in r:
                    data = remote.recv(4096)
                    if data.startswith(b"CLOSE"):
                        break
                    if client.send(data) <= 0:
                        break