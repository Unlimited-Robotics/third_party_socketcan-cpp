#include "socketcan_cpp/socketcan_cpp.h"
#include <signal.h>
#include <string>
#include <iostream>
#include <thread>
#include <mutex>
#include <memory>
#include <chrono>
#include <atomic>
#include <functional>

std::atomic<bool> running;

void signal_handler(int dummy)
{
  running = false;
}

void reader_fnc(std::shared_ptr<scpp::SocketCan>& socket_can, std::mutex& mtx)
{
  int32_t timeout_ms = 1000;
  int32_t status;
  scpp::CanFrame fr;
  while (running)
  {
    status = scpp::STATUS_READ_ERROR;
    {
      std::lock_guard<std::mutex> lk(mtx);
      if(socket_can)
        status = socket_can->read(fr, timeout_ms);
      else
        break;
    }
    if(status == scpp::STATUS_OK)
    {
      printf("len %d byte, id: %d, data: %02x %02x %02x %02x %02x %02x %02x %02x  \n", fr.len, fr.id,
            fr.data[0], fr.data[1], fr.data[2], fr.data[3],
            fr.data[4], fr.data[5], fr.data[6], fr.data[7]);
    }else if (status == scpp::STATUS_NOTHING_TO_READ)
    {
      printf("Timeout, nothing to read from CAN bus \n");
    }else
    {
      printf("Error id: %d\n", status);
    }
  }
}

void writer_fnc(std::shared_ptr<scpp::SocketCan>& socket_can, std::mutex& mtx)
{
  scpp::CanFrame cf_to_write;
  int32_t status;
  cf_to_write.id = 123;
  cf_to_write.len = 8;
  for (int i = 0; i < 8; ++i)
  {
    cf_to_write.data[i] = i;
  }
  while (running)
  {
    status = socket_can->write(cf_to_write);
    if (status != scpp::STATUS_OK)
    {
      printf("something went wrong on socket write, error code : %d \n", status);
      {
        // Take mutex control and reset shared pointer
        std::lock_guard<std::mutex> lk(mtx);
        socket_can.reset();
        break;
      }
    }
    else
    {
      printf("Message was written to the socket \n");
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(100));
  }
}

int main()
{
  signal(SIGINT, signal_handler);
  std::shared_ptr<scpp::SocketCan> socket_can;
  running = true;
  std::mutex socket_mtx;

  while (running.load())
  {
    socket_can = std::make_shared<scpp::SocketCan>();
    if (socket_can->open("can2") == scpp::STATUS_OK)
    {
      std::thread reader_thr{reader_fnc, std::ref(socket_can), std::ref(socket_mtx)}, writer_thr{writer_fnc, std::ref(socket_can), std::ref(socket_mtx)};
      reader_thr.join();
      writer_thr.join();
    }
    else
    {
      printf("Cannot open can socket!");
      std::this_thread::sleep_for(std::chrono::seconds(1));
    }
    socket_can.reset();
  }

  return 0;
}