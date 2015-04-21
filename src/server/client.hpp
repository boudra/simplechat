#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <string>
#include <thread>
#include <queue>

#define DEFAULT_NAME "ANONIM_%d"

enum GROUP {
  USER,
  ADMINISTRATOR
};

class Client
{
public:
  Client();
  ~Client();
    
  bool Initialize();

  static void Read(Client* client);

  const std::string GetName() const;
  const std::string GetIP() const;

  inline const int GetColor() const {
    return m_color;
  }

  const GROUP GetGroup() const;
  const int GetSocket() const;
  const double GetLastMessage() const;
  const bool IsBanned() const;
  const bool IsConnected() const;
  const bool IsInitialized() const;

  std::queue<std::string>* GetMessageQueue();
  char* GetBuffer();

  void SendMsg(Client* from, const std::string message);
  void SendData(const std::string data);

  void SendRaw(const std::string data);

  void Ban();
  void Prompt();
  void Disconnect();
  void Disconnect(const std::string reason);
  void ClearBuffer();
  void SetName(std::string name);

  inline void SetColor(int color) {
    m_color = color;
  }

  void SetGroup(GROUP group);
  void SetSocket(int socket);
  void SetLastMessage(double time);

  int FloodCount;

private:

  GROUP m_group; /* user's group */
  std::string m_name; /* nickname */
  int m_color;
  double m_lastmessage; /* timestamp of the last sent message */
  bool m_banned;
  std::string m_ip;
  int m_socket; 
  bool m_connected;
  bool m_initialized;

  /* this thread will listen for client's data */
  std::thread *m_thread;
  char* m_buffer;
    
  static int color;

  /* queue of message's that the client sent,
     the server wil broadcast them in the main loop */
  std::queue<std::string> m_messages;

  std::string m_prompt;
};

inline const std::string Client::GetName() const
{
  return m_name;
}

inline const std::string Client::GetIP() const
{
  return m_ip;
}

inline const GROUP Client::GetGroup() const
{
  return m_group;
}

inline const int Client::GetSocket() const
{
  return m_socket;
}

inline const double Client::GetLastMessage() const
{
  return m_lastmessage;
}

inline const bool Client::IsBanned() const
{
  return m_banned;
}

inline const bool Client::IsInitialized() const
{
  return m_initialized;
}

inline std::queue<std::string>* Client::GetMessageQueue()
{
  return &m_messages;
}

inline char* Client::GetBuffer()
{
  return m_buffer;
}

#endif
