#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <string>

#include <semaphore.h>
#include <fcntl.h>
enum Type
{
    SPOTREBA,
    VYROBA
};

Type serverType;


void HandleClient(int socket)
{
    char buffer[256];
    int readSize;
    std::string message;
    Type clientType;
    bool type = false;
    sem_t* mutexC =sem_open("mutex",O_CREAT);
    sem_t* emptyC =sem_open("empty",O_CREAT);
    sem_t* fullC = sem_open("full",O_CREAT);


    std::string ack = "ACK";
    std::string wait = "WAIT";
    std::string nowait = "NOWAIT";

    int waiting = 0;

    int pipe;

    while((readSize = recv(socket, buffer, sizeof(buffer) - 1, 0)) > 0)
    {
        buffer[readSize] = '\0';
        std::cout << "Processing name: " << buffer << std::endl;
        if (!type)
        {
            if (strncmp(buffer, "SPOTREBA", strlen("SPOTREBA")) == 0)
            {
                if (serverType == Type::SPOTREBA)
                {
                    pipe = open("pipe", O_RDONLY);
                    clientType = Type::SPOTREBA;
                    send(socket, buffer, readSize, 0);
                    type = true;
                    continue;
                }
                else
                {
                    printf("This server accepts only SPOTREBA clients\n");
                    continue;
                }
            }
            else if (strncmp(buffer, "VYROBA", strlen("VYROBA")) == 0)
            {
                if (serverType == Type::VYROBA)
                {
                    pipe = open("pipe", O_WRONLY);
                    clientType = Type::VYROBA;
                    send(socket, buffer, readSize, 0);
                    type = true;
                    continue;
                }
                else
                {
                    printf("This server accepts only VYROBA clients\n");
                    continue;
                }
            }

        }
        else if (type && (clientType == Type::VYROBA))
        {
            if (sem_trywait(emptyC) == -1)
            {
                if (errno == EAGAIN)
                {
                    send(socket, wait.c_str(), wait.length(), 0);
                    sem_wait(emptyC);
                    printf("Waiting...\n");
                }
            }
            else
            {
                send(socket, nowait.c_str(), nowait.length(), 0);
            }
            sem_wait(mutexC);
            char size = readSize;
            write(pipe, &size, 1);
            write(pipe, buffer, size);
            send(socket, ack.c_str(), strlen(ack.c_str()), 0);
            sem_post(mutexC);
            sem_post(fullC);
        }
        else if (type && (clientType == Type::SPOTREBA))
        {
            sleep(1);
            sem_wait(fullC);
            sem_wait(mutexC);
            printf("1");
            char size;
            read(pipe, &size, 1);
            char name[size];
            read(pipe, name, size);
            printf("2");
            send(socket, name, strlen(name), 0);
            sem_post(mutexC);
            printf("3");
            sem_post(emptyC);
        }
    }
    close(pipe);
    close(pipe);
    close(socket);
}


int main() {
    sem_unlink("empty");
    sem_unlink("full");
    sem_unlink("mutex");

    sem_open("empty", O_CREAT, 0666, 20);
    sem_open("full", O_CREAT, 0666, 0);
    sem_open("mutex", O_CREAT, 0666, 1);

    char buffer[1000];

    int serverSocket = socket(AF_INET, SOCK_STREAM, 0);

    if (serverSocket == -1)
    {
        std::cout << "Failed to create socket" << std::endl;
        exit(EXIT_FAILURE);
    }

    write(STDOUT_FILENO, "Choose type of server: \n", strlen("Choose type of server: \n"));
    read(STDIN_FILENO, buffer, sizeof (buffer));

    if (strncmp("SPOTREBA", buffer, strlen("SPOTREBA")) == 0)
    {
        serverType = Type::SPOTREBA;
        std::cout << "Welcome SPOTREBA" << std::endl;
    }
    else if (strncmp("VYROBA", buffer, strlen("VYROBA")) == 0)
    {
        serverType = Type::VYROBA;
        std::cout << "Welcome VYROBA" << std::endl;
    }
    else
    {
        std::cout << "Wrong type of server" << std::endl;
        close(serverSocket);
        exit(EXIT_FAILURE);
    }

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(8050);
    serverAddress.sin_addr.s_addr = INADDR_ANY;

    int option = 1;
    if (setsockopt(serverSocket, SOL_SOCKET, SO_REUSEADDR, &option, sizeof (option)) == -1)
    {
        std::cout << "Failed to set socket options" << std::endl;
        close(serverSocket);
        exit(EXIT_FAILURE);
    }
    if (bind(serverSocket, (struct sockaddr*) &serverAddress, sizeof (serverAddress)) == -1)
    {
        std::cout << "Failed to bind socket" << std::endl;
        close(serverSocket);
        exit(EXIT_FAILURE);
    }


    listen(serverSocket, 10);
    struct sockaddr_in clientAddress;
    socklen_t clientAddressSize = sizeof (clientAddress);


    while(true)
    {
        int clientSocket = accept(serverSocket, (struct sockaddr*) &clientAddress, &clientAddressSize);

        if (clientSocket == -1)
        {
            std::cout << "Failed to accept client" << std::endl;
            close(serverSocket);
            return 0;
        }

        if (fork() == 0)
        {
            HandleClient(clientSocket);
        }
    }
    close(serverSocket);
    exit(EXIT_SUCCESS);
}
