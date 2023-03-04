#pragma once

#include "user_syscall.h"

// syscall wrapper templates

struct null_reply_t {};
static constexpr null_reply_t null_reply;

// send from value reference, receive nothing
// automatically fills in msglen
template<class T>
inline int SendValue(tid_t tid, const T& msg, null_reply_t) {
  auto msglen = sizeof(T); 
  return Send(tid, reinterpret_cast<const char*>(&msg), msglen, nullptr, 0);
}

// send from value reference, receive nothing
// allows user to specify msglen
template<class T>
inline int SendValue(tid_t tid, const T& msg, size_t msglen, null_reply_t) {
  return Send(tid, reinterpret_cast<const char*>(&msg), msglen, nullptr, 0);
}

// send from value reference, receive into value reference
// automatically fills in msglen and rplen
template<class T, class U>
inline int SendValue(tid_t tid, const T& msg, U& reply) {
  auto msglen = sizeof(T);
  auto rplen = sizeof(U);
  return Send(tid, reinterpret_cast<const char*>(&msg), msglen, reinterpret_cast<char*>(&reply), rplen);
}

// send from value reference, receive into value reference
// allows user to specify msglen and rplen in the same order as Send() does
template<class T, class U>
inline int SendValue(tid_t tid, const T& msg, size_t msglen, U& reply, size_t rplen = sizeof(U)) {
  return Send(tid, reinterpret_cast<const char*>(&msg), msglen, reinterpret_cast<char*>(&reply), rplen);
}

// receive into value reference
template<class T>
inline int ReceiveValue(tid_t& tid, T& msg, size_t msglen = sizeof(T)) {
  return Receive(reinterpret_cast<int*>(&tid), reinterpret_cast<char*>(&msg), msglen);
}

// reply nothing
inline int ReplyValue(tid_t tid, null_reply_t) {
  return Reply(tid, nullptr, 0);
}

// reply from value reference
template<class T>
inline int ReplyValue(tid_t tid, const T& reply, size_t rplen = sizeof(T)) {
  return Reply(tid, reinterpret_cast<const char*>(&reply), rplen);
}
