#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <signal.h>
#include <sys/select.h>
#include <fcntl.h>
#include <time.h>
#include <mqueue.h>
#include <errno.h>
#include <iostream>
#include <thread>


void send_mq(mqd_t mqd, const int data) {
    char* data_c;
    data_c = (char*)&data;
    // std::cout << "send: " << data << std::endl;
    mq_send(mqd, data_c, sizeof(data), 0);
    if (errno != 0)
    {
        std::cout << "send_mq errno: " << errno << std::endl;
    }
    
}

bool read_mq(mqd_t mqd, int& data) 
{
    char* data_c = (char*)&data;
    struct mq_attr attr;
    mq_getattr(mqd, &attr);

    errno = 0;
    ssize_t ret = mq_receive(mqd, data_c, attr.mq_msgsize, NULL);

    if (errno == EAGAIN)
    {
        return false;
    }

    if(errno != 0) {
        std::cout << "errno " << errno << std::endl;
        return false;
    }
    // std::cout << "ret: " << ret << std::endl;

    return true;
}

int f(int x) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return x * x;  // Приклад обчислення, ніколи не досягне
}

int g(int x) {
    // std::this_thread::sleep_for(std::chrono::seconds(10));
    return x;  // Приклад обчислення
}

void prompt_decision() {
    printf("1) continue execution\n2) stop\n3) continue execution, don't ask again\n");
}

int main() {
    int x;
    printf("Enter x: ");
    scanf("%d", &x);

    char queue_name[] = "/mqp";
    mq_unlink(queue_name);
    mqd_t mqd;
    mqd = mq_open(queue_name, O_RDWR | O_NONBLOCK | O_CREAT , S_IRUSR | S_IWUSR, NULL);
    if(errno != 0) {
        std::cout << "Failed create queue " << errno << std::endl;
    }
    // return 3;
    pid_t pid_f = fork();
    pid_t pid_g = pid_f ? fork() : 0;

    if (pid_f != 0 && pid_g != 0) {
      printf("PID of children processes: f: %d, g: %d\n", pid_f, pid_g);
        int results[2] = {1, 1}, count = 0;
        int continue_without_prompt = 0;
        time_t start_time = time(NULL);

        while (count < 2) {
            if (read_mq(mqd, results[count])) {
                printf("Got result form subprocess: %d\n", results[count]);
                if (results[count] == 0) {
                    printf("One of results is equal to 0. End result: 0\n");
                    kill(pid_f, SIGKILL);
                    kill(pid_g, SIGKILL);
                    break;
                }
                count++;
            }

            if (!continue_without_prompt && difftime(time(NULL), start_time) > 5) {
                prompt_decision();
                int choice;
                scanf("%d", &choice);
                switch(choice) {
                    case 1: break;
                    case 2: 
                        kill(pid_f, SIGKILL);
                        kill(pid_g, SIGKILL);
                        printf("Processes has been killed by user.\n");
                        return 1;
                    case 3: continue_without_prompt = 1; break;
                }
                start_time = time(NULL);  // Скидання таймера
            }
        }

        if (count == 2) {
            int final_result = results[0] && results[1];
            printf("Result f(x) && g(x) = %d\n", final_result);
        }
        
        waitpid(pid_f, NULL, 0);
        waitpid(pid_g, NULL, 0);
        mq_close(mqd);
    } else if (pid_f == 0) {
        int result = f(x);
        mqd = mq_open(queue_name, O_RDWR | O_NONBLOCK);
        if(errno != 0) {
            std::cout << "failed open queue " << errno << std::endl;
        }
        send_mq(mqd, result);
        mq_close(mqd);
        exit(EXIT_SUCCESS);
    } else if (pid_g == 0) {
        int result = g(x);
        if(errno != 0) {
            std::cout << "failed open queue " << errno << std::endl;
        }
        mqd = mq_open(queue_name, O_RDWR | O_NONBLOCK);
        send_mq(mqd, result);
        mq_close(mqd);
        exit(EXIT_SUCCESS);
    }

    return 0;
}