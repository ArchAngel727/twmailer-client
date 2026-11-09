#include <algorithm>
#include <arpa/inet.h>
#include <cctype>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <getopt.h>
#include <iostream>
#include <netinet/in.h>
#include <print>
#include <sstream>
#include <string>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <vector>

volatile sig_atomic_t is_running = 1;

int send_str(int fd, const std::string &str) {
  std::string out = str;

  // NOTE: Spec doc requires '\n' as line terminator
  if (str.empty() || str.back() != '\n') {
    out.push_back('\n');
  }

  const char *p = out.data();
  size_t left = out.size();

  // NOTE: Loop because sometimes send() does not send everything
  while (left > 0) {
    ssize_t n = send(fd, p, left, MSG_NOSIGNAL);

    if (n == -1) {
      std::perror("Failed to send str to server");
      return EXIT_FAILURE;
    }

    p += n;
    left -= static_cast<size_t>(n);
  }

  return EXIT_SUCCESS;
}

void send_izuner(int client_socket_fd) {
  int bytes_sent = 0;
  std::ifstream file("izuner");
  std::string line;

  if (!file.is_open()) {
    std::println(std::cerr, "Failed to open file `izuner`");
    exit(EXIT_FAILURE);
  }

  while (std::getline(file, line)) {
    line.push_back('\n');
    send(client_socket_fd, line.c_str(), line.length(), 0);
    bytes_sent += line.length();
  }

  std::println("Bytes sent: {}", bytes_sent);

  file.close();
}

int handle_send_request(int fd) {
  std::string msg, sender, receiver, subject, message;

  std::print("Enter sender: ");
  getline(std::cin, sender);

  if (sender.empty()) {
    std::println(std::cerr, "Sender cannot be empty");
    return EXIT_FAILURE;
  }

  if (sender.length() > 8) {
    std::println(std::cerr, "Username length is limited to 8 characters");
    return EXIT_FAILURE;
  }

  std::print("Enter receiver: ");
  getline(std::cin, receiver);

  if (receiver.empty()) {
    std::println(std::cerr, "Receiver cannot be empty");
    return EXIT_FAILURE;
  }

  if (receiver.length() > 8) {
    std::println(std::cerr, "Receiver length is limited to 8 characters");
    return EXIT_FAILURE;
  }

  std::print("Enter subject: ");
  getline(std::cin, subject);

  if (subject.empty()) {
    std::println(std::cerr, "Subject cannot be empty");
    return EXIT_FAILURE;
  }

  if (subject.length() > 80) {
    std::println(std::cerr, "Subject length is limited to 80 characters");
    return EXIT_FAILURE;
  }

  std::print("Enter message: ");

  std::string line;
  while (std::getline(std::cin, line) && !line.empty()) {
    message += line;
    message.push_back('\n');

    if (line == ".") {
      break;
    }
  }

  msg.append("SEND\n");
  msg.append(sender);
  msg.push_back('\n');
  msg.append(receiver);
  msg.push_back('\n');
  msg.append(subject);
  msg.push_back('\n');
  msg.append(message);

  send_str(fd, msg);

  return EXIT_SUCCESS;
}

int handle_list_request(int client_fd) {
  std::string msg = "LIST\n";
  std::string user;

  getline(std::cin, user);

  if (user.empty()) {
    std::println(std::cerr, "Receiver cannot be empty");
    return EXIT_FAILURE;
  }

  if (user.length() > 8) {
    std::println(std::cerr, "Username length is limited to 8 characters");
    return EXIT_FAILURE;
  }

  msg.append(user);
  msg.push_back('\n');

  send_str(client_fd, msg);

  return EXIT_SUCCESS;
}

void handle_quit_request(int client_fd) {
  auto msg = std::string("quit\n");
  send_str(client_fd, msg);
  is_running = false;
}

std::vector<std::string> split_string(const std::string &str) {
  std::vector<std::string> out;
  std::istringstream iss(str);
  std::string line;

  while (std::getline(iss, line)) {
    out.push_back(std::move(line));
  }

  return out;
}

void get_response(int fd) {
  char buf[4096];

  // NOTE: Loop till you get the exit line
  ssize_t n = recv(fd, buf, sizeof(buf), 0);

  if (n == -1) {
    std::perror("recv");
    return;
  }

  if (n == 0) {
    std::println("Server closed the connection");
    is_running = false;
    return;
  }

  auto lines = split_string(std::string(buf, static_cast<size_t>(n)));

  for (auto &line : lines) {
    std::println("{}", line);
  }
}

int handle_read_request(int fd) {
  auto msg = std::string("READ\n");
  std::string user, num;

  getline(std::cin, user);

  if (user.empty()) {
    std::println(std::cerr, "Receiver cannot be empty");
    return EXIT_FAILURE;
  }

  if (user.length() > 8) {
    std::println(std::cerr, "Username length is limited to 8 characters");
    return EXIT_FAILURE;
  }

  getline(std::cin, num);

  if (num.empty()) {
    std::println(std::cerr, "Invalid input");
    return EXIT_FAILURE;
  }

  msg.append(user);
  msg.push_back('\n');
  msg.append(num);

  send_str(fd, msg);

  return EXIT_SUCCESS;
}

int handle_del_request(int fd) {
  auto msg = std::string("DEL\n");
  std::string user, num;

  getline(std::cin, user);

  if (user.empty()) {
    std::println(std::cerr, "Receiver cannot be empty");
    return EXIT_FAILURE;
  }

  if (user.length() > 8) {
    std::println(std::cerr, "Username length is limited to 8 characters");
    return EXIT_FAILURE;
  }

  getline(std::cin, num);

  if (num.empty()) {
    std::println(std::cerr, "Invalid input");
    return EXIT_FAILURE;
  }

  msg.append(user);
  msg.push_back('\n');
  msg.append(num);

  send_str(fd, msg);

  return EXIT_SUCCESS;
}

void get_command(int fd) {
  std::string line;

  while (std::getline(std::cin, line)) {
    std::transform(line.cbegin(), line.cend(), line.begin(),
                   [](char c) { return std::tolower(c); });

    switch (line.c_str()[0]) {
    case 's':
      handle_send_request(fd);
      get_response(fd);
      break;

    case 'l':
      handle_list_request(fd);
      get_response(fd);
      break;

    case 'r':
      handle_read_request(fd);
      get_response(fd);
      break;

    case 'd':
      handle_del_request(fd);
      get_response(fd);
      break;

    case 'q':
      handle_quit_request(fd);
      return;

    default:
      send_str(fd, line);
      get_response(fd);
      break;
    }

    if (std::cin.eof()) {
      is_running = false;
    }
  }
}

int main() {
  int socket_fd = socket(AF_INET, SOCK_STREAM, 0);
  sockaddr_in addr;

  memset(&addr, 0, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(8080);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (connect(socket_fd, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
    std::perror("Connect failed");
    return EXIT_FAILURE;
  }

  while (is_running) {
    get_command(socket_fd);
  }

  close(socket_fd);

  return EXIT_SUCCESS;
}
