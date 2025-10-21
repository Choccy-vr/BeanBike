#ifndef UART_H
#define UART_H

class UART {
public:
    void init();
    void sendMessage(String message);
    void sendData(String name, String data);
};

#endif