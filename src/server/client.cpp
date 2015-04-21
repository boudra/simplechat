#include "client.hpp" 

#if defined(_WIN32)
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#endif

#include <time.h>
#include <iostream>

int Client::color = 1;

Client::Client() : FloodCount(0), m_group(GROUP::USER), m_name("noname"), m_lastmessage(0.0), 
		   m_banned(false), m_socket(-1),  m_connected(false), m_initialized(false), m_thread(nullptr), m_buffer(nullptr)
{
  m_buffer = new char[1024];
  memset(m_buffer, 0, 1024);
  m_color = color++;
  if(color > 6) color = 1;
  m_prompt = "";
  for (int i = 0; i < 11; i++) m_prompt += "\u2500";
  m_prompt += "\u253C";
  for (int i = 0; i < 30; i++) m_prompt += "\u2500";
  m_prompt += "\n\x1B[2K%10s \u2502 ";
}

Client::~Client() {
  delete m_buffer;
}

void Client::ClearBuffer()
{
  memset(m_buffer, 0, 1024);
}

const bool Client::IsConnected() const {
  return m_connected;
}

bool Client::Initialize() {

  try {

    if(!this->IsConnected())
      throw "Initializing a disconnected client";

    /* create a new thread that will listen to data sent by this client */
    m_thread = new std::thread(Client::Read, this);

    if(m_thread == nullptr) 
      throw "Couldn't create a new thread";

    m_initialized = true;

  }
  catch (std::string e) {
    std::cerr << "Initialization of client " << m_name << " with IP " << m_ip << " has failed.\n";
    std::cerr << "Reason: " << e << std::endl;
    return false;
  }

  return true;

}

void Client::Read(Client* client)
{

  /* listening main loop */
  while(client->IsConnected()) {

    /* wait for data */
    int status = recv(client->GetSocket(), client->GetBuffer(), 1024, 0);

    if(status == 0) {
      client->Disconnect();
      break;
    }

    double lastmessage = client->GetLastMessage();
    double delta = 0.0;

    /* set time since last message */
#if defined(_WIN32)
    client->SetLastMessage(GetTickCount());
    delta = client->GetLastMessage() - lastmessage;
#else
    client->SetLastMessage(clock());
    delta = (client->GetLastMessage() - lastmessage) / CLOCKS_PER_SEC * 1000;
#endif

    size_t len = strlen(client->GetBuffer());

    /* if it's just an intro */
    if(len <= 2 && client->GetBuffer()[0] == '\r') {

      client->SendRaw("\x1B[A\x1B[2K");
      client->SendRaw("\x1B[A\x1B[2K");
      client->Prompt();

    } else {

      /* Prevent flood */
      if(delta <= 700.0) {

	client->FloodCount++;

	if(client->FloodCount >= 4) {
	  client->Disconnect("Flooding");
	  break;
	}

      } else {
	client->FloodCount = 0;
      }

      client->GetMessageQueue()->push(std::string(client->GetBuffer()));
    }

    client->ClearBuffer();

  }

}

/* Message from another client */
void Client::SendMsg(Client* from, const std::string message) {
  this->SendRaw("\x1B[A\x1B[2K");
  if(from == this) this->SendRaw("\x1B[A\x1B[2K");
  sprintf(m_buffer, "\x1B[2K\x1B[A\n\x1B[%dm%10s\x1B[0m \u2502 %s", 30 + from->GetColor(), from->GetName().c_str(), message.c_str());
  send(m_socket, m_buffer, strlen(m_buffer), 0);
  this->Prompt();
  this->ClearBuffer();
}

/* System message */
void Client::SendData(const std::string data) {
  this->SendRaw("\x1B[A\x1B[2K\x1B[A\x1B[2K\033[A\n");
  sprintf(m_buffer, "\x1B[2K\x1B[A\n\x1B[2m%10s \x1B[0m\u2502 \x1B[2m%s\x1B[0m", "System", data.c_str());
  send(m_socket, m_buffer, strlen(m_buffer), 0);
  this->ClearBuffer();
  this->Prompt();
}

void Client::SendRaw(const std::string data) {
  send(m_socket, data.c_str(), data.size(), 0);
  this->ClearBuffer();
}

void Client::Prompt() {
  sprintf(m_buffer, m_prompt.c_str(), GetName().c_str());
  send(m_socket, m_buffer, strlen(m_buffer), 0);
  this->ClearBuffer();
}

void Client::Ban() {
  m_banned = true;
}

void Client::Disconnect(const std::string reason) {
  this->SendData("Disconnected. Reason: " + reason + "\n");
  this->Disconnect();
}

void Client::Disconnect() {
  if(m_connected) {
    this->SendData("Bye\n");
    shutdown(m_socket, 2);
    m_connected = false;
    m_thread->join();
    delete m_thread;
  }
}

void Client::SetLastMessage(double time) {
  m_lastmessage = time;
}

void Client::SetName(std::string name) {
  m_name = name;
}

void Client::SetGroup(GROUP group) {
  m_group = group;
}

void Client::SetSocket(int socket) {

  if(m_connected) {
    std::cerr << "Warning: The client is already connected." << std::endl;
    this->Disconnect();
  }

  m_socket = socket;

  /* get client's IP */
  struct sockaddr_in adress;
  socklen_t length = sizeof adress;
  getpeername(m_socket, (struct sockaddr*)&adress, &length); 

  m_ip = inet_ntoa(adress.sin_addr);

  if(m_socket < 0)
    m_connected = false;
  else
    m_connected = true;

}
