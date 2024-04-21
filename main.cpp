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

    return true;
}

int f(int x) {
    std::this_thread::sleep_for(std::chrono::seconds(10));
    return x * x;
}

int g(int x) {
    return x; 
}

void print_info() {
    printf("1) continue execution\n2) stop\n");
}

void run_main_part(mqd_t mqd, pid_t f_pid, pid_t g_pid) 
{   
    printf("PID of children processes: f: %d, g: %d\n", f_pid, g_pid);

    int results[2] = {1, 1}, count = 0;
    time_t start_time = time(NULL);

    while (count < 2) {
        if (read_mq(mqd, results[count])) {
            printf("Got result form subprocess: %d\n", results[count]);
            if (results[count] == 0) {
                printf("One of results is equal to 0. End result: 0\n");
                kill(f_pid, SIGKILL);
                kill(g_pid, SIGKILL);
                break;
            }
            count++;
        }

        if ( difftime(time(NULL), start_time) > 5) {
            print_info();
            int choice;
            scanf("%d", &choice);
            switch(choice) {
                case 1: break;
                case 2: 
                    kill(f_pid, SIGKILL);
                    kill(g_pid, SIGKILL);
                    printf("Processes has been killed by user.\n");
                    return;
                default:
                    printf("Ivalid value, continuing");
            }
            start_time = time(NULL);  // Скидання таймера
        }
    }

    if (count == 2) {
        int final_result = results[0] && results[1];
        printf("Result f(x) && g(x) = %d\n", final_result);
    }

           
    waitpid(f_pid, NULL, 0);
    waitpid(g_pid, NULL, 0);
    mq_close(mqd);
            
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

    pid_t f_pid = fork();
    pid_t g_pid = f_pid ? fork() : 0;

    if (f_pid != 0 && g_pid != 0) {
      run_main_part(mqd, f_pid, g_pid);
    } else if (f_pid == 0) {
        int result = f(x);
        mqd = mq_open(queue_name, O_RDWR | O_NONBLOCK);
        if(errno != 0) {
            std::cout << "failed open queue " << errno << std::endl;
        }
        send_mq(mqd, result);
        mq_close(mqd);
        exit(EXIT_SUCCESS);
    } else if (g_pid == 0) {
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