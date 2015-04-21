#include "server.hpp"
#include "client.hpp"

#if defined(_WIN32)
#include <winsock.h>
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#endif

#include <iostream>

Server::Server() : m_port(0), m_running(false), m_socket(-1), m_listener(nullptr), m_clients()
{
  memset(m_buffer, 0, 1024);
}

Server::~Server()
{
  delete m_listener;
}

void Server::SetPortNumber(unsigned short port)
{
  m_port = port;
}

void Server::ClearBuffer()
{
  memset(m_buffer, 0, 1024);
}

bool Server::ParseCommand(Client *client, const std::string& msg)
{
  const char *string = msg.c_str();
  char c_command[255];

  if((string[0] >= 0 && string[0] <= 31) || string[0] < 0) return true;   
  if(string[0] != '/') return false;

  sscanf(string, "/%s\r\n", c_command);
  sscanf(string, "/%*s %1024[0-9a-zA-Z. ]s\r\n", m_buffer);

  if(strcmp(c_command, "exit") == 0) {

    client->Disconnect();

  } else if(strcmp(c_command, "name") == 0) {

    if(strlen(m_buffer) <= 10) {
      std::string old_name = client->GetName();
      client->SetName(m_buffer);
      m_nicks[client->GetIP()] = client->GetName();
      this->Broadcast("User " + old_name +" changed his nick to " + client->GetName() + "\n");
    } else {
      client->SendData("The name can't be longer than 10 characters\n");
    }

  } else {

    if(client->GetGroup() != GROUP::ADMINISTRATOR) {
      client->SendData("You don't have sufficient permissions.\n");
      return true;
    }

    if(strcmp(c_command, "kick") == 0) {

      bool found = false;

      for(Client& c : m_clients) {

	if(strcmp(m_buffer, c.GetName().c_str()) == 0) {

	  this->Broadcast(c.GetName() + " with IP " + c.GetIP() + " has been kicked.\n");

	  c.Disconnect("Kicked");

	  this->Broadcast(m_buffer);
	  found = true;
	  break;
	}
      }

      if(!found) {
	std::string username = std::string(m_buffer);
	client->SendData("Couldn't find the user " + username + ".\n");
      }

    }
    else if(strcmp(c_command, "ban") == 0) {

      bool found = false;

      for(Client& c : m_clients) {
	if(strcmp(m_buffer, c.GetName().c_str()) == 0) {
	  this->Broadcast(c.GetName() + " with IP " + c.GetIP() + " has been banned.\n");
	  c.Disconnect("Banned");
	  m_bans.push_back(c.GetIP());
	  found = true;
	  break;
	}
      }

      if(!found) {
	std::string username = std::string(m_buffer);
	client->SendData("Could not find the user " + username + ".\n");
      }

    }
    else if(strcmp(c_command, "unban") == 0) {

      bool found = false;

      for(auto i = m_bans.begin(); i != m_bans.end(); ++i) {

	if(strcmp(m_buffer, (*i).c_str()) == 0) {
	  found = true;
	  m_bans.erase(i);
	  break;
	}

      }

      if(!found) {
	this->ClearBuffer();
	sprintf(m_buffer, "This IP is not banned.\n");
	client->SendData(m_buffer);
      }

    }
    else if(strcmp(c_command, "shutdown") == 0) {
      this->ClearBuffer();
      client->SendData("The server is closing\n");
      m_running = false;
    } else {
      this->ClearBuffer();
      client->SendData("Unrecognized command\n");
      return true;
    }

  }

  std::cout << "[CMD] " << msg;

  this->ClearBuffer();
  return true;
}

void Server::ReadNicknames()
{
  char ip[255];

  FILE *f = fopen(NICKNAMES, "rb");

  if(!f) return;

  while(!feof(f)) {
    if(fscanf(f, "%s %1024[0-9a-zA-Z. ]s\n", ip, m_buffer) != 2)
      continue;
    m_nicks.insert(std::make_pair(std::string(ip), std::string(m_buffer)));
  }

  fclose(f);
}

void Server::WriteNicknames()
{
  FILE *f = fopen(NICKNAMES, "wb+");

  for(auto i = m_nicks.begin(); i != m_nicks.end(); ++i) {
    fprintf(f, "%s %s\n", i->first.c_str(), i->second.c_str());
  }

  fclose(f);

}

std::string Server::GetNickname(std::string ip)
{
  auto i = m_nicks.find(ip);
  if(i != m_nicks.end()) return i->second;
  else return "noname";
}

bool Server::Start() {

  try {

#if defined(_WIN32)
    WSADATA wsadata;
    if(WSAStartup(MAKEWORD(2,2), &wsadata) != 0)
      throw "WSAStartup failed.";
#endif

    if(m_port <= 0) throw "Incorrect port";

    m_socket = socket(AF_INET, SOCK_STREAM, 0);

#if defined(_WIN32)
    ioctlsocket(m_socket, FIONBIO, 0);
#endif

    struct sockaddr_in sin;
    memset( &sin, 0, sizeof sin );

    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = INADDR_ANY;
    sin.sin_port = htons(m_port);

    /* reuse the address, normally the socket stays in a TIME_WAIT state and you can't open it again */
    int yes = 1;
    setsockopt(m_socket, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

    if (bind(m_socket, (struct sockaddr*)&sin, sizeof sin) < 0)
      throw "Bind has failed.";

    if(listen(m_socket, 0) < 0)
      throw "Listen has failed.";

    m_running = true;

    m_listener = new std::thread(Server::Listen, this);

    if(m_listener == nullptr)
      throw "Couldn't create the listener thread";

  } 
  catch(std::string e) {
    std::cerr << "Couldn't listen the port number " << m_port << "\n";
    std::cerr << e << std::endl;
    return false;
  }

  std::cout << "Server listening at " << m_port << "!" << std::endl;

  this->ReadNicknames();

  while(m_running) {

    /* lock the clients list */
    std::lock_guard<std::mutex> lock(m_clients_mtx);

    for(auto i = m_clients.begin(); i != m_clients.end();) {

      Client& client = (*i);
      
      if(!client.IsInitialized()) continue;

      std::queue<std::string>* messageQueue = nullptr;

      if(!client.IsConnected()) {
      	this->Broadcast(client.GetName() + " has disconnected\n");
      	i = m_clients.erase(i);
      	continue;
      }

      messageQueue = client.GetMessageQueue();

      if(messageQueue->size() > 0) {

	while(!messageQueue->empty()) {

	  std::string str = messageQueue->front();

	  if(!this->ParseCommand(&client, str)) {
	    this->Broadcast(&client, str);
	  }

	  messageQueue->pop();

	}

      }

      ++i;

    }

  }

  this->WriteNicknames();

  for(auto it = m_clients.begin(); it != m_clients.end(); ++it) {
    m_clients.erase(it);
  }

  shutdown(m_socket, 2);

#if defined(_WIN32)
  closesocket(m_socket);
#else
  close(m_socket);
#endif

  m_listener->join();

  return true;

}

void Server::Broadcast(Client* from, std::string message)
{
  for(Client& client : m_clients) {
    client.SendMsg(from, message);
  }
}

void Server::Broadcast(std::string message)
{
  std::cout << message;

  for(Client& client : m_clients) {
    if(!client.IsConnected()) continue;
    client.SendData(message);
  }

}

void Server::Listen(Server* server)
{

  auto bans = server->GetBansList();
  std::list<Client>& clients = server->GetClientsList();

  while(server->IsRunning()) {

    struct sockaddr_in adress;
    socklen_t length = sizeof adress;

    int socket = accept(server->GetSocket(), (struct sockaddr*)&adress, &length);
    
    if(socket < 0) {
#if defined(_WIN32)
      std::cout << "Error: " << WSAGetLastError() << std::endl;
#endif
      continue;
    }

    /* we need to add a new client, lock the list */
    std::lock_guard<std::mutex> lock(server->GetClientsMutex());

    clients.emplace_back();
    Client& client = clients.back();

    client.SetSocket(socket);

    if(!client.Initialize()) {
      clients.pop_back();
      continue;
    }

    bool found = false;

    for(auto i = bans->begin(); i != bans->end(); ++i) {

      if(client.GetIP() == (*i)) {
	client.Disconnect("You are banned");
	found = true;
	clients.pop_back();
	break;
      }

    }

    if(found) continue;

    if(client.GetIP() == "127.0.0.1") {
      client.SetGroup(GROUP::ADMINISTRATOR);
    }

    /* look for a saved nickname */
    client.SetName(server->GetNickname(client.GetIP()));

    /* clear the user's screen */
    client.SendRaw("\033[2J\033[1;1H");

    /* send a welcome message */
    client.Prompt();
    client.SendData("Welcome to the server, " + client.GetName() + "!\n"); 

    server->Broadcast("User " + client.GetName() + " with IP " + client.GetIP() + " has joined.\n");

  }

}
