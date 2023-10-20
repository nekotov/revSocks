import socket
import select

USE_AUTH = False
local_port = 1080
remote_port = 8080

SOCKS_VERSION = 5
hijacked_conn = None


class Proxy:
    def __init__(self):
        self.username = "username"
        self.password = "password"

    def handle_client(self, connection):
        version, nmethods = connection.recv(2)

        methods = self.get_available_methods(nmethods, connection)

        if USE_AUTH and 2 not in set(methods):
            # close connection
            connection.close()
            return

        connection.sendall(bytes([SOCKS_VERSION, 2]))

        if USE_AUTH and not self.verify_credentials(connection):
            return

        version, cmd, _, address_type = connection.recv(4)

        if address_type == 1:
            address = socket.inet_ntoa(connection.recv(4))
        elif address_type == 3: # TODO forward to remote
            domain_length = connection.recv(1)[0]
            address = connection.recv(domain_length)
            address = socket.gethostbyname(address)

        port = int.from_bytes(connection.recv(2), 'big', signed=False)

        try:
            if cmd == 1:
                hijacked_conn.sendall(bytes(f"CONNECT {address}:{port}",'ascii'))
                print("* Connected to {} {}".format(address, port))
            else:
                connection.close()

            addr = int.from_bytes(socket.inet_aton("127.0.0.1"), 'big', signed=False) # TODO:
            port = 1234 # fix

            reply = b''.join([
                SOCKS_VERSION.to_bytes(1, 'big'),
                int(0).to_bytes(1, 'big'),
                int(0).to_bytes(1, 'big'),
                int(1).to_bytes(1, 'big'),
                addr.to_bytes(4, 'big'),
                port.to_bytes(2, 'big')
            ])
        except Exception as e:
            # return connection refused error
            reply = self.generate_failed_reply(address_type, 5)

        connection.sendall(reply)

        # establish data exchange
        if reply[1] == 0 and cmd == 1:
            self.exchange_loop(connection, hijacked_conn)
        hijacked_conn.sendall(b"CLOSE")
        connection.close()


    def exchange_loop(self, client, remote):
        while True:
            # wait until client or remote is available for read
            r, w, e = select.select([client, remote], [], [])

            if client in r:
                data = client.recv(4096)
                if remote.send(data) <= 0:
                    break

            if remote in r:
                data = remote.recv(4096)
                if client.send(data) <= 0:
                    break


    def generate_failed_reply(self, address_type, error_number):
        return b''.join([
            SOCKS_VERSION.to_bytes(1, 'big'),
            error_number.to_bytes(1, 'big'),
            int(0).to_bytes(1, 'big'),
            address_type.to_bytes(1, 'big'),
            int(0).to_bytes(4, 'big'),
            int(0).to_bytes(4, 'big')
        ])


    def verify_credentials(self, connection):
        version = ord(connection.recv(1)) # should be 1

        username_len = ord(connection.recv(1))
        username = connection.recv(username_len).decode('utf-8')

        password_len = ord(connection.recv(1))
        password = connection.recv(password_len).decode('utf-8')

        if username == self.username and password == self.password:
            # success, status = 0
            response = bytes([version, 0])
            connection.sendall(response)
            return True

        # failure, status != 0
        response = bytes([version, 0xFF])
        connection.sendall(response)
        connection.close()
        return False


    def get_available_methods(self, nmethods, connection):
        methods = []
        for i in range(nmethods):
            methods.append(ord(connection.recv(1)))
        return methods

    def run(self, host, port):
        s = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        s.bind((host, port))
        s.listen()

        print("* Socks5 proxy server is running on {}:{}".format(host, port))

        while True:
            conn, addr = s.accept()
            print("* new connection from {}".format(addr))
            self.handle_client(conn)


if __name__ == "__main__":
    proxy = Proxy()
    hijacked = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
    hijacked.bind(("0.0.0.0", remote_port))
    hijacked.listen()
    hijacked_conn, addr = hijacked.accept()
    proxy.run("0.0.0.0", local_port)
