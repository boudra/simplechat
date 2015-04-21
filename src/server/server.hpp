#ifndef SERVER_HPP
#define SERVER_HPP

#include <list>
#include <thread>
#include <mutex>
#include <map>

#define NICKNAMES "nicks.txt"

class Client;

class Server
{
public:

  Server();
  ~Server();

  bool Start();
  static void Listen(Server* server);

  void SetPortNumber(unsigned short port);
  void ClearBuffer();

  std::list<Client>& GetClientsList();

  std::list<std::string>* GetBansList();

  std::string GetNickname(std::string ip);

  void Broadcast(std::string message);
  void Broadcast(Client* from, std::string message);
  bool ParseCommand(Client *client, const std::string& msg);

  void ReadNicknames();
  void WriteNicknames();

  const bool IsRunning() const;
  const int GetSocket() const;

  std::mutex& GetClientsMutex();

private:

  unsigned short m_port;
  bool m_running;

  int m_socket;

  /* this thread will listen for new connections */
  std::thread *m_listener;

  std::list<Client> m_clients;
  std::list<std::string> m_bans;

  std::map<std::string, std::string> m_nicks;

  char m_buffer[1024];
  
  std::mutex m_clients_mtx;

};

inline std::mutex& Server::GetClientsMutex()
{
  return m_clients_mtx;
}

inline std::list<Client>& Server::GetClientsList()
{
  return m_clients;
}

inline std::list<std::string>* Server::GetBansList()
{
  return &m_bans;
}

inline const bool Server::IsRunning() const
{
  return m_running;
}

inline const int Server::GetSocket() const
{
  return m_socket;
}

#endif
